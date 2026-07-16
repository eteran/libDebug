
#ifndef ELF_HPP_
#define ELF_HPP_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <sys/types.h>

[[nodiscard]] void *resolve_dl_debug_state(uint64_t ld_base, pid_t pid, std::function<bool(void *, uint64_t, size_t)> read_memory);

#endif
