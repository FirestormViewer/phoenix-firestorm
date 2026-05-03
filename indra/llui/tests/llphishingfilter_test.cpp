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
		set_test_name("Evaluate risk scores");

		// Official domains should have 0 risk even with keywords
		ensure_equals("Official marketplace", LLPhishingFilter::instance().evaluateURLRisk("https://marketplace.secondlife.com/p/item/123"), 0);
		ensure_equals("Official domain", LLPhishingFilter::instance().evaluateURLRisk("https://secondlife.com/destinations"), 0);

		// Malicious domains with SL keywords should have high risk
		ensure_equals("Malicious marketplace on herokuapp", LLPhishingFilter::instance().evaluateURLRisk("http://marketplace.seconde-life-com.herokuapp.com/login"), 150); // 100 (keyword) + 50 (hosting)
		ensure_equals("Typosquatting", LLPhishingFilter::instance().evaluateURLRisk("https://marketplace.seconde-life.com/"), 100);

		// Benign non-SL domains should have 0 or low risk
		ensure_equals("Google", LLPhishingFilter::instance().evaluateURLRisk("https://www.google.com/search?q=secondlife"), 0); // keyword is in query, but not in domain
	}

	template<> template<>
	void phishingfilter_object::test<2>()
	{
		set_test_name("isSuspicious check");

		ensure("Official is not suspicious", !LLPhishingFilter::instance().isSuspicious("https://secondlife.com/"));
		ensure("Phishing is suspicious", LLPhishingFilter::instance().isSuspicious("http://marketplace-secondlife.com.herokuapp.com/"));
	}
}
