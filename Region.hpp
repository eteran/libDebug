
#ifndef REGION_HPP_
#define REGION_HPP_

#include <cstdint>
#include <string>

class Region {
public:
	Region(uint64_t start, uint64_t end, uint64_t permissions, const std::string &name)
		: start_(start), end_(end), permissions_(permissions), name_(name) {}

	Region(const Region &)            = default;
	Region &operator=(const Region &) = default;
	Region(Region &&)                 = default;
	Region &operator=(Region &&)      = default;

public:
	uint64_t start() const { return start_; }
	uint64_t end() const { return end_; }
	uint64_t permissions() const { return permissions_; }
	const std::string &name() const { return name_; }

public:
	bool is_readable() const { return permissions_ & 0x4; }
	bool is_writable() const { return permissions_ & 0x2; }
	bool is_executable() const { return permissions_ & 0x1; }
	bool is_private() const { return permissions_ & 0x2000; }
	bool is_shared() const { return permissions_ & 0x1000; }

public:
	bool is_stack() const { return name_.find("[stack]") != std::string::npos; }
	bool is_heap() const { return name_.find("[heap]") != std::string::npos; }
	bool is_vdso() const { return name_.find("[vdso]") != std::string::npos; }

private:
	uint64_t start_;
	uint64_t end_;
	uint64_t permissions_;
	std::string name_;
};

#endif
