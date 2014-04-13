/** 
 * @file llleapmotioncontroller.cpp
 * @brief LLLeapMotionController class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2013, Cinder Roxley <cinder.roxley@phoenixviewer.com>
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

#include "llleapmotioncontroller.h"

#ifdef USE_LEAPMOTION

#include "llagent.h"
#include "llagentcamera.h"
#include "llgesturemgr.h"
#include "llmath.h"
#include "llstartup.h"
#include "lltimer.h"
#include "llviewercontrol.h"
#include "fsnearbychathub.h"
#include "fsleaptool.h"

#include <leap-motion/Leap.h>

// <FS:Zi> Leap Motion flycam
#include "llmoveview.h"
#include "llquaternion.h"
#include "llviewercamera.h"
#include "llviewerjoystick.h"
// </FS:Zi>

const F32	LM_DEAD_ZONE			= 20.f;		// Dead zone in the middle of the space
const F32   LM_ORBIT_RATE_FACTOR	= 80.f;		// Number for camera orbit magic factor

class 	LLLMImpl : public Leap::Listener
{
public:
	LLLMImpl();
	~LLLMImpl();

	// LeapMotion callbacks
	virtual void onInit(const Leap::Controller&);
    virtual void onConnect(const Leap::Controller&);
    virtual void onDisconnect(const Leap::Controller&);
    virtual void onFrame(const Leap::Controller&);

	// Called from viewer main loop
	void	stepFrame();

	Leap::Controller * mLMController;		// Leapmotion's object
	bool			mLMConnected;			// true if device is connected
	bool			mFrameAvailable;		// true if there is a new frame of data available
	int64_t			mCurrentFrameID;		// Id of the most recent frame of data

	LLTimer			mYawTimer;				// Avoid turning left / right too fast

	// Hacky demo code - send controller data to in-world objects via chat
	LLTimer			mChatMsgTimer;			// Throttle sending LM controller data to region local chat

	LLTimer			mGestureTimer;			// Throttle invoking SL gestures

	std::string getDebugString();
	void render();

private:
	nd::leap::Tool *mTool;

	// Various controller modes
	void	modeFlyingControlTest(Leap::HandList & hands);
	void	modeStreamDataToSL(Leap::HandList & hands);
	void	modeGestureDetection1(Leap::HandList & hands);
	void	modeMoveAndCamTest1(Leap::HandList & hands);
	void	modeDumpDebugInfo(Leap::HandList & hands);

// <FS:Zi> Leap Motion flycam
public:
	// returns true when in leapmotion flycam mode
	bool 	getOverrideCamera();
	// called once per frame when in flycam mode
	void	moveFlycam();

private:
	Leap::Vector	mHandCenterPos;			// point where the hand entered flycam

	void	modeFlycam(Leap::HandList & hands);

	bool			mOverrideCamera;		// true when in leapmotion flycam mode

	LLVector3		mFlycamDelta;			// next movement delta for flycam
	LLVector3		mFlycamFeatheredDelta;	// feathered positional data
	LLVector3		mFlycamPos;				// current flycam position
	LLQuaternion	mFlycamInitialRot;		// initial flycam rotation
	LLVector3		mFlycamRot;				// current flycam rotation as vector
	LLVector3	 	mFlycamFeatheredRot;	// current feathered rotation as vector
// </FS:Zi>
};

const F32		LLLEAP_YAW_INTERVAL = 0.075f;

// Time between spamming chat messages.  Server-side throttle is 200 msgs in 10 seconds
const F32		LLLEAP_CHAT_MSG_INTERVAL = 0.200f;		// Send 5/second

const F32		LLLEAP_GESTURE_INTERVAL = 3.f;		// 3 seconds in between SL gestures


LLLMImpl::LLLMImpl() : mLMController(NULL)
, mLMConnected(false)
, mFrameAvailable(false)
, mCurrentFrameID(0)
, mOverrideCamera(false)	// <FS:Zi> Leap Motion flycam
, mTool(0)
{
	mLMController = new Leap::Controller(*this);
	mYawTimer.setTimerExpirySec(LLLEAP_YAW_INTERVAL);
	mChatMsgTimer.setTimerExpirySec(LLLEAP_CHAT_MSG_INTERVAL);
	mGestureTimer.setTimerExpirySec(LLLEAP_GESTURE_INTERVAL);
}

LLLMImpl::~LLLMImpl()
{
	delete mLMController;
	delete mTool;
}

void LLLMImpl::onInit(const Leap::Controller& controller) 
{
	LL_INFOS("LeapMotion") << "Initialized" << LL_ENDL;
}

void LLLMImpl::onConnect(const Leap::Controller& controller) 
{
	LL_INFOS("LeapMotion") << "Connected" << LL_ENDL;
	mLMConnected = true;
	mCurrentFrameID = 0;
}

void LLLMImpl::onDisconnect(const Leap::Controller& controller) 
{
	LL_INFOS("LeapMotion") << "Disconnected" << LL_ENDL;
	mLMConnected = false;
}

// Callback from Leapmotion code when a new frame is available.  It simply
// sets a flag so stepFrame() can pick up new controller data
void LLLMImpl::onFrame(const Leap::Controller& controller) 
{
	if (mLMConnected)
	{
		// Get the most recent frame and report some basic information
		const Leap::Frame frame = controller.frame();
		int64_t frame_id = frame.id();
		if (frame_id != mCurrentFrameID)
		{	// Just record the ID and set flag indicating data is available
			mCurrentFrameID = frame_id;
			mFrameAvailable = true;
		}

		if( mTool )
			mTool->onLeapFrame( frame);
	}
}

std::string LLLMImpl::getDebugString()
{
	if( mTool )
		return mTool->getDebugString();

	return "";
}

void LLLMImpl::render()
{
	if( mTool )
		return mTool->render();
}

// This is called every SL viewer frame
void LLLMImpl::stepFrame()
{
	if (mLMController
		&& mFrameAvailable
		&& mLMConnected)
	{
		mFrameAvailable = false;

		// Get the most recent frame and report some basic information
		const Leap::Frame frame = mLMController->frame();
		Leap::HandList hands = frame.hands();
		
		static LLCachedControl<S32> sControllerMode(gSavedSettings, "LeapMotionTestMode", 0);

		if( !mTool || mTool->getId() != sControllerMode )
		{
			delete mTool;
			mTool = nd::leap::constructTool( sControllerMode );
		}

		if( mTool )
			mTool->onRenderFrame( frame );
		else
		{
			switch (sControllerMode)
			{
				case 1:
					modeFlyingControlTest(hands);
					break;
				case 2:
					modeStreamDataToSL(hands);
					break;
				case 3:
					modeGestureDetection1(hands);
					break;
				case 4:
					modeMoveAndCamTest1(hands);
					break;
				// <FS:Zi> Leap Motion flycam
				case 10:
					modeFlycam(hands);
					break;
				// </FS:Zi>
				case 411:
					modeDumpDebugInfo(hands);
					break;
				default:
					// Do nothing
					break;
			}
		}
	}
}

// This controller mode is used to fly the avatar, going up, down, forward and turning.
void LLLMImpl::modeFlyingControlTest(Leap::HandList & hands)
{
	static S32 sLMFlyingHysteresis = 0;

	S32 numHands = hands.count();		
	BOOL agent_is_flying = gAgent.getFlying();

	if (numHands == 0
		&& agent_is_flying
		&& sLMFlyingHysteresis > 0)
	{
		sLMFlyingHysteresis--;
		if (sLMFlyingHysteresis == 0)
		{
			LL_INFOS("LeapMotion") << "LM stop flying - look ma, no hands!" << LL_ENDL;
			gAgent.setFlying(FALSE);
		}
	}
	else if (numHands == 1)
	{
		// Get the first hand
		Leap::Hand hand = hands[0];

		// Check if the hand has any fingers
		Leap::FingerList finger_list = hand.fingers();
		S32 num_fingers = finger_list.count();

		Leap::Vector palm_pos = hand.palmPosition();
		Leap::Vector palm_normal = hand.palmNormal();

		F32 ball_radius = (F32) hand.sphereRadius();
		Leap::Vector ball_center = hand.sphereCenter();

		// Number of fingers controls flying on / off
		if (num_fingers == 0 &&			// To do - add hysteresis or data smoothing?
			agent_is_flying)
		{
			if (sLMFlyingHysteresis > 0)
			{
				sLMFlyingHysteresis--;
			}
			else
			{
				LL_INFOS("LeapMotion") << "LM stop flying" << LL_ENDL;
				gAgent.setFlying(FALSE);
			}
		}
		else if (num_fingers > 2 && 
				!agent_is_flying)
		{
			LL_INFOS("LeapMotion") << "LM start flying" << LL_ENDL;
			gAgent.setFlying(TRUE);
			sLMFlyingHysteresis = 5;
		}

		// Radius of ball controls forward motion
		if (agent_is_flying)
		{

			if (ball_radius > 110.f)
			{	// Open hand, move fast
				gAgent.setControlFlags(AGENT_CONTROL_AT_POS | AGENT_CONTROL_FAST_AT);
			}
			else if (ball_radius > 85.f)
			{	// Partially open, move slow
				gAgent.setControlFlags(AGENT_CONTROL_AT_POS);
			}
			else
			{	// Closed - stop
				gAgent.clearControlFlags(AGENT_CONTROL_AT_POS);
			}

			// Height of palm controls moving up and down
			if (palm_pos.y > 260.f)
			{	// Go up fast
				gAgent.setControlFlags(AGENT_CONTROL_UP_POS | AGENT_CONTROL_FAST_UP);
			}
			else if (palm_pos.y > 200.f)
			{	// Go up
				gAgent.setControlFlags(AGENT_CONTROL_UP_POS);
			}
			else if (palm_pos.y < 60.f)
			{	// Go down fast
				gAgent.setControlFlags(AGENT_CONTROL_FAST_UP | AGENT_CONTROL_UP_NEG);
			}
			else if (palm_pos.y < 120.f)
			{	// Go down
				gAgent.setControlFlags(AGENT_CONTROL_UP_NEG);
			}
			else
			{	// Clear up / down
				gAgent.clearControlFlags(AGENT_CONTROL_FAST_UP | AGENT_CONTROL_UP_POS | AGENT_CONTROL_UP_NEG);
			}

			// Palm normal going left / right controls direction
			if (mYawTimer.checkExpirationAndReset(LLLEAP_YAW_INTERVAL))
			{
				if (palm_normal.x > 0.4f)
				{	// Go left fast
					gAgent.moveYaw(1.f);
				}
				else if (palm_normal.x < -0.4f)
				{	// Go right fast
					gAgent.moveYaw(-1.f);
				}
			}

		}		// end flying controls
	}
}


// This experimental mode sends chat messages into SL on a back channel for LSL scripts
// to intercept with a listen() event.   This is experimental and not sustainable for
// a production feature ... many avatars using this would flood the chat system and
// hurt server performance.   Depending on how useful this proves to be, a better
// mechanism should be designed to stream data from the viewer into SL scripts.
void LLLMImpl::modeStreamDataToSL(Leap::HandList & hands)
{
	S32 numHands = hands.count();
	if (numHands == 1 &&
		mChatMsgTimer.checkExpirationAndReset(LLLEAP_CHAT_MSG_INTERVAL))
	{
		// Get the first (and only) hand
		Leap::Hand hand = hands[0];

		Leap::Vector palm_pos = hand.palmPosition();
		Leap::Vector palm_normal = hand.palmNormal();

		F32 ball_radius = (F32) hand.sphereRadius();
		Leap::Vector ball_center = hand.sphereCenter();

		// Chat message looks like "/2343 LM1,<palm pos>,<palm normal>,<sphere center>,<sphere radius>"
		LLVector3 vec;
		std::stringstream status_chat_msg;
		status_chat_msg << "/2343 LM,";
		status_chat_msg << "<" << palm_pos.x << "," << palm_pos.y << "," << palm_pos.z << ">,";
		status_chat_msg << "<" << palm_normal.x << "," << palm_normal.y << "," << palm_normal.z << ">,";
		status_chat_msg << "<" << ball_center.x << "," << ball_center.y << "," << ball_center.z << ">," << ball_radius;

		FSNearbyChat::instance().sendChatFromViewer(status_chat_msg.str(), CHAT_TYPE_SHOUT, FALSE);
	}
}

// This mode tries to detect simple hand motion and either triggers an avatar gesture or 
// sends a chat message into SL in response.   It is very rough, hard-coded for detecting 
// a hand wave (a SL gesture) or the wiggling-thumb gun trigger (a chat message sent to a
// special version of the popgun).
void LLLMImpl::modeGestureDetection1(Leap::HandList & hands)
{
	static S32 trigger_direction = -1;

	S32 numHands = hands.count();
	if (numHands == 1)
	{
		// Get the first hand
		Leap::Hand hand = hands[0];

		// Check if the hand has any fingers
		Leap::FingerList finger_list = hand.fingers();
		S32 num_fingers = finger_list.count();
		static S32 last_num_fingers = 0;

		if (num_fingers == 1)
		{	// One finger ... possibly reset the 
			Leap::Finger finger = finger_list[0];
			Leap::Vector finger_dir = finger.direction();

			// Negative Z is into the screen - check that it's the largest component
			S32 abs_z_dir = llabs(finger_dir.z);
			if (finger_dir.z < -0.5 &&
				abs_z_dir > llabs(finger_dir.x) &&
				abs_z_dir > llabs(finger_dir.y))
			{
				Leap::Vector finger_pos = finger.tipPosition();
				Leap::Vector finger_vel = finger.tipVelocity(); 
				LL_INFOS("LeapMotion") << "finger direction is " << finger_dir.x << ", " << finger_dir.y << ", " << finger_dir.z
					<< ", position " << finger_pos.x << ", " << finger_pos.y << ", " << finger_pos.z 
					<< ", velocity " << finger_vel.x << ", " << finger_vel.y << ", " << finger_vel.z 
					<< LL_ENDL;
			}

			if (trigger_direction != -1)
			{
				LL_INFOS("LeapMotion") << "Reset trigger_direction - one finger" << LL_ENDL;
				trigger_direction = -1;
			}
		}
		else if (num_fingers == 2)
		{
			Leap::Finger barrel_finger = finger_list[0];
			Leap::Vector barrel_finger_dir = barrel_finger.direction();

			// Negative Z is into the screen - check that it's the largest component
			F32 abs_z_dir = llabs(barrel_finger_dir.z);
			if (barrel_finger_dir.z < -0.5f &&
				abs_z_dir > llabs(barrel_finger_dir.x) &&
				abs_z_dir > llabs(barrel_finger_dir.y))
			{
				Leap::Finger thumb_finger = finger_list[1];
				Leap::Vector thumb_finger_dir = thumb_finger.direction();
				Leap::Vector thumb_finger_pos = thumb_finger.tipPosition();
				Leap::Vector thumb_finger_vel = thumb_finger.tipVelocity();

				if ((thumb_finger_dir.x < barrel_finger_dir.x) )
				{	// Trigger gunfire
					if (trigger_direction < 0 &&		// Haven't fired
						thumb_finger_vel.x > 50.f &&	// Moving into screen
						thumb_finger_vel.z < -50.f &&
						mChatMsgTimer.checkExpirationAndReset(LLLEAP_CHAT_MSG_INTERVAL))
					{
						// Chat message looks like "/2343 LM2 gunfire"
						std::string gesture_chat_msg("/2343 LM2 gunfire");
						//LLNearbyChatBar::sendChatFromViewer(gesture_chat_msg, CHAT_TYPE_SHOUT, FALSE);
						trigger_direction = 1;
						LL_INFOS("LeapMotion") << "Sent gunfire chat" << LL_ENDL;
					}
					else if (trigger_direction > 0 &&	// Have fired, need to pull thumb back
						thumb_finger_vel.x < -50.f &&
						thumb_finger_vel.z > 50.f)		// Moving out of screen
					{
						trigger_direction = -1;
						LL_INFOS("LeapMotion") << "Reset trigger_direction" << LL_ENDL;
					}
				}
			}
			else if (trigger_direction != -1)
			{
				LL_INFOS("LeapMotion") << "Reset trigger_direction - hand pos" << LL_ENDL;
				trigger_direction = -1;
			}
		}
		else if (num_fingers == 5 &&
			num_fingers == last_num_fingers)
		{
			if (mGestureTimer.checkExpirationAndReset(LLLEAP_GESTURE_INTERVAL))
			{
				// figure out a gesture to trigger
				std::string gestureString("/overhere");
				LLGestureMgr::instance().triggerAndReviseString( gestureString );
			}
		}
		
		last_num_fingers = num_fingers;
	}
}


// This mode tries to move the avatar and camera in Second Life.   It's pretty rough and needs a lot of work
void LLLMImpl::modeMoveAndCamTest1(Leap::HandList & hands)
{
	S32 numHands = hands.count();		
	if (numHands == 1)
	{
		// Get the first hand
		Leap::Hand hand = hands[0];

		// Check if the hand has any fingers
		Leap::FingerList finger_list = hand.fingers();
		S32 num_fingers = finger_list.count();

		F32 orbit_rate = 0.f;

		Leap::Vector pos(0, 0, 0);
		for (size_t i = 0; i < num_fingers; ++i) 
		{
			Leap::Finger finger = finger_list[i];
			pos += finger.tipPosition();
		}
		pos = Leap::Vector(pos.x/num_fingers, pos.y/num_fingers, pos.z/num_fingers);

		if (num_fingers == 1)
		{	// 1 finger - move avatar
			if (pos.x < -LM_DEAD_ZONE)
			{	// Move left
				gAgent.moveLeftNudge(1.f);
			}
			else if (pos.x > LM_DEAD_ZONE)
			{
				gAgent.moveLeftNudge(-1.f);
			}
			
			/*
			if (pos.z < -LM_DEAD_ZONE)
			{
				gAgent.moveAtNudge(1.f);
			}
			else if (pos.z > LM_DEAD_ZONE)
			{	
				gAgent.moveAtNudge(-1.f);
			} */

			if (pos.y < -LM_DEAD_ZONE)
			{
				gAgent.moveYaw(-1.f);
			}
			else if (pos.y > LM_DEAD_ZONE)
			{
				gAgent.moveYaw(1.f);
			}
		}	// end 1 finger
		else if (num_fingers == 2)
		{	// 2 fingers - move camera around
			// X values run from about -170 to +170
			if (pos.x < -LM_DEAD_ZONE)
			{	// Camera rotate left
				gAgentCamera.unlockView();
				orbit_rate = (llabs(pos.x) - LM_DEAD_ZONE) / LM_ORBIT_RATE_FACTOR;
				gAgentCamera.setOrbitLeftKey(orbit_rate);
			}
			else if (pos.x > LM_DEAD_ZONE)
			{
				gAgentCamera.unlockView();
				orbit_rate = (pos.x - LM_DEAD_ZONE) / LM_ORBIT_RATE_FACTOR;
				gAgentCamera.setOrbitRightKey(orbit_rate);
			}
			if (pos.z < -LM_DEAD_ZONE)
			{	// Camera zoom in
				gAgentCamera.unlockView();
				orbit_rate = (llabs(pos.z) - LM_DEAD_ZONE) / LM_ORBIT_RATE_FACTOR;
				gAgentCamera.setOrbitInKey(orbit_rate);
			}
			else if (pos.z > LM_DEAD_ZONE)
			{	// Camera zoom out
				gAgentCamera.unlockView();
				orbit_rate = (pos.z - LM_DEAD_ZONE) / LM_ORBIT_RATE_FACTOR;
				gAgentCamera.setOrbitOutKey(orbit_rate);
			}

			if (pos.y < -LM_DEAD_ZONE)
			{	// Camera zoom in
				gAgentCamera.unlockView();
				orbit_rate = (llabs(pos.y) - LM_DEAD_ZONE) / LM_ORBIT_RATE_FACTOR;
				gAgentCamera.setOrbitUpKey(orbit_rate);
			}
			else if (pos.y > LM_DEAD_ZONE)
			{	// Camera zoom out
				gAgentCamera.unlockView();
				orbit_rate = (pos.y - LM_DEAD_ZONE) / LM_ORBIT_RATE_FACTOR;
				gAgentCamera.setOrbitDownKey(orbit_rate);
			}
		}	// end 2 finger
	}
}


