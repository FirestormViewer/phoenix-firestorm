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

		// Malicious domains with SL keywords in hostname should have high risk
		ensure_equals("Malicious marketplace on herokuapp", LLPhishingFilter::instance().evaluateURLRisk("http://marketplace.seconde-life-com.herokuapp.com/login"), 150); // 100 (keyword) + 50 (hosting)
		ensure_equals("Typosquatting", LLPhishingFilter::instance().evaluateURLRisk("https://marketplace.seconde-life.com/"), 100);
		ensure_equals("Phishing on herokuapp", LLPhishingFilter::instance().evaluateURLRisk("https://second-lif-9bc8e3689e62.herokuapp.com/"), 150);

		// Benign domains with SL keywords in PATH or QUERY should have 0 or low risk
		ensure_equals("Google search", LLPhishingFilter::instance().evaluateURLRisk("https://www.google.com/search?q=secondlife"), 0); 
		ensure_equals("Relay for life", LLPhishingFilter::instance().evaluateURLRisk("https://relayforlife.com/secondlife-campaign"), 0);

		// Internal protocol
		ensure_equals("Internal protocol", LLPhishingFilter::instance().evaluateURLRisk("secondlife:///app/agent/6b5042e8-112f-4f8f-990d-2a4a737bd391/about"), 0);

		// Explicit test domain
		ensure_equals("Suspicious test domain", LLPhishingFilter::instance().evaluateURLRisk("http://www.suspicious-url.com/path"), 100);
	}

	template<> template<>
	void phishingfilter_object::test<2>()
	{
		set_test_name("isSuspicious check");

		ensure("Official is not suspicious", !LLPhishingFilter::instance().isSuspicious("https://secondlife.com/"));
		ensure("Official marketplace is not suspicious", !LLPhishingFilter::instance().isSuspicious("https://marketplace.secondlife.com/p/item/123"));
		ensure("Relay for life is not suspicious", !LLPhishingFilter::instance().isSuspicious("https://relayforlife.com/secondlife-campaign"));
		ensure("Internal protocol is not suspicious", !LLPhishingFilter::instance().isSuspicious("secondlife:///app/agent/6b5042e8-112f-4f8f-990d-2a4a737bd391/about"));
		ensure("Suspicious test domain is suspicious", LLPhishingFilter::instance().isSuspicious("http://suspicious-url.com/"));

		// Real world scam URLs
		ensure("second-lif herokuapp", LLPhishingFilter::instance().isSuspicious("https://second-lif-9bc8e3689e62.herokuapp.com/"));
		ensure("marketplace-secondellife herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-secondellife-6c9143ddec6d.herokuapp.com/"));
		ensure("marketplace-seconldlife herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-seconldlife-let-p-be1335f33c3f.herokuapp.com/id.html"));
		ensure("marketplace-seconldlife-angel herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-seconldlife-angel-b45a44c35bab.herokuapp.com/"));
		ensure("marketplace-secondlife-com herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-secondlife-com-p-m-655efa0e1350.herokuapp.com/"));
		ensure("marketplacesecondlife herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplacesecondlife-necklace-7a51f474ed25.herokuapp.com/"));
		ensure("marketplace-secondlife-p herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-secondlife-p-re-my-1c3fab5d4034.herokuapp.com/id.htm"));
		ensure("kirastyle herokuapp (pattern match)", LLPhishingFilter::instance().isSuspicious("https://kirastyle-283-2787130-4aff445324e8.herokuapp.com/"));
		ensure("marketplace-seconldlife-let-pe herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-seconldlife-let-pe-b8bb43cf66f1.herokuapp.com/id.html"));
		ensure("seconde-live herokuapp", LLPhishingFilter::instance().isSuspicious("https://seconde-live-1d5912081d12.herokuapp.com/"));
		ensure("marketplace-secondlife herokuapp", LLPhishingFilter::instance().isSuspicious("https://marketplace-secondlife-28fbbedee6b3.herokuapp.com/"));
		ensure("secondlife-marketplaces-nonoma herokuapp", LLPhishingFilter::instance().isSuspicious("https://secondlife-marketplaces-nonoma-6e7834ae410b.herokuapp.com/"));
	}
}
