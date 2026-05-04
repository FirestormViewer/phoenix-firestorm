/**
 * @file llphishingfilter.h
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

#ifndef LL_PHISHINGFILTER_H
#define LL_PHISHINGFILTER_H

#include "llsingleton.h"
#include <string>
#include <vector>

class LLPhishingFilter : public LLSingleton<LLPhishingFilter>
{
	LLSINGLETON(LLPhishingFilter);
	~LLPhishingFilter();

public:
	/**
	 * @brief Evaluates the risk score of a given URL.
	 * @param url The URL to evaluate.
	 * @return A score between 0 and 100, where higher is more suspicious.
	 */
	S32 evaluateURLRisk(const std::string& url) const;

	/**
	 * @brief Checks if a URL is considered suspicious based on its risk score.
	 * @param url The URL to check.
	 * @return True if the URL is suspicious.
	 */
	bool isSuspicious(const std::string& url) const;

private:
	std::string getBaseDomain(const std::string& url) const;
	std::string getHostname(const std::string& url) const;
	size_t calculateLevenshteinDistance(const std::string& s1, const std::string& s2) const;
};

#endif // LL_PHISHINGFILTER_H