// This controller mode just dumps out a bunch of the Leap Motion device data, which can then be
// analyzed for other use.
void LLLMImpl::modeDumpDebugInfo(Leap::HandList & hands)
{
	S32 numHands = hands.count();		
	if (numHands == 1)
	{
		// Get the first hand
		Leap::Hand hand = hands[0];

		// Check if the hand has any fingers
		Leap::FingerList finger_list = hand.fingers();
		S32 num_fingers = finger_list.count();

		if (num_fingers >= 1) 
		{	// Calculate the hand's average finger tip position
			Leap::Vector pos(0, 0, 0);
			Leap::Vector direction(0, 0, 0);
			for (size_t i = 0; i < num_fingers; ++i) 
			{
				Leap::Finger finger = finger_list[i];
				pos += finger.tipPosition();
				direction += finger.direction();

				// Lots of log spam
				LL_INFOS("LeapMotion") << "Finger " << i << " string is " << finger.toString() << LL_ENDL;
			}
			pos = Leap::Vector(pos.x/num_fingers, pos.y/num_fingers, pos.z/num_fingers);
			direction = Leap::Vector(direction.x/num_fingers, direction.y/num_fingers, direction.z/num_fingers);

			LL_INFOS("LeapMotion") << "Hand has " << num_fingers << " fingers with average tip position"
				<< " (" << pos.x << ", " << pos.y << ", " << pos.z << ")" 
				<< " direction (" << direction.x << ", " << direction.y << ", " << direction.z << ")" 
				<< LL_ENDL;

		}

		Leap::Vector palm_pos = hand.palmPosition();
		Leap::Vector palm_normal = hand.palmNormal();
		LL_INFOS("LeapMotion") << "Palm pos " << palm_pos.x
			<< ", " <<  palm_pos.y
			<< ", " <<  palm_pos.z
			<< ".   Normal: " << palm_normal.x
			<< ", " << palm_normal.y
			<< ", " << palm_normal.z
			<< LL_ENDL;

		F32 ball_radius = (F32) hand.sphereRadius();
		Leap::Vector ball_center = hand.sphereCenter();
		LL_INFOS("LeapMotion") << "Ball pos " << ball_center.x
			<< ", " << ball_center.y
			<< ", " << ball_center.z
			<< ", radius " << ball_radius
			<< LL_ENDL;
	}	// dump_out_data
}


