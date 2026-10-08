// Real-time annotations of the engine spike (docs/realtime.md).
#pragma once

// [[clang::nonblocking]] marks functions that run on the audio thread. Clang 19 introduced the attribute, Clang 20
// verifies it (-Wfunction-effects) and adds RealtimeSanitizer (-fsanitize=realtime), which stops the process when
// an annotated function allocates, locks or does IO. The `linux-clang-rtsan` preset runs the tests that way
// (scripts/gate.sh rtsan). Clang 18 (Ubuntu 24.04) and GCC would warn about the unknown attribute, which breaks
// -Werror, so there the macro expands to nothing.
// It is a function type attribute: write it after `noexcept` (Clang 20 requires it: a nonblocking function must not
// throw), e.g. `void process() noexcept SPIKE_NONBLOCKING;` or
// `void processBlock (...) noexcept SPIKE_NONBLOCKING override;`.
#if defined(__clang__) && defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::nonblocking)
#define SPIKE_NONBLOCKING [[clang::nonblocking]]
#endif
#endif

#ifndef SPIKE_NONBLOCKING
#define SPIKE_NONBLOCKING
#endif
