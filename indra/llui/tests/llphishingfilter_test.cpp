/**
 * @file llphishingfilter_test.cpp
 * @brief Unit tests for LLPhishingFilter
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

#include "linden_common.h"
#include "lltut.h"
#include "llphishingfilter.h"
#include <vector>

namespace tut
{
	struct phishingfilter_data
	{
	};
	typedef test_group<phishingfilter_data> phishingfilter_test;
	typedef phishingfilter_test::object phishingfilter_object;
	tut::phishingfilter_test phishingfilter_testcase("LLPhishingFilter");
template<> template<>
void phishingfilter_object::test<1>()
{
	set_test_name("Regex scoring component");

	ensure_equals("Regex match secondlife", LLPhishingFilter::instance().getRegexScore("marketplace-secondiife.com"), 100);
	ensure_equals("Regex match marketplace", LLPhishingFilter::instance().getRegexScore("market-place.com"), 100);
	ensure_equals("Regex no match", LLPhishingFilter::instance().getRegexScore("google.com"), 0);
}

template<> template<>
void phishingfilter_object::test<2>()
{
	set_test_name("Levenshtein scoring component (proportional)");

	// secondlife.com (14 chars) -> threshold 6
	// distance 1: 100 - (100*1/14) = 100 - 7 = 93
	ensure_equals("Levenshtein close secondlife", LLPhishingFilter::instance().getLevenshteinScore("secondl1fe.com"), 93);
	ensure_equals("Levenshtein far secondlife", LLPhishingFilter::instance().getLevenshteinScore("relayforlife.com"), 0); // distance 7 (threshold 6)

	// marketplace (11 chars) -> threshold 4
	// distance 1: 100 - (100*1/11) = 100 - 9 = 91
	ensure_equals("Levenshtein close marketplace", LLPhishingFilter::instance().getLevenshteinScore("marketp1ace"), 91);
}
template<> template<>
void phishingfilter_object::test<3>()
{
	set_test_name("Provider/TLD scoring component");

	ensure_equals("Suspicious provider", LLPhishingFilter::instance().getProviderTLDScore("scam.herokuapp.com", "herokuapp.com"), 50);
	ensure_equals("Suspicious TLD", LLPhishingFilter::instance().getProviderTLDScore("scam.top", "scam.top"), 50);
	ensure_equals("Safe provider/TLD", LLPhishingFilter::instance().getProviderTLDScore("google.com", "google.com"), 0);
}

template<> template<>
void phishingfilter_object::test<4>()
{
	set_test_name("Suspicious URL detection list");

	struct TestCase {
		std::string url;
		bool expected_suspicious;
	};

	std::vector<TestCase> test_cases = {
		// Official - Safe
		{"https://secondlife.com/", false},
		{"https://marketplace.secondlife.com/p/item/123", false},
		{"https://maps.secondlife.com/secondlife/Ahern/128/128/2", false},
		{"secondlife:///app/agent/6b5042e8-112f-4f8f-990d-2a4a737bd391/about", false},

		// Legitimate non-SL - Safe
		{"https://www.google.com/search?q=secondlife", false},
		{"https://relayforlife.com/secondlife-campaign", false},
		{"https://en.wikipedia.org/wiki/Second_Life", false},

		// Explicit Test Domain - Suspicious
		{"http://suspicious-url.com/", true},

		// Typos/Regex/Levenshtein - Suspicious
		{"https://marketplace-secondiife.com", true},
		{"https://marketplace-secondife.online", true},
		{"https://second-lif.org/", true},
		{"https://seconde-live.net/", true},
		{"https://market-place-second-life.com", true},
		{"https://maps-secondlife.com", true},

		// Suspicious PaaS/DNS + Pattern - Suspicious
		{"https://second-lif-9bc8e3689e62.herokuapp.com/", true},
		{"https://marketplace-item-deal.vercel.app/", true},

		// Might be a scam, might not, but at least it's not impersonating an official Second Life site.
		{"https://free-sl-items.duckdns.org/", false},

		// Cheap TLDs - Suspicious (only if keywords are present or score is high)
		{"https://secondlife-deals.shop/", true},
		{"https://marketplace-sl.top/", true},

		// Levenshtein Boundary Tests for 14-char string (Threshold < 6)
		// Distance 3: 100 - (300/14) = 100 - 21 = 79 (Suspicious, >= 75)
		{"https://sec123life.com", true}, 
		// Distance 4: 100 - (400/14) = 100 - 28 = 72 (Safe, < 75)
		{"https://se1234life.com", false}
	};

	for (const auto& tc : test_cases)
	{
		std::string msg = "URL: " + tc.url + (tc.expected_suspicious ? " should be suspicious" : " should NOT be suspicious");
		ensure(msg, LLPhishingFilter::instance().isSuspicious(tc.url) == tc.expected_suspicious);
	}
	}

	template<> template<>
	void phishingfilter_object::test<5>()
	{
	set_test_name("calculateLevenshteinDistance raw tests");

	ensure_equals("Equal strings", LLPhishingFilter::instance().calculateLevenshteinDistance("abc", "abc"), 0);
	ensure_equals("One char diff", LLPhishingFilter::instance().calculateLevenshteinDistance("abc", "abd"), 1);
	ensure_equals("One insertion", LLPhishingFilter::instance().calculateLevenshteinDistance("abc", "abcd"), 1);
	ensure_equals("One deletion", LLPhishingFilter::instance().calculateLevenshteinDistance("abcd", "abc"), 1);
	ensure_equals("Completely different", LLPhishingFilter::instance().calculateLevenshteinDistance("abc", "xyz"), 3);
	ensure_equals("Transposition (distance 2)", LLPhishingFilter::instance().calculateLevenshteinDistance("abc", "acb"), 2);
	}
	}