// --------------------------------------------------------------------------------
// The LLLeapMotionController class is a thin public glue layer into the LLLMImpl
// class, which does all the interesting work.

// One controller instance to rule them all
LLLeapMotionController::LLLeapMotionController()
{
	mController = new LLLMImpl();
}


LLLeapMotionController::~LLLeapMotionController()
{
	cleanup();
}

void LLLeapMotionController::cleanup()
{
	delete mController;
	mController = NULL;
}

// Called every viewer frame
void LLLeapMotionController::stepFrame()
{
	if (mController &&
		STATE_STARTED == LLStartUp::getStartupState())
	{
		mController->stepFrame();
	}
}

// <FS:Zi> Leap Motion flycam
bool LLLMImpl::getOverrideCamera()
{
	return mOverrideCamera;
}

// This controller mode is used to fly around with the camera
void LLLMImpl::modeFlycam(Leap::HandList& hands)
{
	static bool old_joystick_enabled=false;

	S32 numHands=hands.count();
	if(numHands!=1)
	{
		if(mOverrideCamera)
		{
			mOverrideCamera=false;

			gSavedSettings.setBOOL("JoystickEnabled",old_joystick_enabled);
			gSavedSettings.setBOOL("JoystickFlycamEnabled",false);

			LLPanelStandStopFlying::clearStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);

			// make sure to keep the camera where we left it when we switch off flycam
			LLViewerJoystick::instance().setCameraNeedsUpdate(false);
		}
	}
	else
	{
		// Get the first hand
		Leap::Hand hand=hands[0];

		// Check if the hand has at least 3 fingers
		Leap::FingerList finger_list=hand.fingers();
		S32 num_fingers=finger_list.count();

		static F32 y_rot=0.0f;
		static F32 z_rot=0.0f;

		if(!mOverrideCamera)
		{
			mOverrideCamera=true;

			old_joystick_enabled=gSavedSettings.getBOOL("JoystickEnabled");

			gSavedSettings.setBOOL("JoystickEnabled",true);
			gSavedSettings.setBOOL("JoystickFlycamEnabled",true);

			mFlycamPos=LLViewerCamera::instance().getOrigin();
			mFlycamDelta=LLVector3::zero;
			mFlycamFeatheredDelta=LLVector3::zero;
			mFlycamInitialRot=LLViewerCamera::instance().getQuaternion();
			mFlycamRot=LLVector3::zero;
			mFlycamFeatheredRot=LLVector3::zero;
			y_rot=0.0f;
			z_rot=0.0f;

			LLPanelStandStopFlying::setStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);
		}

		Leap::Vector palm_pos=hand.palmPosition();
		Leap::Vector palm_normal=hand.palmNormal();

		if(num_fingers<3)
		{
			mFlycamDelta=LLVector3::zero;
			mHandCenterPos=hand.palmPosition();

			// auto leveling (not quite perfect yet)
			mFlycamRot=LLVector3(0.0f,y_rot,z_rot);
		}
		else
		{
			F32 delta=palm_pos.z-mHandCenterPos.z;
			if(fabsf(delta)>5.0f)
			{
				mFlycamDelta.mV[VX]=-delta*fabsf(delta)/1000.0f;
			}

			delta=palm_pos.y-mHandCenterPos.y;
			if(fabsf(delta)>5.0f)
			{
				mFlycamDelta.mV[VZ]=delta*fabsf(delta)/1000.0f;
			}

			delta=palm_pos.x-mHandCenterPos.x;
			if(fabsf(delta)>5.0f)
			{
				mFlycamDelta.mV[VY]=-delta*fabsf(delta)/1000.0f;
			}

			y_rot=palm_normal.z*150.0f;
			if(fabsf(palm_normal.x)>0.2f)
			{
				z_rot+=(palm_normal.x*fabsf(palm_normal.x)*50.0f);
			}

			// palm_normal.z = pitch
			// palm_normal.x = roll
			mFlycamRot=LLVector3(-palm_normal.x*60.0f,y_rot,z_rot);
		}
	}
}

