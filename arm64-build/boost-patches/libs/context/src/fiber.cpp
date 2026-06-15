
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(BOOST_USE_UCONTEXT)
#include "boost/context/fiber_ucontext.hpp"
#elif defined(BOOST_USE_WINFIB)
#include "boost/context/fiber_winfib.hpp"
#endif

#include <boost/config.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace context {
namespace detail {

// zero-initialization
thread_local fiber_activation_record * fib_current_rec;
thread_local static std::size_t counter;

// schwarz counter
fiber_activation_record_initializer::fiber_activation_record_initializer() noexcept {
    if ( 0 == counter++) {
        fib_current_rec = new fiber_activation_record();
    }
}

fiber_activation_record_initializer::~fiber_activation_record_initializer() {
    if ( 0 == --counter) {
        BOOST_ASSERT( fib_current_rec->is_main_context() );
        delete fib_current_rec;
    }
}

// ARM64 Windows PAC fix: replace thread_local static in current() with
// heap-allocated initializer via plain thread_local pointer.
// Same fix as boost::fibers::context::active() — see context.cpp.
static thread_local fiber_activation_record_initializer * s_fib_initializer{ nullptr };
static thread_local bool s_fib_dtor_running{ false };

struct fib_initializer_cleanup {
    ~fib_initializer_cleanup() {
        if ( s_fib_initializer) {
            s_fib_dtor_running = true;
            fiber_activation_record_initializer * p = s_fib_initializer;
            s_fib_initializer = nullptr;
            delete p;
        }
    }
};
static thread_local fib_initializer_cleanup s_fib_cleanup{};

}

namespace detail {

fiber_activation_record *&
fiber_activation_record::current() noexcept {
    if ( ! s_fib_initializer && ! s_fib_dtor_running) {
        s_fib_initializer = new fiber_activation_record_initializer{};
    }
    return fib_current_rec;
}

}

}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif
