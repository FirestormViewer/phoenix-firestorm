/** 
 * @file llfloatersettingsdebug.cpp
 * @brief floater for debugging internal viewer settings
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llfloatersettingsdebug.h"

//#include "llfirstuse.h"
#include "llbutton.cpp"
#include "llcolorswatch.h"
#include "llfloater.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llsearcheditor.h"
#include "llspinctrl.h"
#include "llstring.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a)
#include "rlvhandler.h"
#include "rlvextensions.h"
#include "stdenums.h"		// for ADD_BOTTOM
// [/RLVa:KB]


LLFloaterSettingsDebug::LLFloaterSettingsDebug(const LLSD& key) 
:	LLFloater(key)
{
	mCommitCallbackRegistrar.add("SettingSelect",	boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this));
	mCommitCallbackRegistrar.add("CommitSettings",	boost::bind(&LLFloaterSettingsDebug::onCommitSettings, this));
	mCommitCallbackRegistrar.add("ClickDefault",	boost::bind(&LLFloaterSettingsDebug::onClickDefault, this));
	mCommitCallbackRegistrar.add("UpdateFilter",	boost::bind(&LLFloaterSettingsDebug::onUpdateFilter, this));
}

LLFloaterSettingsDebug::~LLFloaterSettingsDebug()
{}

void LLFloaterSettingsDebug::onUpdateFilter()
{
	mSettingsScrollList->deleteAllItems();

	std::string searchTerm=mSearchSettingsInput->getValue();
	
	settings_map_t::iterator it;
	for(it=mSettingsMap.begin();it!=mSettingsMap.end();it++)
	{
		BOOL addItem=FALSE;

		if(searchTerm.empty())
		{
			addItem=TRUE;
		}
		else
		{
			std::string itemValue=it->second->getName();
			std::string itemComment=it->second->getComment();

			LLStringUtil::toLower(searchTerm);
			LLStringUtil::toLower(itemValue);

			if(itemValue.find(searchTerm,0)!=std::string::npos)
			{
				addItem=TRUE;
			}
			else	// performance: broken out to save toLower calls on comments
			{
				LLStringUtil::toLower(itemComment);
				if(itemComment.find(searchTerm,0)!=std::string::npos)
					addItem=TRUE;
			}
		}

		if(addItem)
		{
			LLSD item;
			item["columns"][0]["value"]=it->second->getName();
			mSettingsScrollList->addElement(item,ADD_BOTTOM,it->second);
		}
	}
	onSettingSelect();
}

BOOL LLFloaterSettingsDebug::postBuild()
{
	mSearchSettingsInput = getChild<LLSearchEditor>("search_settings_input");
	mSettingsScrollList = getChild<LLScrollListCtrl>("settings_scroll_list");
	mComment = getChild<LLTextEditor>("comment_text");
	mSpinner1 = getChild<LLSpinCtrl>("val_spinner_1");
	mSpinner2 = getChild<LLSpinCtrl>("val_spinner_2");
	mSpinner3 = getChild<LLSpinCtrl>("val_spinner_3");
	mSpinner4 = getChild<LLSpinCtrl>("val_spinner_4");
	mColorSwatch = getChild<LLColorSwatchCtrl>("val_color_swatch");
	mValText = getChild<LLLineEditor>("val_text");
	mBooleanCombo = getChild<LLRadioGroup>("boolean_combo");
	mDefaultButton = getChild<LLButton>("default_btn");

	// tried to make this an XUI callback, but keystroke_callback doesn't
	// seem to work as hoped, so build the callback manually :/ -Zi
	mSearchSettingsInput->setKeystrokeCallback(boost::bind(&LLFloaterSettingsDebug::onUpdateFilter,this));

	struct f : public LLControlGroup::ApplyFunctor
	{
		settings_map_t* map;
		f(settings_map_t* m) : map(m) {}
		virtual void apply(const std::string& name, LLControlVariable* control)
		{
			if (!control->isHiddenFromSettingsEditor())
			{
				(*map)[name]=control;
			}
		}
	} func(&mSettingsMap);

	std::string key = getKey().asString();
	if (key == "all" || key == "base")
	{
		gSavedSettings.applyToAll(&func);
	}
	if (key == "all" || key == "account")
	{
		gSavedPerAccountSettings.applyToAll(&func);
	}

	onUpdateFilter();
	mSettingsScrollList->sortByColumnIndex(0,TRUE);
	return TRUE;
}

LLControlVariable* LLFloaterSettingsDebug::getControlVariable()
{
	LLControlVariable* controlp=NULL;

	LLScrollListItem* item=mSettingsScrollList->getFirstSelected();
	if(item)
		controlp=(LLControlVariable*) item->getUserdata();

	return controlp;
}

void LLFloaterSettingsDebug::onSettingSelect()
{
	updateControl(getControlVariable());
}

void LLFloaterSettingsDebug::onCommitSettings()
{
	LLControlVariable* controlp=getControlVariable();
	if (!controlp)
		return;

	LLVector3 vector;
	LLVector3d vectord;
	LLRect rect;
	LLColor4 col4;
	LLColor3 col3;
	LLColor4U col4U;
	LLColor4 color_with_alpha;

	switch(controlp->type())
	{		
	  case TYPE_U32:
		controlp->set(mSpinner1->getValue());
		break;
	  case TYPE_S32:
		controlp->set(mSpinner1->getValue());
		break;
	  case TYPE_F32:
		controlp->set(LLSD(mSpinner1->getValue().asReal()));
		break;
	  case TYPE_BOOLEAN:
		controlp->set(mBooleanCombo->getValue());
		break;
	  case TYPE_STRING:
		controlp->set(LLSD(mValText->getValue().asString()));
		break;
	  case TYPE_VEC3:
		vector.mV[VX] = (F32) mSpinner1->getValue().asReal();
		vector.mV[VY] = (F32) mSpinner2->getValue().asReal();
		vector.mV[VZ] = (F32) mSpinner3->getValue().asReal();
		controlp->set(vector.getValue());
		break;
	  case TYPE_VEC3D:
		vectord.mdV[VX] = mSpinner1->getValue().asReal();
		vectord.mdV[VY] = mSpinner2->getValue().asReal();
		vectord.mdV[VZ] = mSpinner3->getValue().asReal();
		controlp->set(vectord.getValue());
		break;
	  case TYPE_RECT:
		rect.mLeft = mSpinner1->getValue().asInteger();
		rect.mRight = mSpinner2->getValue().asInteger();
		rect.mBottom = mSpinner3->getValue().asInteger();
		rect.mTop = mSpinner4->getValue().asInteger();
		controlp->set(rect.getValue());
		break;
	  case TYPE_COL4:
		col3.setValue(mColorSwatch->getValue());
		col4 = LLColor4(col3, (F32) mSpinner4->getValue().asReal());
		controlp->set(col4.getValue());
		break;
	  case TYPE_COL3:
		controlp->set(mColorSwatch->getValue());
		//col3.mV[VRED] = (F32)floaterp->getChild<LLUICtrl>("val_spinner_1")->getValue().asC();
		//col3.mV[VGREEN] = (F32)floaterp->getChild<LLUICtrl>("val_spinner_2")->getValue().asReal();
		//col3.mV[VBLUE] = (F32)floaterp->getChild<LLUICtrl>("val_spinner_3")->getValue().asReal();
		//controlp->set(col3.getValue());
		break;
	  default:
		break;
	}
}

void LLFloaterSettingsDebug::onClickDefault()
{
	LLControlVariable* controlp=getControlVariable();

	if (controlp)
	{
		controlp->resetToDefault(true);
		updateControl(controlp);
	}
}

// we've switched controls, so update spinners, etc.
void LLFloaterSettingsDebug::updateControl(LLControlVariable* controlp)
{
	if (!mSpinner1 || !mSpinner2 || !mSpinner3 || !mSpinner4 || !mColorSwatch)
	{
		llwarns << "Could not find all desired controls by name"
			<< llendl;
		return;
	}

	mSpinner1->setVisible(FALSE);
	mSpinner2->setVisible(FALSE);
	mSpinner3->setVisible(FALSE);
	mSpinner4->setVisible(FALSE);
	mColorSwatch->setVisible(FALSE);
	mValText->setVisible( FALSE);
	mComment->setText(LLStringUtil::null);
	mDefaultButton->setEnabled(FALSE);
	mBooleanCombo->setVisible(FALSE);

	if (controlp)
	{
// [RLVa:KB] - Checked: 2011-05-28 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
		// If "HideFromEditor" was toggled while the floater is open then we need to manually disable access to the control
		// NOTE: this is no longer once per frame, so it might need special handling for RLVa -Zi
		bool fEnable = !controlp->isHiddenFromSettingsEditor();
		mSpinner1->setEnabled(fEnable);
		mSpinner2->setEnabled(fEnable);
		mSpinner3->setEnabled(fEnable);
		mSpinner4->setEnabled(fEnable);
		mColorSwatch->setEnabled(fEnable);
		mValText->setEnabled(fEnable);
		mBooleanCombo->setEnabled(fEnable);
		mDefaultButton->setEnabled(fEnable);
// [/RLVa:KB]

		eControlType type = controlp->type();

		mComment->setText(controlp->getComment());
		mSpinner1->setMaxValue(F32_MAX);
		mSpinner2->setMaxValue(F32_MAX);
		mSpinner3->setMaxValue(F32_MAX);
		mSpinner4->setMaxValue(F32_MAX);
		mSpinner1->setMinValue(-F32_MAX);
		mSpinner2->setMinValue(-F32_MAX);
		mSpinner3->setMinValue(-F32_MAX);
		mSpinner4->setMinValue(-F32_MAX);
		if (!mSpinner1->hasFocus())
		{
			mSpinner1->setIncrement(0.1f);
		}
		if (!mSpinner2->hasFocus())
		{
			mSpinner2->setIncrement(0.1f);
		}
		if (!mSpinner3->hasFocus())
		{
			mSpinner3->setIncrement(0.1f);
		}
		if (!mSpinner4->hasFocus())
		{
			mSpinner4->setIncrement(0.1f);
		}

		LLSD sd = controlp->get();
		switch(type)
		{
		  case TYPE_U32:
			mSpinner1->setVisible(TRUE);
			mSpinner1->setLabel(std::string("value")); // Debug, don't translate
			if (!mSpinner1->hasFocus())
			{
				mSpinner1->setValue(sd);
				mSpinner1->setMinValue((F32)U32_MIN);
				mSpinner1->setMaxValue((F32)U32_MAX);
				mSpinner1->setIncrement(1.f);
				mSpinner1->setPrecision(0);
			}
			break;
		  case TYPE_S32:
			mSpinner1->setVisible(TRUE);
			mSpinner1->setLabel(std::string("value")); // Debug, don't translate
			if (!mSpinner1->hasFocus())
			{
				mSpinner1->setValue(sd);
				mSpinner1->setMinValue((F32)S32_MIN);
				mSpinner1->setMaxValue((F32)S32_MAX);
				mSpinner1->setIncrement(1.f);
				mSpinner1->setPrecision(0);
			}
			break;
		  case TYPE_F32:
			mSpinner1->setVisible(TRUE);
			mSpinner1->setLabel(std::string("value")); // Debug, don't translate
			if (!mSpinner1->hasFocus())
			{
				mSpinner1->setPrecision(3);
				mSpinner1->setValue(sd);
			}
			break;
		  case TYPE_BOOLEAN:
			mBooleanCombo->setVisible(TRUE);
			if (!mBooleanCombo->hasFocus())
			{
				if (sd.asBoolean())
				{
					mBooleanCombo->setValue(LLSD("true"));
				}
				else
				{
					mBooleanCombo->setValue(LLSD(""));
				}
			}
			break;
		  case TYPE_STRING:
			mValText->setVisible( TRUE);
			if (!mValText->hasFocus())
			{
				mValText->setValue(sd);
			}
			break;
		  case TYPE_VEC3:
		  {
			LLVector3 v;
			v.setValue(sd);
			mSpinner1->setVisible(TRUE);
			mSpinner1->setLabel(std::string("X"));
			mSpinner2->setVisible(TRUE);
			mSpinner2->setLabel(std::string("Y"));
			mSpinner3->setVisible(TRUE);
			mSpinner3->setLabel(std::string("Z"));
			if (!mSpinner1->hasFocus())
			{
				mSpinner1->setPrecision(3);
				mSpinner1->setValue(v[VX]);
			}
			if (!mSpinner2->hasFocus())
			{
				mSpinner2->setPrecision(3);
				mSpinner2->setValue(v[VY]);
			}
			if (!mSpinner3->hasFocus())
			{
				mSpinner3->setPrecision(3);
				mSpinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_VEC3D:
		  {
			LLVector3d v;
			v.setValue(sd);
			mSpinner1->setVisible(TRUE);
			mSpinner1->setLabel(std::string("X"));
			mSpinner2->setVisible(TRUE);
			mSpinner2->setLabel(std::string("Y"));
			mSpinner3->setVisible(TRUE);
			mSpinner3->setLabel(std::string("Z"));
			if (!mSpinner1->hasFocus())
			{
				mSpinner1->setPrecision(3);
				mSpinner1->setValue(v[VX]);
			}
			if (!mSpinner2->hasFocus())
			{
				mSpinner2->setPrecision(3);
				mSpinner2->setValue(v[VY]);
			}
			if (!mSpinner3->hasFocus())
			{
				mSpinner3->setPrecision(3);
				mSpinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_RECT:
		  {
			LLRect r;
			r.setValue(sd);
			mSpinner1->setVisible(TRUE);
			mSpinner1->setLabel(std::string("Left"));
			mSpinner2->setVisible(TRUE);
			mSpinner2->setLabel(std::string("Right"));
			mSpinner3->setVisible(TRUE);
			mSpinner3->setLabel(std::string("Bottom"));
			mSpinner4->setVisible(TRUE);
			mSpinner4->setLabel(std::string("Top"));
			if (!mSpinner1->hasFocus())
			{
				mSpinner1->setPrecision(0);
				mSpinner1->setValue(r.mLeft);
			}
			if (!mSpinner2->hasFocus())
			{
				mSpinner2->setPrecision(0);
				mSpinner2->setValue(r.mRight);
			}
			if (!mSpinner3->hasFocus())
			{
				mSpinner3->setPrecision(0);
				mSpinner3->setValue(r.mBottom);
			}
			if (!mSpinner4->hasFocus())
			{
				mSpinner4->setPrecision(0);
				mSpinner4->setValue(r.mTop);
			}

			mSpinner1->setMinValue((F32)S32_MIN);
			mSpinner1->setMaxValue((F32)S32_MAX);
			mSpinner1->setIncrement(1.f);

			mSpinner2->setMinValue((F32)S32_MIN);
			mSpinner2->setMaxValue((F32)S32_MAX);
			mSpinner2->setIncrement(1.f);

			mSpinner3->setMinValue((F32)S32_MIN);
			mSpinner3->setMaxValue((F32)S32_MAX);
			mSpinner3->setIncrement(1.f);

			mSpinner4->setMinValue((F32)S32_MIN);
			mSpinner4->setMaxValue((F32)S32_MAX);
			mSpinner4->setIncrement(1.f);
			break;
		  }
		  case TYPE_COL4:
		  {
			LLColor4 clr;
			clr.setValue(sd);
			mColorSwatch->setVisible(TRUE);
			// only set if changed so color picker doesn't update
			if(clr != LLColor4(mColorSwatch->getValue()))
			{
				mColorSwatch->set(LLColor4(sd), TRUE, FALSE);
			}
			mSpinner4->setVisible(TRUE);
			mSpinner4->setLabel(std::string("Alpha"));
			if (!mSpinner4->hasFocus())
			{
				mSpinner4->setPrecision(3);
				mSpinner4->setMinValue(0.0);
				mSpinner4->setMaxValue(1.f);
				mSpinner4->setValue(clr.mV[VALPHA]);
			}
			break;
		  }
		  case TYPE_COL3:
		  {
			LLColor3 clr;
			clr.setValue(sd);
			mColorSwatch->setVisible(TRUE);
			mColorSwatch->setValue(sd);
			break;
		  }
		  default:
			mComment->setText(std::string("unknown"));
			break;
		}
	}
}