void LLLMImpl::moveFlycam()
{
	if(!mOverrideCamera)
	{
		return;
	}

	// simple feathering of the positional data
	mFlycamFeatheredDelta+=(mFlycamDelta-mFlycamFeatheredDelta)/10.0f;

	// simple feathering of the rotational data
	mFlycamFeatheredRot+=(mFlycamRot-mFlycamFeatheredRot)/10.0f;

	LLQuaternion final_flycam_rot=mayaQ(
		mFlycamFeatheredRot.mV[VX],
		mFlycamFeatheredRot.mV[VY],
		mFlycamFeatheredRot.mV[VZ],
		LLQuaternion::XYZ
	);

	final_flycam_rot=final_flycam_rot*mayaQ(
		0.0f,
		0.0f,
		mFlycamFeatheredRot.mV[VZ],
		LLQuaternion::XYZ
	);

	final_flycam_rot=final_flycam_rot*mFlycamInitialRot;

	mFlycamPos+=mFlycamFeatheredDelta*final_flycam_rot;

	LLViewerCamera::instance().setView(LLViewerCamera::instance().getView());
	LLViewerCamera::instance().setOrigin(mFlycamPos);

	LLMatrix3 mat(final_flycam_rot);
	LLViewerCamera::instance().mXAxis=LLVector3(mat.mMatrix[0]);
	LLViewerCamera::instance().mYAxis=LLVector3(mat.mMatrix[1]);
	LLViewerCamera::instance().mZAxis=LLVector3(mat.mMatrix[2]);
}

bool LLLeapMotionController::getOverrideCamera()
{
	return mController->getOverrideCamera();
}

void LLLeapMotionController::moveFlycam()
{
	if(mController->getOverrideCamera())
	{
		mController->moveFlycam();
	}
}
// </FS:Zi>

std::string LLLeapMotionController::getDebugString()
{
	if( !mController || STATE_STARTED != LLStartUp::getStartupState())
		return "";

	return mController->getDebugString();
}

void LLLeapMotionController::render()
{
	if( !mController || STATE_STARTED != LLStartUp::getStartupState())
		return;

	return mController->render();
}
#endif
