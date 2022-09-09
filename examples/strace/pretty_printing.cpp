#include <iostream>       // std::cout
#include <sstream>        // std::ostringstream
#include <sys/syscall.h>  // syscall numbers
#include "pretty_printing.hpp"

std::ostream& pretty_print::out = std::cerr;

/**
 * @brief Returns the documented number of arguments for the given system call.
 * Incomplete, just for demonstration purposes. 
 */
int pretty_print::get_syscall_n_arguments(long syscall_number) {
	int n_args = tracer::n_syscall_arguments;
	switch(syscall_number) {
		case __NR_brk:
		case __NR_close:
		case __NR_exit:
		case __NR_exit_group:
			n_args = 1;
			break;
		case __NR_read:
		case __NR_write:
#ifdef __NR_open
		case __NR_open:
#endif
#ifdef __NR_access
		case __NR_access:
#endif
#ifdef __NR_stat
		case __NR_stat:
#endif
		case __NR_fstat:
		case __NR_mprotect:
		case __NR_execve:
			n_args = 3;
			break;
		case __NR_openat:
		case __NR_faccessat:
#ifdef __NR_fstatat
		case __NR_fstatat:
#endif
#ifdef __NR_newfstatat
		case __NR_newfstatat:
#endif
			n_args = 4;
			break;
		case __NR_execveat:
			n_args = 5;
			break;
		case __NR_mmap:
			n_args = 6;
			break;
	}
	return n_args;
}


/**
 * @brief Prints a strace-like half-line about the called system call (name) and
 * its arguments. The real strace uses its knowledge of system call arguments,
 * interprets them and prints more meaningful information. Since this is only
 * intended as an example use of the tracer, we only implement this for a small
 * subset of arguments.
 */
void pretty_print::print_syscall_entry(tracer &child_tracer, long syscall_number) {
	long n_args = get_syscall_n_arguments(syscall_number);
	out << child_tracer.get_syscall_name() << "(";
	if(n_args > 0) {
		print_syscall_argument_1(child_tracer);
	}
	if(n_args > 1) {
		out << ", ";
		print_syscall_argument_2(child_tracer);
	}
	if(n_args > 2) {
		out << ", ";
		print_syscall_argument_3(child_tracer);
	}
	if(n_args > 3) {
		out << ", ";
		print_syscall_argument_4(child_tracer);
	}
	// We skip special treatment of the rest of the arguments for brevity
	for(int i = 4; i < n_args; i++) {
		out << ", ";
		print_default(child_tracer.get_syscall_argument(i));
	}
	out << ")";
}

void pretty_print::print_syscall_argument_1(tracer& child_tracer) {
	long syscall_number = child_tracer.get_syscall_number();
	long argument_1 = child_tracer.get_syscall_argument(0);
	switch(syscall_number) {
#ifdef __NR_open
		case __NR_open:
#endif
#ifdef __NR_access
		case __NR_access:
#endif
#ifdef __NR_stat
		case __NR_stat:
#endif
#ifdef __NR_fstatat
		case __NR_fstatat:
#endif
		case __NR_execve:
			print_string_pointer(child_tracer, argument_1);
			break;
		case __NR_mmap:
		case __NR_mprotect:
		case __NR_brk:
			print_pointer(argument_1);
			break;
		default:
			print_default(argument_1);
			break;
	}
}

void pretty_print::print_syscall_argument_2(tracer& child_tracer) {
	long syscall_number = child_tracer.get_syscall_number();
	long argument_2 = child_tracer.get_syscall_argument(1);
	switch(syscall_number) {
		case __NR_execveat:
		case __NR_openat:
		case __NR_newfstatat:
		case __NR_faccessat:
			print_string_pointer(child_tracer, argument_2);
			break;
		case __NR_read:
		case __NR_write:
			print_string_pointer(child_tracer, argument_2, child_tracer.get_syscall_argument(2));
			break;
		case __NR_execve:
#ifdef __NR_stat
		case __NR_stat:
#endif
#ifdef __NR_fstat
		case __NR_fstat:
#endif
#ifdef __NR_lstat
		case __NR_lstat:
#endif
			print_pointer(argument_2);
			break;
		default:
			print_default(argument_2);
			break;
	}
}

void pretty_print::print_syscall_argument_3(tracer& child_tracer) {
	long syscall_number = child_tracer.get_syscall_number();
	long argument_3 = child_tracer.get_syscall_argument(2);
	switch(syscall_number) {
		case __NR_execveat:
			print_pointer(argument_3);
			break;
		default:
			print_default(argument_3);
			break;
	}
}

void pretty_print::print_syscall_argument_4(tracer& child_tracer) {
	long syscall_number = child_tracer.get_syscall_number();
	long argument_4 = child_tracer.get_syscall_argument(3);
	switch(syscall_number) {
		case __NR_execveat:
			print_pointer(argument_4);
			break;
		default:
			print_default(argument_4);
			break;
	}
}

void pretty_print::print_syscall_exit(tracer &child_tracer, long syscall_number) {
	out << " = ";
	long return_value = child_tracer.get_syscall_return_value();
	switch(syscall_number) {
		case __NR_brk:
		case __NR_mmap:
			print_pointer(return_value);
			break;
		default:
			print_default(return_value);
			break;
	}
	out << "\n";
}

void pretty_print::print_default(long arg) {
	out << std::dec << arg;
}

void pretty_print::print_pointer(long arg) {
	if(arg == 0) {
		out << "NULL";
	} else {
		out << "0x" << std::hex << arg;
	}
}

void pretty_print::print_string_pointer(tracer& child_tracer, long arg, size_t max_length) {
	if(arg == 0) {
		out << "NULL";
	} else {
		std::ostringstream temp_out;
		try {
			temp_out << '"';
			char read_word[4] = {};
			bool cut_off = true;
			for(size_t i = 0; i < max_length; i++) {
				*(long *)read_word = child_tracer.read_word((void *)(arg + i*4));
				int j = 0;
				for(; j < 4 && read_word[j] != 0; j++) {
					temp_out << read_word[j];
				}
				if(read_word[j] == 0) {
					// Reached NULL terminator
					cut_off = false;
					break;
				}
			}
			if(cut_off) {
				temp_out << "...";
			}
			temp_out << '"';
		} catch(const tracer_exception& e) {
			// If we cannot read memory successfully, just print the pointer address.
			out << "0x" << std::hex << arg;
			return;
		}
		// Only print to stdout if reading memory did not fail
		out << temp_out.str();
	}
}

void pretty_print::print_string_pointer_pointer(tracer& child_tracer, long arg, size_t max_length) {
	if(arg == 0) {
		out << "NULL";
	} else {
		long pointer = 0;
		try {
			pointer = child_tracer.read_word((void *)arg);
		} catch(const tracer_exception& e) {
			out << "0x" << std::hex << arg; 
		}
		print_string_pointer(child_tracer, pointer, max_length);
	}
}