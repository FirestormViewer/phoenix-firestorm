// Standalone boundary checks: no viewer initialization or large allocations.
#include "llallocationlimits.h"
#include <cstdlib>
#include <iostream>

static void check(bool value)
{
    if (!value) std::abort();
}

template<class Exception, class F>
static void rejects(F operation)
{
    try { operation(); }
    catch (const Exception&) { return; }
    std::abort();
}

int main()
{
    using namespace LLAllocationLimits;
    check(squareCount(257) == 66049);
    check(squareCount(16) == 256);
    check(squareCount(769) == 591361); // non-power-of-two region
    check(squareCount(46340) == 2147395600);
    rejects<std::length_error>([] { squareCount(46341); });
    rejects<std::length_error>([] { squareCount(65537); });
    rejects<std::length_error>([] { squareCount(UINT64_MAX); });
    rejects<std::length_error>([] { squareCount(0); });
    check(arrayCount<int>(256) == 256);
    rejects<std::length_error>([] { arrayCount<int>(UINT64_MAX); });
    rejects<std::length_error>([] { arrayCount<int>(UINT64_C(2147483648)); });
    check(nearestPowerOfTwo(1) == 1);
    check(nearestPowerOfTwo(2) == 2);
    check(nearestPowerOfTwo(3) == 4);
    check(nearestPowerOfTwo(5) == 4);
    check(nearestPowerOfTwo(6) == 8);
    check(nearestPowerOfTwo(256) == 256);
    check(nearestPowerOfTwo(1024) == 1024);
    check(nearestPowerOfTwo(1610612735) == 1073741824);
    rejects<std::length_error>([] { nearestPowerOfTwo(1610612736); });
    rejects<std::length_error>([] { nearestPowerOfTwo(INT32_MAX); });
    rejects<std::invalid_argument>([] { nearestPowerOfTwo(0); });
    rejects<std::invalid_argument>([] { nearestPowerOfTwo(-1); });
    // Exhaustively preserve the old rounding result across ordinary sizes.
    for (int input = 1; input <= 65536; ++input)
    {
        int old = input;
        for (int i = 30; i > 0; --i)
        {
            if (old & (1 << i))
            {
                old = old >= (3 << (i - 1)) ? (1 << (i + 1)) : (1 << i);
                break;
            }
        }
        check(nearestPowerOfTwo(input) == old);
    }
    check(queueCapacity(2, 524288) == 2);
    check(queueCapacity(2048, 524288) == 2048);
    check(queueCapacity(524288, 524288) == 524288);
    for (std::size_t bad : {std::size_t(0), std::size_t(1), std::size_t(3),
                            std::size_t(1048576), SIZE_MAX})
    {
        rejects<std::invalid_argument>([bad] { queueCapacity(bad, 524288); });
    }
    std::cout << "Allocation boundary checks passed\n";
}
