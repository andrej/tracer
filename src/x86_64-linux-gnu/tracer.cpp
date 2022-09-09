#include <sys/uio.h>    // struct iovec
#include <linux/elf.h>  // NT_PRSTATUS, NT_ARM_SYSTEM_CALL
#include <cstring>      // strerror
#include <errno.h>      // errno
#include "tracer.hpp"

const int tracer::n_syscall_arguments = 6;

long tracer::_read_registers_internal(pid_t pid, struct user_regs_struct& destination) {
	struct iovec iov {
		(void *)&destination,
		sizeof(destination)
	};
	return ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov);
}

long tracer::_write_registers_internal(pid_t pid, const struct user_regs_struct& source) {
	struct iovec iov {
		(void *)&source,
		sizeof(source)
	};
	return ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

long tracer::get_syscall_number() {
	tracer_ensure_invariants();
	const struct user_regs_struct& registers = read_registers();
	return registers.orig_rax;
}

void tracer::set_syscall_number(long number) {
	/* Aarch64 has a weird inconsistency, where writing the system call
	   number through PTRACE_SETREGSET does not work with NT_PRSTATUS.
	   This works around that.
	   See: https://stackoverflow.com/questions/63620203/ptrace-change-syscall-number-arm64/70530225#70530225 */
	tracer_ensure_invariants();
	struct user_regs_struct new_registers = read_registers();
	new_registers.orig_rax = number;  // This is where we would expect the system call number
	write_registers(new_registers);
}

long tracer::get_syscall_argument(size_t i) {
	// See e.g. glibc sysdeps/unix/sysv/linux/x86_64/syscall.S
	// for the registers to arguments correspondence
	tracer_ensure_invariants();
	const struct user_regs_struct& registers = read_registers();
	switch(i) {
		case 0:
			return registers.rdi;
		case 1:
			return registers.rsi;
		case 2: 
			return registers.rdx;
		case 3:
			return registers.r10;
		case 4:
			return registers.r8; 
		case 5:
			return registers.r9;
		default:
			throw tracer_exception("syscall argument " + std::to_string(i) + " not in range (0,5)");
	}
	return -1; // unreachable
}

void tracer::set_syscall_argument(size_t i, long value) {
	tracer_ensure_invariants();
	struct user_regs_struct new_registers = read_registers();
	switch(i) {
		case 0:
			new_registers.rdi = value;
			break;
		case 1:
			new_registers.rsi = value;
			break;
		case 2: 
			new_registers.rdx = value;
			break;
		case 3:
			new_registers.r10 = value;
			break;
		case 4:
			new_registers.r8 = value; 
			break;
		case 5:
			new_registers.r9 = value;
			break;
		default:
			throw tracer_exception("syscall argument " + std::to_string(i) + " not in range (0,5)");
	}
	write_registers(new_registers);
}

long tracer::get_syscall_return_value() {
	tracer_ensure_invariants();
	const struct user_regs_struct& registers = read_registers();
	return registers.rax;
}

void tracer::set_syscall_return_value(long value) {
	tracer_ensure_invariants();
	struct user_regs_struct new_registers = read_registers();
	new_registers.rax = value;
	write_registers(new_registers);
}
