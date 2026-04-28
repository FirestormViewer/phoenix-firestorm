/**
 * @file llfloaterimcontainer_ll.h
 * @brief LLFloaterIMContainerLL — Phase 0 minimal stub (LL-style Chat Window)
 *
 * Coexists with FSFloaterIMContainer (Tab UI). This file is intentionally
 * minimal; conversation list integration is deferred to Phase 1+.
 */

// <FS:AYA> Phase 0: LL-style Chat Window coexistence stub
#ifndef LL_LLFLOATERIMCONTAINERLL_H
#define LL_LLFLOATERIMCONTAINERLL_H

#include "llfloater.h"

class LLFloaterIMContainerLL : public LLFloater
{
public:
    LLFloaterIMContainerLL(const LLSD& key);
    virtual ~LLFloaterIMContainerLL() = default;

    bool postBuild() override;
};

#endif // LL_LLFLOATERIMCONTAINERLL_H
// </FS:AYA>
