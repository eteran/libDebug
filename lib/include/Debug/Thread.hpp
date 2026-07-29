
#ifndef THREAD_HPP_
#define THREAD_HPP_

#if defined(__x86_64__) || defined(__i386__)
#include "ThreadIntel.hpp"
#elif defined(__aarch64__) || defined(__arm__)
#include "ThreadArm.hpp"
#endif

#endif
