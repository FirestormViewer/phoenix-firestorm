#ifndef GROWL_STUB_HPP
#define GROWL_STUB_HPP

#include <string>

#define GROWL_TCP 0

class Growl
{
public:
    Growl(int, const char*, const char*, const char** const, int, const char*) {}
    ~Growl() {}
    void Notify(const char*, const char*, const char*) {}
    bool isConnected() { return false; }
};

#endif
