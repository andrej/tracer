#pragma once
#include <iostream> // std::ostream
#include "tracer.hpp"

/**
 * @brief Pretty printing functionality for system call names and arguments.
 * 
 * We wrap this in a class to easily be able to swap out the output stream
 * (if we want to print to stdout instead of stderr, for example).
 * 
 * This is only a small subset of the functionality of the real `strace`; we
 * show how, for a couple of common system calls, we can use the tracer
 * architecture to read system call names, numbers and arguments, as well as
 * memory, and format it meaningfully based on system call argument type 
 * documentation.
 */
struct pretty_print {

	static std::ostream& out;  // strace prints to stderr by default

	static int get_syscall_n_arguments(long syscall_number);

	static void print_syscall_entry(tracer &child_tracer, long syscall_number);

	static void print_syscall_exit(tracer &child_tracer, long syscall_number);

	static void print_syscall_argument_1(tracer &child_tracer);

	static void print_syscall_argument_2(tracer &child_tracer);

	static void print_syscall_argument_3(tracer &child_tracer);

	static void print_syscall_argument_4(tracer &child_tracer);

	static void print_default(long arg);

	static void print_pointer(long arg);

	static void print_string_pointer(tracer &child_tracer, long arg, size_t max_length = 32);

	static void print_string_pointer_pointer(tracer &child_tracer, long arg, size_t max_length = 32);

};