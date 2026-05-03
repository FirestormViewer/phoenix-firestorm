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

#include "lluicontrolfactory.h"
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

	// Official keywords
	bool has_sl_keyword = (lower_url.find("secondlife") != std::string::npos || 
						   lower_url.find("marketplace") != std::string::npos);

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
	}

	return score;
}

bool LLPhishingFilter::isSuspicious(const std::string& url) const
{
	return evaluateURLRisk(url) >= 75;
}

bool LLPhishingFilter::processMessage(const std::string& message, std::string& filtered_message) const
{
	static LLCachedControl<bool> enabled(std::string("FSPhishingFilterEnabled"), true);
	if (!enabled)
	{
		filtered_message = message;
		return false;
	}

	// We need to find all URLs in the message.
	// LLUrlRegistry can help with this.
	std::vector<LLUrlMatch> matches;
	LLUrlRegistry::instance().findUrl(LLWString(utf8str_to_wstring(message)), matches);

	bool modified = false;
	std::string result = message;

	// Process matches in reverse to maintain offsets
	for (std::vector<LLUrlMatch>::reverse_iterator it = matches.rbegin(); it != matches.rend(); ++it)
	{
		std::string url = wstring_to_utf8str(it->getUrl());
		if (isSuspicious(url))
		{
			modified = true;
			// Mangle the URL to make it non-clickable
			std::string mangled_url = url;
			boost::replace_all(mangled_url, "http://", "h-ttp://");
			boost::replace_all(mangled_url, "https://", "h-ttps://");
			boost::replace_all(mangled_url, ".", "[dot]");
		}
	}

	if (modified)
	{
		LLWString w_message = utf8str_to_wstring(message);
		for (std::vector<LLUrlMatch>::reverse_iterator it = matches.rbegin(); it != matches.rend(); ++it)
		{
			std::string url = wstring_to_utf8str(it->getUrl());
			if (isSuspicious(url))
			{
				std::string mangled_url = url;
				boost::replace_all(mangled_url, "http", "h-ttp");
				boost::replace_all(mangled_url, ".", "[dot]");
				
				w_message.replace(it->getStart(), it->getEnd() - it->getStart() + 1, utf8str_to_wstring(mangled_url));
			}
		}

		filtered_message = "[WARNING, LIKELY PHISHING ATTEMPT] " + wstring_to_utf8str(w_message) + " [WARNING, LIKELY PHISHING ATTEMPT]";
		return true;
	}

	filtered_message = message;
	return false;
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
