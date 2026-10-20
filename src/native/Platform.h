#pragma once

// Preserve the existing Windows optimization boundary on other compilers.
#if defined(_MSC_VER)
#define KARMA_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define KARMA_NOINLINE __attribute__((noinline))
#else
#define KARMA_NOINLINE
#endif
