#include "../remote/bsremotevalidation.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace BlazingStorm;
    int page;
    assert(parseInventoryPage("", page) && page == 0);
    assert(parseInventoryPage("12", page) && page == 12);
    for (const auto* text : {"-1", "+1", "1x", "1,2", "100001", "999999999999999999999", " 1"})
        assert(!parseInventoryPage(text, page));
    double x, y, z;
    assert(parseTeleportPosition("256128,256064,23.5", x, y, z));
    assert(x == 256128 && y == 256064 && z == 23.5);
    assert(parseTeleportPosition("0,4294967295,4096", x, y, z));
    for (const auto* text : {"", "nan,0,0", "0,inf,0", "0,0,1e999", "1,2", "1,2,3,4",
                             "1,2,3x", "1;2;3", "-1,2,3", "1,2,-1", "1,2,4097", "4294967296,0,0"})
        assert(!parseTeleportPosition(text, x, y, z));
    assert(mayRezCopy(true,true,true,false,true,true,false));
    assert(!mayRezCopy(true,true,true,false,false,true,false)); // no-copy
    assert(!mayRezCopy(true,true,true,false,true,false,false)); // revoked
    assert(!mayRezCopy(true,true,true,false,true,true,true));   // RLVa
    assert(!mayRezCopy(false,true,true,false,true,true,false)); // other inventory
    assert(!mayRezCopy(true,false,true,false,true,true,false)); // non-object
    assert(!mayRezCopy(true,true,false,false,true,true,false)); // not fetched
    assert(!mayRezCopy(true,true,true,true,true,true,false));   // unresolved link
    std::cout << "Remote payload and copy-only rez validation passed\n";
}
