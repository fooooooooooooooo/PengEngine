#pragma once

// TODO: add a checkf macro
// TODO: add an unreachable macro

#ifndef NO_CHECKS

#include <core/logger.h>

#ifdef _WIN32
#include <intrin.h>
#define _BREAK __debugbreak()
#else
#include <signal.h>
//#define _BREAK raise(SIGTRAP)
#define _BREAK ((void)0)
#endif

#define STRINGIFY(x) #x
#define TO_STR(x) STRINGIFY(x)

#define check(expression)                                                                        \
    do                                                                                           \
    {                                                                                            \
        if (!(expression)) [[unlikely]]                                                          \
        {                                                                                        \
            Logger::error("Assertion failed: " __FILE__ "(" TO_STR(__LINE__) "): " #expression); \
            _BREAK;                                                                              \
        }                                                                                        \
    }                                                                                            \
    while (0)

#define verify(expression) check(expression)

#else

#define check(expression) ((void)0)
#define verify(expression) expression

#endif
