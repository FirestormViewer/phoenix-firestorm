/** 
 * @file lggbeamcolormapfloater.cpp
 * @brief Floater for beam colors
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */


#include "llviewerprecompiledheaders.h"

#include "lggbeamcolormapfloater.h"

#include "llagentdata.h"
#include "llcommandhandler.h"
#include "llfloater.h"
#include "llsdutil.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llfilepicker.h"
#include "llpanel.h"
#include "lliconctrl.h"
#include "llbutton.h"
#include "llcolorswatch.h"
#include "lggbeammaps.h"


#include "llsdserialize.h"
#include "fspanelprefs.h"
#include "lggbeamscolors.h"
#include "llsliderctrl.h"
#include "llfocusmgr.h"

F32 convertXToHue(S32 place)
{
	return ((place-6)/396.0f)*720.0f;
}

S32 convertHueToX(F32 place)
{
	return llround((place/720.0f)*396.0f)+6;
}

F32 hueToRgb1 ( F32 val1In, F32 val2In, F32 valHUeIn )
{
	if ( valHUeIn < 0.0f ) valHUeIn += 1.0f;
	if ( valHUeIn > 1.0f ) valHUeIn -= 1.0f;
	if ( ( 6.0f * valHUeIn ) < 1.0f ) return ( val1In + ( val2In - val1In ) * 6.0f * valHUeIn );
	if ( ( 2.0f * valHUeIn ) < 1.0f ) return ( val2In );
	if ( ( 3.0f * valHUeIn ) < 2.0f ) return ( val1In + ( val2In - val1In ) * ( ( 2.0f / 3.0f ) - valHUeIn ) * 6.0f );
	return ( val1In );
}

void hslToRgb1 ( F32 hValIn, F32 sValIn, F32 lValIn, F32& rValOut, F32& gValOut, F32& bValOut )
{
	if ( sValIn < 0.00001f )
	{
		rValOut = lValIn;
		gValOut = lValIn;
		bValOut = lValIn;
	}
	else
	{
		F32 interVal1;
		F32 interVal2;

		if ( lValIn < 0.5f )
			interVal2 = lValIn * ( 1.0f + sValIn );
		else
			interVal2 = ( lValIn + sValIn ) - ( sValIn * lValIn );

		interVal1 = 2.0f * lValIn - interVal2;

		rValOut = hueToRgb1 ( interVal1, interVal2, hValIn + ( 1.f / 3.f ) );
		gValOut = hueToRgb1 ( interVal1, interVal2, hValIn );
		bValOut = hueToRgb1 ( interVal1, interVal2, hValIn - ( 1.f / 3.f ) );
	}
}


const F32 CONTEXT_CONE_IN_ALPHA = 0.0f;
const F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
const F32 CONTEXT_FADE_TIME = 0.08f;

// Correction factors needed after porting from Phoenix
const S32 CORRECTION_X = 0;
const S32 CORRECTION_Y = -40;

void lggBeamColorMapFloater::onClickSlider(LLUICtrl* crtl, void* userdata)
{
	lggBeamColorMapFloater* self = (lggBeamColorMapFloater*)userdata;
	self->fixOrder();
}

