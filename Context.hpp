
#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#if defined(__x86_64__) || defined(__i386__)
#include "ContextIntel.hpp"
#elif defined(__arm__)
#include "ContextArm.hpp"
#endif

#endif
