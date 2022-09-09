#include <sys/uio.h>    // struct iovec
#include <linux/elf.h>  // NT_PRSTATUS, NT_ARM_SYSTEM_CALL
#include <cstring>      // strerror
#include <errno.h>      // errno
#include "tracer.hpp"

const int tracer::n_syscall_arguments = 7;

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
	int syscall_number;
	struct iovec iov {
		(void *)&syscall_number,
		sizeof(syscall_number)
	};
	if(ptrace(PTRACE_GETREGSET, tracee.process_id, NT_ARM_SYSTEM_CALL, &iov) != 0) {
		throw tracer_exception("Unable to read ARM-specific system call register: " + std::to_string(errno) + " " + std::string(strerror(errno)));
	}
	return syscall_number;
}

void tracer::set_syscall_number(long number) {
	/* Aarch64 has a weird inconsistency, where writing the system call
	   number through PTRACE_SETREGSET does not work with NT_PRSTATUS.
	   This works around that.
	   See: https://stackoverflow.com/questions/63620203/ptrace-change-syscall-number-arm64/70530225#70530225 */
	tracer_ensure_invariants();
	struct user_regs_struct new_registers = read_registers();
	new_registers.regs[8] = number;  // This is where we would expect the system call number
	struct iovec iov {
		(void *)&new_registers.regs[8],
		sizeof(new_registers.regs[8])
	};
	if(ptrace(PTRACE_SETREGSET, tracee.process_id, NT_ARM_SYSTEM_CALL, &iov) != 0) {
		throw tracer_exception("Unable to write ARM-specific system call register: " + std::to_string(errno) + " " + std::string(strerror(errno)));
	}
}

long tracer::get_syscall_argument(size_t i) {
	// See e.g. glibc sysdeps/unix/sysv/linux/aarch64/syscall.S
	// for system call arguments and corresponding registers
	tracer_ensure_invariants();
	if(i < 0 || i > 7) {
		throw tracer_exception("syscall argument " + std::to_string(i) + " not in range (0,7)");
	}
	const struct user_regs_struct& registers = read_registers();
	return registers.regs[0 + i];
}

void tracer::set_syscall_argument(size_t i, long value) {
	tracer_ensure_invariants();
	if(i < 0 || i > 7) {
		throw tracer_exception("syscall argument " + std::to_string(i) + " not in range (0,7)");
	}
	struct user_regs_struct new_registers = read_registers();
	new_registers.regs[0 + i] = value;
	write_registers(new_registers);
}

long tracer::get_syscall_return_value() {
	tracer_ensure_invariants();
	const struct user_regs_struct& registers = read_registers();
	return registers.regs[0];
}

void tracer::set_syscall_return_value(long value) {
	tracer_ensure_invariants();
	struct user_regs_struct new_registers = read_registers();
	new_registers.regs[0] = value;
	write_registers(new_registers);
}
