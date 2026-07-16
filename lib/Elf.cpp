#include "Debug/Elf.hpp"
#include "Debug/Debugger.hpp"
#include "Debug/Proc.hpp"

#include <cstddef>
#include <cstring>
#include <elf.h>
#include <vector>

namespace {

template <std::size_t N>
struct Elf {
	using Ehdr = typename std::conditional<N == 32, Elf32_Ehdr, Elf64_Ehdr>::type;
	using Phdr = typename std::conditional<N == 32, Elf32_Phdr, Elf64_Phdr>::type;
	using Dyn  = typename std::conditional<N == 32, Elf32_Dyn, Elf64_Dyn>::type;
	using Sym  = typename std::conditional<N == 32, Elf32_Sym, Elf64_Sym>::type;
};

/**
 * @brief Resolves the address of the `_dl_debug_state` symbol in the dynamic linker.
 *
 * @param ld_base The base address of the dynamic linker.
 * @param read_memory A function that reads memory from the target process. It should return true if the read was successful, false otherwise.
 * @return The address of the `_dl_debug_state` symbol, or nullptr if it could not be resolved.
 */
template <std::size_t N = 64>
void *resolve_dl_debug_state_internal(uint64_t ld_base, std::function<bool(void *, uint64_t, size_t)> read_memory) {

	using Ehdr = typename Elf<N>::Ehdr;
	using Phdr = typename Elf<N>::Phdr;
	using Dyn  = typename Elf<N>::Dyn;
	using Sym  = typename Elf<N>::Sym;

	// Read the ELF header of the dynamic linker to find the program headers
	Ehdr eh;
	if (!read_memory(&eh, ld_base, sizeof(eh))) {
		return nullptr;
	}

	// Read the program headers to find the dynamic section
	uint64_t phdr_addr = ld_base + eh.e_phoff;
	uint64_t ph_size   = eh.e_phnum * sizeof(Phdr);

	std::vector<Phdr> phdrs(eh.e_phnum);
	if (!read_memory(phdrs.data(), phdr_addr, static_cast<size_t>(ph_size))) {
		return nullptr;
	}

	// Find the dynamic section and read its entries to find the symbol table and string table
	uint64_t dyn_addr = 0;

	for (uint16_t i = 0; i < eh.e_phnum; i++) {
		if (phdrs[i].p_type == PT_DYNAMIC) {
			dyn_addr = ld_base + phdrs[i].p_vaddr;
			break;
		}
	}

	if (dyn_addr == 0) {
		return nullptr;
	}

	std::vector<Dyn> dyns;
	dyns.resize(256);

	if (!read_memory(dyns.data(), dyn_addr, dyns.size() * sizeof(Dyn))) {
		return nullptr;
	}

	// Find the string table and symbol table addresses and sizes
	uint64_t strtab_addr = 0;
	uint64_t symtab_addr = 0;
	uint64_t syment_size = 0;

	for (size_t i = 0; i < dyns.size(); i++) {
		if (dyns[i].d_tag == DT_NULL) {
			break;
		}

		switch (dyns[i].d_tag) {
		case DT_STRTAB:
			strtab_addr = ld_base + dyns[i].d_un.d_ptr;
			break;
		case DT_SYMTAB:
			symtab_addr = ld_base + dyns[i].d_un.d_ptr;
			break;
		case DT_SYMENT:
			syment_size = dyns[i].d_un.d_val;
			break;
		}
	}

	if (strtab_addr == 0 || symtab_addr == 0 || syment_size == 0) {
		return nullptr;
	}

	for (size_t idx = 0; idx < 4096; idx++) {

		Sym sym;
		uint64_t sym_addr = symtab_addr + idx * syment_size;

		if (!read_memory(&sym, sym_addr, sizeof(sym))) {
			return nullptr;
		}

		if (sym.st_name == 0) {
			continue;
		}

		char name_buf[256];
		uint64_t name_addr = strtab_addr + sym.st_name;

		if (!read_memory(name_buf, name_addr, sizeof(name_buf))) {
			return nullptr;
		}

		name_buf[sizeof(name_buf) - 1] = '\0';

		if (std::strcmp(name_buf, "_dl_debug_state") == 0) {
			return reinterpret_cast<void *>(ld_base + sym.st_value);
		}
	}

	return nullptr;
}

}

/**
 * @brief Resolves the address of the `_dl_debug_state` symbol in the dynamic linker.
 *
 * @param ld_base The base address of the dynamic linker.
 * @param read_memory A function that reads memory from the target process. It should return true if the read was successful, false otherwise.
 * @return The address of the `_dl_debug_state` symbol, or nullptr if it could not be resolved.
 */
void *resolve_dl_debug_state(uint64_t ld_base, pid_t pid, std::function<bool(void *, uint64_t, size_t)> read_memory) {
	Debugger::log("Resolving _dl_debug_state for ld_base=%p", reinterpret_cast<void *>(ld_base));
	if (is_64_bit_elf(pid)) {
		Debugger::log("Using 64-bit ELF resolver");
		return resolve_dl_debug_state_internal<64>(ld_base, read_memory);
	} else {
		Debugger::log("Using 32-bit ELF resolver");
		return resolve_dl_debug_state_internal<32>(ld_base, read_memory);
	}
}
