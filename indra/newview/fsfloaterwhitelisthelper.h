#ifndef FSFLOATERWHITELISTHELPER_H
#define FSFLOATERWHITELISTHELPER_H

#include "llfloater.h"

class FSFloaterWhiteListHelper : public LLFloater
{
public:
    explicit FSFloaterWhiteListHelper(const LLSD& key);
    ~FSFloaterWhiteListHelper() final;
    public:
    BOOL postBuild() final;

private:
    void populateWhitelistInfo();
};
#endif // FSFLOATERWHITELISTHELPER_H
