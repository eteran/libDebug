
#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

class Memory {
public:
	virtual ~Memory()                                                        = default;
	virtual int64_t read(uint64_t address, void *buffer, size_t size)        = 0;
	virtual int64_t write(uint64_t address, const void *buffer, size_t size) = 0;
};

class ProcMemory : public Memory {
public:
	ProcMemory(pid_t pid);
	~ProcMemory();
	int64_t read(uint64_t address, void *buffer, size_t size) override;
	int64_t write(uint64_t address, const void *buffer, size_t size) override;

private:
	int memfd_ = -1;
};

class PtraceMemory : public Memory {
public:
	PtraceMemory(pid_t pid);
	~PtraceMemory() = default;
	int64_t read(uint64_t address, void *buffer, size_t size) override;
	int64_t write(uint64_t address, const void *buffer, size_t size) override;

private:
	pid_t pid_;
};

#endif
