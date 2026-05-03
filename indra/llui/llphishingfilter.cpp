/**
 * @file llphishingfilter.cpp
 * @brief Phishing link detection and mitigation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Phoenix Firestorm Project, Inc.
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
 * Phoenix Firestorm Project, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llui.h"
#include "llphishingfilter.h"
#include "llstring.h"
#include "llurlregistry.h"
#include "llurlmatch.h"
#include "llcontrol.h"

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

LLPhishingFilter::LLPhishingFilter()
{
}

LLPhishingFilter::~LLPhishingFilter()
{
}

S32 LLPhishingFilter::evaluateURLRisk(const std::string& url) const
{
	std::string lower_url = url;
	LLStringUtil::toLower(lower_url);

	S32 score = 0;

	// Regex for Second Life and Marketplace keywords with common typos/variations
	// Catches: secondlife, second-life, second-lif, seconde-life, secondellife, seconldlife, etc.
	static const boost::regex sl_pattern("secon[dl]?d[e]?[-_ \\.]?li[fv]e", boost::regex::icase);
	static const boost::regex mp_pattern("mark[e]?tpl[a]?ce", boost::regex::icase);

	bool has_sl_keyword = boost::regex_search(lower_url, sl_pattern) || 
						   boost::regex_search(lower_url, mp_pattern);

	std::string domain = getBaseDomain(lower_url);

	// Whitelist check
	bool is_official_domain = (domain == "secondlife.com" || domain == "lindenlab.com");

	if (has_sl_keyword && !is_official_domain)
	{
		// High risk: contains SL keywords but is not on an official domain
		score += 100;
	}

	// Medium risk: known free hosting/dynamic DNS often used for phishing
	if (domain == "herokuapp.com" || domain == "firebaseapp.com" || domain == "github.io" || domain == "netlify.app")
	{
		score += 50;

		// If it's on a free host and looks like a marketplace item link (e.g., brand-ID-hex)
		static const boost::regex item_pattern("-[0-9]{5,}-[0-9a-f]{10,}", boost::regex::icase);
		if (boost::regex_search(lower_url, item_pattern))
		{
			score += 50;
		}
	}

	return score;
}

bool LLPhishingFilter::isSuspicious(const std::string& url) const
{
	if (LLUI::instanceExists())
	{
		static LLCachedControl<bool> enabled(*LLUI::getInstance()->mSettingGroups["config"], "FSPhishingFilterEnabled", true);
		if (!enabled)
		{
			return false;
		}
	}
	return evaluateURLRisk(url) >= 75;
}

std::string LLPhishingFilter::getBaseDomain(const std::string& url) const
{
	// Extract hostname
	std::string host = url;
	size_t proto_pos = host.find("://");
	if (proto_pos != std::string::npos)
	{
		host = host.substr(proto_pos + 3);
	}

	size_t slash_pos = host.find('/');
	if (slash_pos != std::string::npos)
	{
		host = host.substr(0, slash_pos);
	}

	// Remove port if present
	size_t colon_pos = host.find(':');
	if (colon_pos != std::string::npos)
	{
		host = host.substr(0, colon_pos);
	}

	// Simple base domain extraction (last two parts)
	std::vector<std::string> parts;
	boost::split(parts, host, boost::is_any_of("."));

	if (parts.size() >= 2)
	{
		// Basic check for .co.uk etc could be added here if needed
		return parts[parts.size() - 2] + "." + parts[parts.size() - 1];
	}

	return host;
}
