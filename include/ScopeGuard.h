/*
 The MIT License (MIT)

 Copyright (c) 2018 HJC hjcapple@gmail.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#ifndef __SCOPE_GUARD_H__
#define __SCOPE_GUARD_H__

#define __SCOPEGUARD_CONCATENATE_IMPL(s1, s2) s1##s2
#define __SCOPEGUARD_CONCATENATE(s1, s2) __SCOPEGUARD_CONCATENATE_IMPL(s1, s2)

#if defined(__cplusplus)

#include <type_traits>

// ScopeGuard for C++11
namespace clover {
    template <typename Fun>
    class ScopeGuard {
    public:
        ScopeGuard(Fun &&f) : _fun(std::forward<Fun>(f)), _active(true) {
        }

        ~ScopeGuard() {
            if (_active) {
                _fun();
            }
        }

        void dismiss() {
            _active = false;
        }

        ScopeGuard() = delete;
        ScopeGuard(const ScopeGuard &) = delete;
        ScopeGuard &operator=(const ScopeGuard &) = delete;

        ScopeGuard(ScopeGuard &&rhs) : _fun(std::move(rhs._fun)), _active(rhs._active) {
            rhs.dismiss();
        }

    private:
        Fun _fun;
        bool _active;
    };

    namespace detail {
        enum class ScopeGuardOnExit {};

        template <typename Fun>
        inline ScopeGuard<Fun> operator+(ScopeGuardOnExit, Fun &&fn) {
            return ScopeGuard<Fun>(std::forward<Fun>(fn));
        }
    } // namespace detail
} // namespace clover

// Helper macro
#define ON_SCOPE_EXIT \
    auto __SCOPEGUARD_CONCATENATE(ext_exitBlock_, __LINE__) = clover::detail::ScopeGuardOnExit() + [&]()

#else

// ScopeGuard for Objective-C
typedef void (^ext_cleanupBlock_t)(void);
static inline void ext_executeCleanupBlock(__strong ext_cleanupBlock_t *block) {
    (*block)();
}

#define ON_SCOPE_EXIT                                                              \
    __strong ext_cleanupBlock_t __SCOPEGUARD_CONCATENATE(ext_exitBlock_, __LINE__) \
        __attribute__((cleanup(ext_executeCleanupBlock), unused)) = ^

#endif

#endif /* __SCOPE_GUARD_H__ */