void lggBeamColorMapFloater::draw()
{
	//set the color of the preview thing
	LLColorSwatchCtrl* colorctrl = getChild<LLColorSwatchCtrl>("BeamColor_Preview");
	LLColor4 bColor = LLColor4(lggBeamMaps::beamColorFromData(myData));
	colorctrl->set(bColor, TRUE);
	
	//Try draw rectangle attach beam
	LLRect swatch_rect;
	LLButton* createButton = fspanel->getChild<LLButton>("BeamColor_new");
	
	createButton->localRectToOtherView(createButton->getLocalRect(), &swatch_rect, this);
	LLRect local_rect = getLocalRect();
	if (gFocusMgr.childHasKeyboardFocus(this) && fspanel->isInVisibleChain() && mContextConeOpacity > 0.001f)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLEnable(GL_CULL_FACE);
		gGL.begin(LLRender::QUADS);
		{
			F32 r = bColor.mV[0];
			F32 g = bColor.mV[1];
			F32 b = bColor.mV[2];

			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mRight, local_rect.mTop);
			gGL.vertex2i(local_rect.mLeft, local_rect.mTop);

			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
			gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mBottom);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);

			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
			gGL.vertex2i(local_rect.mRight, local_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mTop);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mBottom);

			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
			gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mBottom);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mBottom);
		}
		gGL.end();
	}

	F32 opacity = gSavedSettings.getF32("PickerContextOpacity");
	mContextConeOpacity = lerp(mContextConeOpacity, opacity, LLCriticalDamp::getInterpolant(CONTEXT_FADE_TIME));

	//Draw Base Stuff
	LLFloater::draw();
	
	//Draw hues and pointers at color
	gGL.pushMatrix();
	F32 r, g, b;
	LLColor4 output;
	for (int i  = 0;i <= 720;i++)
	{
		int hi =i%360;
		hslToRgb1((hi/360.0f), 1.0f, 0.5f, r, g, b);
		output.set(r, g, b);

		gl_line_2d(
			convertHueToX(i) + CORRECTION_X, 201 + CORRECTION_Y,
			convertHueToX(i) + CORRECTION_X, 277 + CORRECTION_Y, output);
			//convertHueToX(i),161,
			//convertHueToX(i),237,output);

	}
	S32 X1 = convertHueToX(myData.startHue) + CORRECTION_X;
	S32 X2 = convertHueToX(myData.endHue) + CORRECTION_X;
	LLFontGL* font = LLFontGL::getFontSansSerifSmall();

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X1 + CORRECTION_X, 238 + CORRECTION_Y,9.0f, (S32)30, false);

	gGL.color4fv(LLColor4::black.mV);
	gl_circle_2d(X1 + CORRECTION_X, 238 + CORRECTION_Y,8.0f, (S32)30, false);

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X1 + CORRECTION_X, 238 + CORRECTION_Y,7.0f, (S32)30, false);

	gl_line_2d(X1+1 + CORRECTION_X, 210 + CORRECTION_Y, X1+1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1-1 + CORRECTION_X, 210 + CORRECTION_Y, X1-1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1 + CORRECTION_X, 210 + CORRECTION_Y, X1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::black);

	gl_line_2d(X1-25 + CORRECTION_X, 238+1 + CORRECTION_Y, X1+25 + CORRECTION_X, 238+1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1-25 + CORRECTION_X, 238-1 + CORRECTION_Y, X1+25 + CORRECTION_X, 238-1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1-25 + CORRECTION_X, 238 + CORRECTION_Y, X1+25 + CORRECTION_X, 238 + CORRECTION_Y, LLColor4::black);

	font->renderUTF8(
		"Start Hue", 0,
		X1 + CORRECTION_X, 
		212 + CORRECTION_Y,
		LLColor4::white, LLFontGL::HCENTER,
		LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);


	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X2 + CORRECTION_X, 238 + CORRECTION_Y, 9.0f, (S32)30, false);

	gGL.color4fv(LLColor4::black.mV);
	gl_circle_2d(X2 + CORRECTION_X, 238 + CORRECTION_Y, 8.0f, (S32)30, false);

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X2 + CORRECTION_X, 238 + CORRECTION_Y, 7.0f, (S32)30, false);

	gl_line_2d(X2+1 + CORRECTION_X, 210 + CORRECTION_Y, X2+1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2-1 + CORRECTION_X, 210 + CORRECTION_Y, X2-1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2 + CORRECTION_X ,210 + CORRECTION_Y, X2 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::black);

	gl_line_2d(X2-25 + CORRECTION_X, 238+1 + CORRECTION_Y, X2+25 + CORRECTION_X, 238+1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2-25 + CORRECTION_X, 238-1 + CORRECTION_Y, X2+25 + CORRECTION_X, 238-1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2-25 + CORRECTION_X, 238 + CORRECTION_Y, X2+25 + CORRECTION_X, 238 + CORRECTION_Y, LLColor4::black);

	font->renderUTF8(
		"End Hue", 0,
		X2 + CORRECTION_X, 
		212 + CORRECTION_Y,
		LLColor4::white, LLFontGL::HCENTER,
		LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);
	
	gGL.popMatrix();
	
}

lggBeamColorMapFloater::~lggBeamColorMapFloater()
{
}

lggBeamColorMapFloater::lggBeamColorMapFloater(const LLSD& seed): LLFloater(seed),
	mContextConeOpacity(0.0f)
{
}

