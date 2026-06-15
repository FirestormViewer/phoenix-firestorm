
//          Copyright Oliver Kowalke 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(BOOST_USE_UCONTEXT)
#include "boost/context/continuation_ucontext.hpp"
#elif defined(BOOST_USE_WINFIB)
#include "boost/context/continuation_winfib.hpp"
#endif

#include <boost/config.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace context {
namespace detail {

// zero-initialization
thread_local activation_record * current_rec;
thread_local static std::size_t counter;

// schwarz counter
activation_record_initializer::activation_record_initializer() noexcept {
    if ( 0 == counter++) {
        current_rec = new activation_record();
    }
}

activation_record_initializer::~activation_record_initializer() {
    if ( 0 == --counter) {
        BOOST_ASSERT( current_rec->is_main_context() );
        delete current_rec;
    }
}

// ARM64 Windows PAC fix: replace thread_local static in current() with
// heap-allocated initializer via plain thread_local pointer.
static thread_local activation_record_initializer * s_cont_initializer{ nullptr };
static thread_local bool s_cont_dtor_running{ false };

struct cont_initializer_cleanup {
    ~cont_initializer_cleanup() {
        if ( s_cont_initializer) {
            s_cont_dtor_running = true;
            activation_record_initializer * p = s_cont_initializer;
            s_cont_initializer = nullptr;
            delete p;
        }
    }
};
static thread_local cont_initializer_cleanup s_cont_cleanup{};

}

namespace detail {

activation_record *&
activation_record::current() noexcept {
    if ( ! s_cont_initializer && ! s_cont_dtor_running) {
        s_cont_initializer = new activation_record_initializer{};
    }
    return current_rec;
}

}

}}

#ifdef BOOST_HAS_ABI_HEADERS
# include BOOST_ABI_SUFFIX
#endif
