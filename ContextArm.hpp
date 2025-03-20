
#ifndef CONTEXT_ARM_HPP_
#define CONTEXT_ARM_HPP_

#include <cstddef>
#include <cstdint>

struct Context_arm {
	uint32_t regs[18];
};

static_assert(sizeof(Context_arm) == 72, "Context_arm is messed up!");

class Context {
public:
	Context_arm regs_;
};

#endif
