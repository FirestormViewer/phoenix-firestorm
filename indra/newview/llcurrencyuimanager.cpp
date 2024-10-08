/**
 * @file llcurrencyuimanager.cpp
 * @brief LLCurrencyUIManager class implementation
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lluictrlfactory.h"
#include "lltextbox.h"
#include "lllineeditor.h"
#include "llresmgr.h" // for LLLocale
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llversioninfo.h"

#include "llcurrencyuimanager.h"

// viewer includes
#include "llagent.h"
#include "llconfirmationmanager.h"
#include "llframetimer.h"
#include "lllineeditor.h"
#include "llviewchildren.h"
#include "llxmlrpctransaction.h"
#include "llviewernetwork.h"
#include "llpanel.h"
#include "stringize.h"


const F64 CURRENCY_ESTIMATE_FREQUENCY = 2.0;
    // how long of a pause in typing a currency buy amount before an
    // esimate is fetched from the server

class LLCurrencyUIManager::Impl
{
public:
    Impl(LLPanel& dialog);
    virtual ~Impl();

    LLPanel&        mPanel;

    bool            mHidden;

    bool            mError;
    std::string     mErrorMessage;
    std::string     mErrorURI;

    std::string     mZeroMessage;

    // user's choices
    S32             mUserCurrencyBuy;
    bool            mUserEnteredCurrencyBuy;

    // from website

    // pre-viewer 2.0, the server returned estimates as an
    // integer US cents value, e.g., "1000" for $10.00
    // post-viewer 2.0, the server may also return estimates
    // as a string with currency embedded, e.g., "10.00 Euros"
    bool            mUSDCurrencyEstimated;
    S32             mUSDCurrencyEstimatedCost;
    bool            mLocalCurrencyEstimated;
    std::string     mLocalCurrencyEstimatedCost;
    bool            mSupportsInternationalBilling;
    std::string     mSiteConfirm;

    bool            mBought;

    enum TransactionType
    {
        TransactionNone,
        TransactionCurrency,
        TransactionBuy
    };

    TransactionType      mTransactionType;
    LLXMLRPCTransaction* mTransaction;

    bool         mCurrencyChanged;
    LLFrameTimer mCurrencyKeyTimer;


    void updateCurrencyInfo();
    void finishCurrencyInfo();

    void startCurrencyBuy(const std::string& password);
    void finishCurrencyBuy();

    void clearEstimate();
    bool hasEstimate() const;
    std::string getLocalEstimate() const;

    void startTransaction(TransactionType type, const char* method, const LLSD& params);

    // return true if update needed
    bool checkTransaction();

    void setError(const std::string& message, const std::string& uri);
    void clearError();

    // return true if update needed
    bool considerUpdateCurrency();

    void currencyKey(S32);
    static void onCurrencyKey(LLLineEditor* caller, void* data);

    void prepare();
    void updateUI();
};

// is potentially not fully constructed.
LLCurrencyUIManager::Impl::Impl(LLPanel& dialog)
    : mPanel(dialog),
    mHidden(false),
    mError(false),
    mUserCurrencyBuy(2000), // note, this is a default, real value set in llfloaterbuycurrency.cpp
    mUserEnteredCurrencyBuy(false),
    mSupportsInternationalBilling(false),
    mBought(false),
    mTransactionType(TransactionNone), mTransaction(0),
    mCurrencyChanged(false)
{
    clearEstimate();
}

LLCurrencyUIManager::Impl::~Impl()
{
    delete mTransaction;
}

void LLCurrencyUIManager::Impl::updateCurrencyInfo()
{
    clearEstimate();
    mBought = false;
    mCurrencyChanged = false;

    if (mUserCurrencyBuy == 0)
    {
        mLocalCurrencyEstimated = true;
        return;
    }

    const LLVersionInfo& vi(LLVersionInfo::instance());

    LLSD params = LLSD::emptyMap();
    params["agentId"] = gAgent.getID().asString();
    params["secureSessionId"] = gAgent.getSecureSessionID().asString();
    params["language"] = LLUI::getLanguage();
    params["currencyBuy"] = mUserCurrencyBuy;
    params["viewerChannel"] = vi.getChannel();
    params["viewerMajorVersion"] = vi.getMajor();
    params["viewerMinorVersion"] = vi.getMinor();
    params["viewerPatchVersion"] = vi.getPatch();
    // With GitHub builds, the build number is too big to fit in a 32-bit int,
    // and XMLRPC value doesn't deal with integers wider than int. Use string.
    params["viewerBuildVersion"] = std::to_string(vi.getBuild());

    startTransaction(TransactionCurrency, "getCurrencyQuote", params);
}

void LLCurrencyUIManager::Impl::finishCurrencyInfo()
{
    const LLSD& result = mTransaction->response();

    bool success = result["success"].asBoolean();
    if (!success)
    {
        setError(
            result["errorMessage"].asString(),
            result["errorURI"].asString()
        );
        return;
    }

    const LLSD& currency = result["currency"];

    // old XML-RPC server: estimatedCost = value in US cents
    mUSDCurrencyEstimated = currency.has("estimatedCost");
    if (mUSDCurrencyEstimated)
    {
        mUSDCurrencyEstimatedCost = currency["estimatedCost"].asInteger();
    }

    // newer XML-RPC server: estimatedLocalCost = local currency string
    mLocalCurrencyEstimated = currency.has("estimatedLocalCost");
    if (mLocalCurrencyEstimated)
    {
        mLocalCurrencyEstimatedCost = currency["estimatedLocalCost"].asString();
        mSupportsInternationalBilling = true;
    }

    S32 newCurrencyBuy = currency["currencyBuy"].asInteger();
    if (newCurrencyBuy != mUserCurrencyBuy)
    {
        mUserCurrencyBuy = newCurrencyBuy;
        mUserEnteredCurrencyBuy = false;
    }

    mSiteConfirm = result["confirm"].asString();
}

void LLCurrencyUIManager::Impl::startCurrencyBuy(const std::string& password)
{
    const LLVersionInfo& vi(LLVersionInfo::instance());

    LLSD params = LLSD::emptyMap();
    params["agentId"] = gAgent.getID().asString();
    params["secureSessionId"] = gAgent.getSecureSessionID().asString();
    params["language"] = LLUI::getLanguage();
    params["currencyBuy"] = mUserCurrencyBuy;
    params["confirm"] = mSiteConfirm;
    params["viewerChannel"] = vi.getChannel();
    params["viewerMajorVersion"] = vi.getMajor();
    params["viewerMinorVersion"] = vi.getMinor();
    params["viewerPatchVersion"] = vi.getPatch();
    // With GitHub builds, the build number is too big to fit in a 32-bit int,
    // and XMLRPC value doesn't deal with integers wider than int. Use string.
    params["viewerBuildVersion"] = std::to_string(vi.getBuild());

    if (mUSDCurrencyEstimated)
    {
        params["estimatedCost"] = mUSDCurrencyEstimatedCost;
    }

    if (mLocalCurrencyEstimated)
    {
        params["estimatedLocalCost"] = mLocalCurrencyEstimatedCost;
    }

    if (!password.empty())
    {
        params["password"] = password;
    }

    startTransaction(TransactionBuy, "buyCurrency", params);

    clearEstimate();
    mCurrencyChanged = false;
}

void LLCurrencyUIManager::Impl::finishCurrencyBuy()
{
    const LLSD& result = mTransaction->response();

    bool success = result["success"].asBoolean();
    if (!success)
    {
        setError(
            result["errorMessage"].asString(),
            result["errorURI"].asString()
        );
    }
    else
    {
        mUserCurrencyBuy = 0;
        mUserEnteredCurrencyBuy = false;
        mBought = true;
    }
}

void LLCurrencyUIManager::Impl::startTransaction(TransactionType type,
        const char* method, const LLSD& params)
{
    // <COLOSI opensim multi-currency support>
    //static std::string transactionURI;
    //if (transactionURI.empty())
    //{
    //  transactionURI = LLGridManager::getInstance()->getHelperURI() + "currency.php";
    //}
    // Previously, transactionURI was a static local var only set once.  This needs
    // to update on HG teleport or crossing regions to a new currency module.
    // Set every time as this action should be infrequent and efficient.
    // However, we can alternatively make this a static member variable and
    // update it when the currency system changes
    std::string transactionURI;
    transactionURI = LLGridManager::getInstance()->getHelperURI() + "currency.php";
    // </COLOSI opensim multi-currency support>

    delete mTransaction;

    mTransactionType = type;
    mTransaction = new LLXMLRPCTransaction(transactionURI, method, params);

    clearError();
}

void LLCurrencyUIManager::Impl::clearEstimate()
{
    mUSDCurrencyEstimated = false;
    mUSDCurrencyEstimatedCost = 0;
    mLocalCurrencyEstimated = false;
    mLocalCurrencyEstimatedCost = "0";
}

bool LLCurrencyUIManager::Impl::hasEstimate() const
{
    return (mUSDCurrencyEstimated || mLocalCurrencyEstimated);
}

std::string LLCurrencyUIManager::Impl::getLocalEstimate() const
{
    if (mLocalCurrencyEstimated)
    {
        // we have the new-style local currency string
        return mLocalCurrencyEstimatedCost;
    }
    if (mUSDCurrencyEstimated)
    {
        // we have the old-style USD-specific value
        LLStringUtil::format_map_t args;
        {
            LLLocale locale_override(LLStringUtil::getLocale());
            args["[AMOUNT]"] = llformat("%#.2f", mUSDCurrencyEstimatedCost / 100.0);
        }
        return LLTrans::getString("LocalEstimateUSD", args);
    }
    return "";
}

bool LLCurrencyUIManager::Impl::checkTransaction()
{
    if (!mTransaction)
    {
        return false;
    }

    if (!mTransaction->process())
    {
        return false;
    }

    if (mTransaction->status(NULL) != LLXMLRPCTransaction::StatusComplete)
    {
        setError(mTransaction->statusMessage(), mTransaction->statusURI());
    }
    else
    {
        switch (mTransactionType)
        {
        case TransactionCurrency:
            finishCurrencyInfo();
            break;
        case TransactionBuy:
            finishCurrencyBuy();
            break;
        default:;
        }
    }

    delete mTransaction;
    mTransaction = NULL;
    mTransactionType = TransactionNone;

    return true;
}

void LLCurrencyUIManager::Impl::setError(
    const std::string& message, const std::string& uri)
{
    mError = true;
    mErrorMessage = message;
    mErrorURI = uri;
}

void LLCurrencyUIManager::Impl::clearError()
{
    mError = false;
    mErrorMessage.clear();
    mErrorURI.clear();
}

bool LLCurrencyUIManager::Impl::considerUpdateCurrency()
{
    if (mCurrencyChanged && !mTransaction &&
        mCurrencyKeyTimer.getElapsedTimeF32() >= CURRENCY_ESTIMATE_FREQUENCY)
    {
        updateCurrencyInfo();
        return true;
    }

    return false;
}

void LLCurrencyUIManager::Impl::currencyKey(S32 value)
{
    mUserEnteredCurrencyBuy = true;
    mCurrencyKeyTimer.reset();

    if (mUserCurrencyBuy == value)
    {
        return;
    }

    mUserCurrencyBuy = value;

    if (hasEstimate())
    {
        clearEstimate();
        //cannot just simply refresh the whole UI, as the edit field will
        // get reset and the cursor will change...

        mPanel.getChildView("currency_est")->setVisible(false);
        mPanel.getChildView("getting_data")->setVisible(true);
    }

    mCurrencyChanged = true;
}

// static
void LLCurrencyUIManager::Impl::onCurrencyKey(LLLineEditor* caller, void* data)
{
    S32 value = atoi(caller->getText().c_str());
    LLCurrencyUIManager::Impl* self = (LLCurrencyUIManager::Impl*)data;
    self->currencyKey(value);
}

void LLCurrencyUIManager::Impl::prepare()
{
    LLLineEditor* lindenAmount = mPanel.getChild<LLLineEditor>("currency_amt");
    if (lindenAmount)
    {
        lindenAmount->setPrevalidate(LLTextValidate::validateNonNegativeS32);
        lindenAmount->setKeystrokeCallback(onCurrencyKey, this);
    }
}

void LLCurrencyUIManager::Impl::updateUI()
{
    if (mHidden)
    {
        mPanel.getChildView("currency_action")->setVisible(false);
        mPanel.getChildView("currency_amt")->setVisible(false);
        mPanel.getChildView("currency_est")->setVisible(false);
        return;
    }

    mPanel.getChildView("currency_action")->setVisible(true);

    LLLineEditor* lindenAmount = mPanel.getChild<LLLineEditor>("currency_amt");
    if (lindenAmount)
    {
        lindenAmount->setVisible(true);
        lindenAmount->setLabel(mZeroMessage);

        if (!mUserEnteredCurrencyBuy)
        {
            if (mUserCurrencyBuy == 0)
            {
                lindenAmount->setText(LLStringUtil::null);
            }
            else
            {
                lindenAmount->setText(llformat("%d", mUserCurrencyBuy));
            }

            lindenAmount->selectAll();
        }
    }

    std::string estimated = (mUserCurrencyBuy == 0) ? mPanel.getString("estimated_zero") : getLocalEstimate();
    mPanel.getChild<LLUICtrl>("currency_est")->setTextArg("[LOCALAMOUNT]", estimated);
    mPanel.getChildView("currency_est")->setVisible( hasEstimate() || mUserCurrencyBuy == 0);

    mPanel.getChildView("currency_links")->setVisible( mSupportsInternationalBilling);
    mPanel.getChildView("exchange_rate_note")->setVisible( mSupportsInternationalBilling);

    if (mPanel.getChildView("buy_btn")->getEnabled()
        ||mPanel.getChildView("currency_est")->getVisible()
        || mPanel.getChildView("error_web")->getVisible())
    {
        mPanel.getChildView("getting_data")->setVisible(false);
    }
}



LLCurrencyUIManager::LLCurrencyUIManager(LLPanel& dialog)
    : impl(* new Impl(dialog))
{
}

LLCurrencyUIManager::~LLCurrencyUIManager()
{
    delete &impl;
}

void LLCurrencyUIManager::setAmount(int amount, bool noEstimate)
{
    impl.mUserCurrencyBuy = amount;
    impl.mUserEnteredCurrencyBuy = false;
    impl.updateUI();

    impl.mCurrencyChanged = !noEstimate;
}

int LLCurrencyUIManager::getAmount()
{
    return impl.mUserCurrencyBuy;
}

void LLCurrencyUIManager::setZeroMessage(const std::string& message)
{
    impl.mZeroMessage = message;
}

void LLCurrencyUIManager::setUSDEstimate(int amount)
{
    impl.mUSDCurrencyEstimatedCost = amount;
    impl.mUSDCurrencyEstimated = true;
    impl.updateUI();

    impl.mCurrencyChanged = false;
}

int LLCurrencyUIManager::getUSDEstimate()
{
    return impl.mUSDCurrencyEstimated ? impl.mUSDCurrencyEstimatedCost : 0;
}

void LLCurrencyUIManager::setLocalEstimate(const std::string &amount)
{
    impl.mLocalCurrencyEstimatedCost = amount;
    impl.mLocalCurrencyEstimated = true;
    impl.updateUI();

    impl.mCurrencyChanged = false;
}

std::string LLCurrencyUIManager::getLocalEstimate() const
{
    return impl.getLocalEstimate();
}

void LLCurrencyUIManager::prepare()
{
    impl.prepare();
}

void LLCurrencyUIManager::updateUI(bool show)
{
    impl.mHidden = !show;
    impl.updateUI();
}

bool LLCurrencyUIManager::process()
{
    bool changed = false;
    changed |= impl.checkTransaction();
    changed |= impl.considerUpdateCurrency();
    return changed;
}

void LLCurrencyUIManager::buy(const std::string& buy_msg)
{
    if (!canBuy())
    {
        return;
    }

    LLUIString msg = buy_msg;
    msg.setArg("[LINDENS]", llformat("%d", impl.mUserCurrencyBuy));
    msg.setArg("[LOCALAMOUNT]", getLocalEstimate());
    LLConfirmationManager::confirm(impl.mSiteConfirm,
                                   msg,
                                   impl,
                                   &LLCurrencyUIManager::Impl::startCurrencyBuy);
}


bool LLCurrencyUIManager::inProcess()
{
    return impl.mTransactionType != Impl::TransactionNone;
}

bool LLCurrencyUIManager::canCancel()
{
    return !buying();
}

bool LLCurrencyUIManager::canBuy()
{
    return !inProcess() && impl.hasEstimate() && impl.mUserCurrencyBuy > 0;
}

bool LLCurrencyUIManager::buying()
{
    return impl.mTransactionType == Impl::TransactionBuy;
}

bool LLCurrencyUIManager::bought()
{
    return impl.mBought;
}

void LLCurrencyUIManager::clearError()
{
    impl.clearError();
}

bool LLCurrencyUIManager::hasError()
{
    return impl.mError;
}

std::string LLCurrencyUIManager::errorMessage()
{
    return impl.mErrorMessage;
}

std::string LLCurrencyUIManager::errorURI()
{
    return impl.mErrorURI;
}