BOOL lggBeamColorMapFloater::postBuild(void)
{
	setCanMinimize(false);

	getChild<LLUICtrl>("BeamColor_Save")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickSave, this));
	getChild<LLUICtrl>("BeamColor_Load")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickLoad, this));
	getChild<LLUICtrl>("BeamColor_Cancel")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickCancel, this));


	mColorSlider = getChild<LLSliderCtrl>("BeamColor_Speed");
	childSetCommitCallback("BeamColor_Speed", onClickSlider, this);
	
	// Is this still needed???
	//mColorSlider->setCallbackUserData(this);

	fixOrder();
	
	return true;
}
BOOL lggBeamColorMapFloater::handleMouseDown(S32 x,S32 y,MASK mask)
{
	//6, 277
		//402 201
	
	if (y > (201 + CORRECTION_Y) &&  y < (277 + CORRECTION_Y))
	{
		if (x < (6 + CORRECTION_X))
		{
			myData.startHue=0.0f;
		}
		else if (x > (402 + CORRECTION_X))
		{
			myData.endHue=720.0f;
		}
		else
		{
			myData.startHue  = convertXToHue(x + CORRECTION_X);
		}
	
		fixOrder();
	}
	
	llinfos << "we got clicked at (" << x << ", " << y << " yay! " << llendl;
	
	return LLFloater::handleMouseDown(x,y,mask);
}

BOOL lggBeamColorMapFloater::handleRightMouseDown(S32 x,S32 y,MASK mask)
{
	if (y > (201 + CORRECTION_Y) &&  y < (277 + CORRECTION_Y))
	{
		if (x < (6 + CORRECTION_X))
		{
			myData.startHue=0.0f;
		}
		else if (x > (402 + CORRECTION_X))
		{
			myData.endHue=720.0f;
		}
		else
		{
			myData.endHue  = convertXToHue(x + CORRECTION_X);
		}

		fixOrder();
	}
	llinfos << "we got right clicked at (" << x << ", " << y << " yay! " << llendl;

	return LLFloater::handleRightMouseDown(x,y,mask);
}

void lggBeamColorMapFloater::fixOrder()
{
	myData.rotateSpeed = mColorSlider->getValueF32();
	myData.rotateSpeed /= 100.0f;
	
	if(myData.endHue < myData.startHue)
	{
		F32 temp = myData.startHue;
		myData.startHue = myData.endHue;
		myData.endHue = temp;
	}
}


void lggBeamColorMapFloater::setData(void* data)
{
	fspanel = (PanelPreferenceFirestorm*)data;
	if (fspanel)
	{
		gFloaterView->getParentFloater(fspanel)->addDependentFloater(this);
	}
}

void lggBeamColorMapFloater::update()
{
	
}

LLSD lggBeamColorMapFloater::getMyDataSerialized()
{
 	
 	return myData.toLLSD();
}

void lggBeamColorMapFloater::onClickSave()
{
	LLFilePicker& picker = LLFilePicker::instance();
	
	std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beamsColors", ""));
				
	std::string filename=path_name2 + "myNewBeamColor.xml";
	if(!picker.getSaveFile( LLFilePicker::FFSAVE_BEAM, filename ) )
	{
		return;
	}	
	
	filename = path_name2 + gDirUtilp->getBaseFileName(picker.getFirstFile());
	
	LLSD main = getMyDataSerialized();
  
	llofstream export_file;
	export_file.open(filename);
	LLSDSerialize::toPrettyXML(main, export_file);
	export_file.close();

	gSavedSettings.setString("FSBeamColorFile", gDirUtilp->getBaseFileName(filename,true));

	if (fspanel != NULL)
	{
		fspanel->refreshBeamLists();
	}
	closeFloater();
}

void lggBeamColorMapFloater::onClickCancel()
{
	closeFloater();	
}

void lggBeamColorMapFloater::onClickLoad()
{
	LLFilePicker& picker = LLFilePicker::instance();
	if(!picker.getOpenFile(LLFilePicker::FFLOAD_XML))
	{
		return; 
	}
	LLSD minedata;
	llifstream importer(picker.getFirstFile());
	LLSDSerialize::fromXMLDocument(minedata, importer);
	
	myData = lggBeamsColors::fromLLSD(minedata);
	childSetValue("BeamColor_Speed",/*self->*/myData.rotateSpeed*100);
}

