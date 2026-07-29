
#ifndef BREAKPOINT_HPP_
#define BREAKPOINT_HPP_

#if defined(__x86_64__) || defined(__i386__)
#include "BreakpointIntel.hpp"
#elif defined(__aarch64__) || defined(__arm__)
#include "BreakpointArm.hpp"
#endif

#endif
