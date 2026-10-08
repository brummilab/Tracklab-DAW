// Real-time annotations of the engine spike (docs/realtime.md).
#pragma once

// [[clang::nonblocking]] marks functions that run on the audio thread. The attribute exists from Clang 20 on;
// Clang 18 (Ubuntu 24.04) and GCC would reject or warn about the unknown attribute, which breaks -Werror.
// So the macro expands to nothing where the compiler cannot check it. The real check is RealtimeSanitizer (M0-07).
// It is a function type attribute: write it after the parameter list, e.g. `void process() SPIKE_NONBLOCKING;`
// or `void processBlock (...) SPIKE_NONBLOCKING override;`.
#if defined(__clang__) && defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::nonblocking)
#define SPIKE_NONBLOCKING [[clang::nonblocking]]
#endif
#endif

#ifndef SPIKE_NONBLOCKING
#define SPIKE_NONBLOCKING
#endif
