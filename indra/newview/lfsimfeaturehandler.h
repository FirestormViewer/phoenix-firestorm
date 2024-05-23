/* Copyright (C) 2013 Liru Færs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#ifndef LFSIMFEATUREHANDLER_H
#define LFSIMFEATUREHANDLER_H

#include "llsingleton.h"
#include "llpermissions.h"  // ExportPolicy

template<typename Type, typename Signal = boost::signals2::signal<void(Type)> >
class SignaledType
{
public:
    SignaledType() : mValue() {}
    SignaledType(Type b) : mValue(b) {}

    typedef typename Signal::slot_type changed_signal_t;
    boost::signals2::connection connect(const changed_signal_t& slot) { return mSignal.connect(slot); }

    SignaledType& operator =(Type val)
    {
        if (val != mValue)
        {
            mValue = val;
            mSignal(mValue);
        }
        return *this;
    }
    operator Type() const { return mValue; }

private:
    Signal mSignal;
    Type mValue;
};

typedef enum e_export_support
{
    EXPORT_UNDEFINED = 0,
    EXPORT_ALLOWED,
    EXPORT_DENIED
} ExportSupport;

class LFSimFeatureHandler : public LLSingleton<LFSimFeatureHandler>
{
    LOG_CLASS(LFSimFeatureHandler);

    LLSINGLETON(LFSimFeatureHandler);

public:
    void handleRegionChange();
    void onSimulatorFeaturesReceived(const LLUUID &region_id);
    void setSupportedFeatures();
    // <COLOSI opensim multi-currency support>
    // public helper to manage all the manual updates in one place callable from other systems like llstartup.
    // Should be called whenever the sActiveCurrency in Tea is changed, but can't simply be called from Tea
    // because that is an llcommon include and really shouldn't call out to newview system.
    // updating the currency in Tea updates any dynamic LLUIStrings automatically when a param is replaced on the next get call,
    // but static LLUIStrings for floaters/panels/etc pre-loaded are only evaluated once, so these need to be manually updated
    // after we update the currency symbol in Tea.  This function handles that.  As we discover more UI components which we've
    // missed, they can be added to this function.  For now, it primaily handles the status panel (top bar) and the buy currency floater.
    // If there is a better location for this function, we could move it.
    static void updateCurrencySymbols();
    // </COLOSI opensim multi-currency support>

    // Connection setters
    boost::signals2::connection setSupportsExportCallback(const SignaledType<bool>::changed_signal_t& slot);
    boost::signals2::connection setSearchURLCallback(const SignaledType<std::string>::changed_signal_t& slot);
    boost::signals2::connection setSayRangeCallback(const SignaledType<U32>::changed_signal_t& slot);
    boost::signals2::connection setShoutRangeCallback(const SignaledType<U32>::changed_signal_t& slot);
    boost::signals2::connection setWhisperRangeCallback(const SignaledType<U32>::changed_signal_t& slot);
    boost::signals2::connection setAvatarPickerCallback(const SignaledType<std::string>::changed_signal_t& slot);
    boost::signals2::connection setDestinationGuideCallback(const SignaledType<std::string>::changed_signal_t& slot);

    // Accessors
    bool simSupportsExport() const { return mSupportsExport; }
    std::string mapServerURL() const { return mMapServerURL; }
    std::string gridStatusURL() const { return mGridStatusURL; }
    std::string gridStatusRSS() const { return mGridStatusRSS; }
    std::string hyperGridURL() const { return mHyperGridPrefix; }
    std::string searchURL() const { return mSearchURL; }
    U32 sayRange() const { return mSayRange; }
    U32 shoutRange() const { return mShoutRange; }
    U32 whisperRange() const { return mWhisperRange; }
    ExportSupport exportPolicy() const;
    std::string avatarPickerURL() const { return mAvatarPickerURL; }
    std::string destinationGuideURL() const { return mDestinationGuideURL; }

    F32 simulatorFPS() const { return mSimulatorFPS; }
    F32 simulatorFPSFactor() const { return mSimulatorFPSFactor; }
    F32 simulatorFPSWarn() const { return mSimulatorFPSWarn; }
    F32 simulatorFPSCrit() const { return mSimulatorFPSCrit; }

    bool hasAvatarPicker() const { return mHasAvatarPicker; }
    bool hasDestinationGuide() const { return mHasDestinationGuide; }

    // <COLOSI opensim multi-currency support>
    std::string helperUriOverride() const { return mHelperUriOverride; }
    std::string currencySymbolOverride() const { return mCurrencySymbolOverride; }
    // </COLOSI opensim multi-currency support>

private:
    // SignaledTypes
    SignaledType<bool> mSupportsExport;
    std::string mMapServerURL;
    std::string mGridStatusURL;
    std::string mGridStatusRSS;
    std::string mHyperGridPrefix;
    SignaledType<std::string> mSearchURL;
    SignaledType<U32> mSayRange;
    SignaledType<U32> mShoutRange;
    SignaledType<U32> mWhisperRange;
    SignaledType<std::string> mAvatarPickerURL;
    SignaledType<std::string> mDestinationGuideURL;

    F32 mSimulatorFPS;
    F32 mSimulatorFPSFactor;
    F32 mSimulatorFPSWarn;
    F32 mSimulatorFPSCrit;

    bool mHasAvatarPicker;
    bool mHasDestinationGuide;

    // <COLOSI opensim multi-currency support>
    std::string mHelperUriOverride;
    std::string mCurrencySymbolOverride;
    // </COLOSI opensim multi-currency support>>
};

#endif //LFSIMFEATUREHANDLER_H
