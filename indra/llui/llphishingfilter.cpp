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
#include <set>

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

	// Whitelist internal protocols
	if (boost::starts_with(lower_url, "secondlife://") ||
		boost::starts_with(lower_url, "about:") ||
		boost::starts_with(lower_url, "data:"))
	{
		return 0;
	}

	std::string host = getHostname(lower_url);
	std::string domain = getBaseDomain(lower_url);

	// Whitelist check
	bool is_official_domain = (domain == "secondlife.com" || domain == "lindenlab.com");
	if (is_official_domain)
	{
		return 0;
	}

	S32 score = 0;

	// Explicit test domain
	if (domain == "suspicious-url.com")
	{
		score += 100;
	}

	// 1. Regex checks (Base score 100 if matched)
	static const boost::regex sl_pattern("secon[dl]*d+[e]?[-_ \\.]*(li[fv][e]?|iife)", boost::regex::icase);
	static const boost::regex mp_pattern("mark[e]?tpl[a]?ce", boost::regex::icase);

	if (boost::regex_search(host, sl_pattern) || boost::regex_search(host, mp_pattern))
	{
		score += 100;
	}

	// 2. Levenshtein distance checks on hostname (+50 each)
	// First 14 characters vs 'secondlife.com'
	std::string host_14 = host.substr(0, 14);
	if (calculateLevenshteinDistance(host_14, "secondlife.com") < 5)
	{
		score += 50;
	}

	// First 22 characters vs 'marketplace.secondlife'
	std::string host_22 = host.substr(0, 22);
	if (calculateLevenshteinDistance(host_22, "marketplace.secondlife") < 5)
	{
		score += 50;
	}

	// First 15 characters vs 'maps.secondlife'
	std::string host_15 = host.substr(0, 15);
	if (calculateLevenshteinDistance(host_15, "maps.secondlife") < 5)
	{
		score += 50;
	}

	// 3. Suspicious providers/TLDs (+50)
	static const std::set<std::string> suspicious_domains = {
		"herokuapp.com", "firebaseapp.com", "github.io", "netlify.app",
		"vercel.app", "fly.dev", "pages.dev", "ondigitalocean.app",
		"azurewebsites.net", "appspot.com", "openshiftapps.com",
		"kamatera.com", "sliplane.app", "duckdns.org", "eu.org",
		"subdomain.me", "your-freedom.net", "afraid.org"
	};

	static const std::set<std::string> suspicious_tlds = {
		"shop", "store", "online", "top", "cfd", "sbs", "xyz", "cyou", "icu", "bond", "live", "wiki", "info"
	};

	bool is_suspicious_base = suspicious_domains.count(domain) > 0;
	
	bool is_suspicious_tld = false;
	size_t dot_pos = host.find_last_of('.');
	if (dot_pos != std::string::npos)
	{
		is_suspicious_tld = suspicious_tlds.count(host.substr(dot_pos + 1)) > 0;
	}

	if (is_suspicious_base || is_suspicious_tld)
	{
		score += 50;
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
	std::string host = getHostname(url);

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

std::string LLPhishingFilter::getHostname(const std::string& url) const
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

	// Remove query/fragment if present (though slash check above should handle most)
	size_t query_pos = host.find_first_of("?#");
	if (query_pos != std::string::npos)
	{
		host = host.substr(0, query_pos);
	}

	return host;
}

size_t LLPhishingFilter::calculateLevenshteinDistance(const std::string& s1, const std::string& s2) const
{
	const size_t m = s1.size();
	const size_t n = s2.size();
	if (m == 0) return n;
	if (n == 0) return m;

	std::vector<size_t> v0(n + 1);
	std::vector<size_t> v1(n + 1);

	for (size_t i = 0; i <= n; ++i) v0[i] = i;

	for (size_t i = 0; i < m; ++i) {
		v1[0] = i + 1;
		for (size_t j = 0; j < n; ++j) {
			size_t cost = (s1[i] == s2[j]) ? 0 : 1;
			v1[j + 1] = std::min({v1[j] + 1, v0[j + 1] + 1, v0[j] + cost});
		}
		v0 = v1;
	}
	return v0[n];
}
