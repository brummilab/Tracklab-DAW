// RTSan negative test (CTest entry spike.rtsan_negative, only registered for the RealtimeSanitizer preset).
//
// The callback below breaks docs/realtime.md on purpose: it allocates inside a [[clang::nonblocking]] function.
// RealtimeSanitizer has to stop the process; the CMake wrapper cmake/rtsan_expect_failure.cmake checks that. If this
// test ever "passes" on its own, the sanitizer is not doing its job and the gate is blind.
#include "spike_realtime.h"
#include "test_support.h"

#include <cstdlib>

namespace
{
// The volatile pointer keeps the optimiser from removing the malloc/free pair.
void allocatingCallback() noexcept SPIKE_NONBLOCKING
{
    void* volatile block = std::malloc(64);
    std::free(block);
}
}  // namespace

TEST_SUITE("rtsan_negative")
{
    // Skipped by default: without RealtimeSanitizer this is a plain malloc, with it the process aborts. The CTest
    // entry passes --no-skip.
    TEST_CASE("an allocating callback marked nonblocking is stopped by RealtimeSanitizer" * doctest::skip())
    {
        allocatingCallback();
        FAIL("RealtimeSanitizer did not stop the allocating callback");
    }
}
