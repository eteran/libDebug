
#ifndef REGION_HPP_
#define REGION_HPP_

#include <cstdint>
#include <string>

class Region {
public:
	Region(uint64_t start, uint64_t end, uint64_t offset, uint64_t permissions, std::string name)
		: start_(start),
		  end_(end),
		  offset_(offset),
		  permissions_(permissions),
		  name_(std::move(name)) {}

	Region(const Region &)            = default;
	Region &operator=(const Region &) = default;
	Region(Region &&)                 = default;
	Region &operator=(Region &&)      = default;

public:
	enum Permissions : uint64_t {
		None    = 0x0000,
		Read    = 0x0001,
		Write   = 0x0002,
		Execute = 0x0004,
		Private = 0x2000,
		Shared  = 0x1000,
	};

public:
	[[nodiscard]] uint64_t start() const { return start_; }
	[[nodiscard]] uint64_t end() const { return end_; }
	[[nodiscard]] uint64_t permissions() const { return permissions_; }
	[[nodiscard]] uint64_t offset() const { return offset_; }
	[[nodiscard]] const std::string &name() const { return name_; }

public:
	[[nodiscard]] bool is_readable() const { return permissions_ & Read; }
	[[nodiscard]] bool is_writable() const { return permissions_ & Write; }
	[[nodiscard]] bool is_executable() const { return permissions_ & Execute; }
	[[nodiscard]] bool is_private() const { return permissions_ & Private; }
	[[nodiscard]] bool is_shared() const { return permissions_ & Shared; }

public:
	[[nodiscard]] bool is_stack() const { return name_.find("[stack]") != std::string::npos; }
	[[nodiscard]] bool is_heap() const { return name_.find("[heap]") != std::string::npos; }
	[[nodiscard]] bool is_vdso() const { return name_.find("[vdso]") != std::string::npos; }

private:
	uint64_t start_;
	uint64_t end_;
	uint64_t offset_;
	uint64_t permissions_;
	std::string name_;
};

#endif
