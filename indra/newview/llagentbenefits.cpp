/**
* @file llagentbenefits.cpp
*
* $LicenseInfo:firstyear=2019&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2019, Linden Research, Inc.
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
#include "llagentbenefits.h"

// <FS:Ansariel> OpenSim legacy economy
#include "llagent.h"
#include "lleconomy.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
// </FS:Ansariel>

LLAgentBenefits::LLAgentBenefits():
	m_initalized(false),
	m_animated_object_limit(-1),
	m_animation_upload_cost(-1),
	m_attachment_limit(-1),
	m_group_membership_limit(-1),
	m_picks_limit(-1),
	m_sound_upload_cost(-1),
	m_texture_upload_cost(-1),
	m_create_group_cost(-1)
{
}

LLAgentBenefits::~LLAgentBenefits()
{
}

// This could be extended to a template scheme or otherwise modified
// to support other types, if and when needed. Currently all fields
// the viewer cares about are integer.
bool get_required_S32(const LLSD& sd, const LLSD::String& key, S32& value)
{
	value = -1;
	if (sd.has(key))
	{
		value = sd[key].asInteger();
		return true;
	}

	LL_WARNS("Benefits") << "Missing required benefit field " << key << LL_ENDL;
	return false;
}

bool LLAgentBenefits::init(const LLSD& benefits_sd)
{
	LL_DEBUGS("Benefits") << "initializing benefits from " << benefits_sd << LL_ENDL;

	if (!get_required_S32(benefits_sd, "animated_object_limit", m_animated_object_limit))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "animation_upload_cost", m_animation_upload_cost))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "attachment_limit", m_attachment_limit))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "create_group_cost", m_create_group_cost))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "group_membership_limit", m_group_membership_limit))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "picks_limit", m_picks_limit))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "sound_upload_cost", m_sound_upload_cost))
	{
		return false;
	}
	if (!get_required_S32(benefits_sd, "texture_upload_cost", m_texture_upload_cost))
	{
		return false;
	}

	// FIXME PREMIUM - either use this field or get rid of it
	m_initalized = true;
	return true;
}

S32 LLAgentBenefits::getAnimatedObjectLimit() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_animated_object_limit;
	if (LLGridManager::instance().isInSecondLife())
	{
		return m_animated_object_limit;
	}
	else
	{
		S32 max_attach = 0;
		if (gSavedSettings.getBOOL("AnimatedObjectsIgnoreLimits"))
		{
			max_attach = getAttachmentLimit();
		}
		else
		{
			if (gAgent.getRegion())
			{
				LLSD features;
				gAgent.getRegion()->getSimulatorFeatures(features);
				if (features.has("AnimatedObjects"))
				{
					max_attach = features["AnimatedObjects"]["MaxAgentAnimatedObjectAttachments"].asInteger();
				}
			}
		}
		return max_attach;
	}
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getAnimationUploadCost() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_animation_upload_cost;
	return LLGridManager::instance().isInSecondLife() ? m_animation_upload_cost : LLGlobalEconomy::instance().getPriceUpload();
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getAttachmentLimit() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_attachment_limit;
	if (LLGridManager::instance().isInSecondLife())
	{
		return m_attachment_limit;
	}
	else
	{
		const S32 MAX_AGENT_ATTACHMENTS = 38;

		S32 max_attach = MAX_AGENT_ATTACHMENTS;

		if (gAgent.getRegion())
		{
			LLSD features;
			gAgent.getRegion()->getSimulatorFeatures(features);
			if (features.has("MaxAgentAttachments"))
			{
				max_attach = features["MaxAgentAttachments"].asInteger();
			}
		}
		return max_attach;
	}
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getCreateGroupCost() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_create_group_cost;
	return LLGridManager::instance().isInSecondLife() ? m_create_group_cost : 0;
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getGroupMembershipLimit() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_group_membership_limit;
	return LLGridManager::instance().isInSecondLife() ? m_group_membership_limit : gMaxAgentGroups;
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getPicksLimit() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_picks_limit;
	return LLGridManager::instance().isInSecondLife() ? m_picks_limit : 10;
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getSoundUploadCost() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_sound_upload_cost;
	return LLGridManager::instance().isInSecondLife() ? m_sound_upload_cost : LLGlobalEconomy::instance().getPriceUpload();
	// </FS:Ansariel>
}

S32 LLAgentBenefits::getTextureUploadCost() const
{
	// <FS:Ansariel> OpenSim legacy economy
	//return m_texture_upload_cost;
	return LLGridManager::instance().isInSecondLife() ? m_texture_upload_cost : LLGlobalEconomy::instance().getPriceUpload();
	// </FS:Ansariel>
}

bool LLAgentBenefits::findUploadCost(LLAssetType::EType& asset_type, S32& cost) const
{
	bool succ = false;
	if (asset_type == LLAssetType::AT_TEXTURE)
	{
		cost = getTextureUploadCost();
		succ = true;
	}
	else if (asset_type == LLAssetType::AT_SOUND)
	{
		cost = getSoundUploadCost();
		succ = true;
	}
	else if (asset_type == LLAssetType::AT_ANIMATION)
	{
		cost = getAnimationUploadCost();
		succ = true;
	}
	return succ;
}

LLAgentBenefitsMgr::LLAgentBenefitsMgr()
{
}

LLAgentBenefitsMgr::~LLAgentBenefitsMgr()
{
}

// static
const LLAgentBenefits& LLAgentBenefitsMgr::current()
{
	return instance().mCurrent;
}

// static
const LLAgentBenefits& LLAgentBenefitsMgr::get(const std::string& package)
{
	if (instance().mPackageMap.find(package) != instance().mPackageMap.end())
	{
		return instance().mPackageMap[package];
	}
	else
	{
		return instance().mDefault;
	}
}

// static
bool LLAgentBenefitsMgr::init(const std::string& package, const LLSD& benefits_sd)
{
	LLAgentBenefits benefits;
	if (!benefits.init(benefits_sd))
	{
		LL_WARNS("Benefits") << "Unable to initialize package " << package << " from sd " << benefits_sd << LL_ENDL;
		return false;
	}
	else
	{
		instance().mPackageMap[package] = benefits;
	}
	return true;

}

// static
bool LLAgentBenefitsMgr::initCurrent(const std::string& package, const LLSD& benefits_sd)
{
	LLAgentBenefits benefits;
	if (!benefits.init(benefits_sd))
	{
		LL_WARNS("Benefits") << "Unable to initialize package " << package << " from sd " << benefits_sd << LL_ENDL;
		return false;
	}
	else
	{
		instance().mCurrent = benefits;
		instance().mCurrentName = package;
	}
	return true;

}

// static
bool LLAgentBenefitsMgr::has(const std::string& package)
{
	return instance().mPackageMap.find(package) != instance().mPackageMap.end();
}

//static
bool LLAgentBenefitsMgr::isCurrent(const std::string& package)
{
	return instance().mCurrentName == package;
}
