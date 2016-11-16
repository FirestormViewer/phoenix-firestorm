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
#include "llpermissions.h"	// ExportPolicy

template<typename Type, typename Signal = boost::signals2::signal<void()> >
class SignaledType
{
public:
	SignaledType() : mValue() {}
	SignaledType(Type b) : mValue(b) {}

	boost::signals2::connection connect(const typename Signal::slot_type& slot) { return mSignal.connect(slot); }

	SignaledType& operator =(Type val)
	{
		if (val != mValue)
		{
			mValue = val;
			mSignal();
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
	void setSupportedFeatures();

	// Connection setters
	boost::signals2::connection setSupportsExportCallback(const boost::signals2::signal<void()>::slot_type& slot);
	boost::signals2::connection setSearchURLCallback(const boost::signals2::signal<void()>::slot_type& slot);
	boost::signals2::connection setSayRangeCallback(const boost::signals2::signal<void()>::slot_type& slot);
	boost::signals2::connection setShoutRangeCallback(const boost::signals2::signal<void()>::slot_type& slot);
	boost::signals2::connection setWhisperRangeCallback(const boost::signals2::signal<void()>::slot_type& slot);
	boost::signals2::connection setAvatarPickerCallback(const boost::signals2::signal<void()>::slot_type& slot);
	boost::signals2::connection setDestinationGuideCallback(const boost::signals2::signal<void()>::slot_type& slot);

	// Accessors
	bool simSupportsExport() const { return mSupportsExport; }
	std::string mapServerURL() const { return mMapServerURL; }
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

private:
	// SignaledTypes
	SignaledType<bool> mSupportsExport;
	std::string mMapServerURL;
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
};

#endif //LFSIMFEATUREHANDLER_H
