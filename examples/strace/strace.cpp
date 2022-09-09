#include <iomanip>   // std::hex, std::dec
#include <iostream>  // std::cout
#include <sstream>   // ostringstream
#include <stdlib.h>  // exit
#include <unistd.h>  // execvp
#include <errno.h>   // errno
#include <string.h>  // strerror
#include <sys/syscall.h>  // syscall numbers
#include "tracer.hpp"
#include "pretty_printing.hpp"

char **command_argv = NULL;
tracer child_tracer;
bool called_exit_group = false;
long exit_code = 0;
std::ostream& out = std::cerr; // strace prints to stderr
std::ostream& errout = std::cerr;

void parse_args(int argc, char **argv);

void exec_command(char **command_argv);

int main(int argc, char **argv) {
	parse_args(argc, argv);
	try {
		if(child_tracer.fork() == 0) {
			exec_command(command_argv);
		}
		while(1) {
			if(!child_tracer.resume_and_wait(stop_reason::SYSCALL_ENTRY)) {
				errout << "Program exited without calling exit()\n";
				break;
			}
			long syscall_number = child_tracer.get_syscall_number();
			if(syscall_number == __NR_exit || syscall_number == __NR_exit_group) {
				called_exit_group = true;
				exit_code = child_tracer.get_syscall_argument(0);
			}
			pretty_print::print_syscall_entry(child_tracer, syscall_number);
			if(!child_tracer.resume_and_wait(stop_reason::SYSCALL_EXIT)) {
				if(called_exit_group) {
					out << " = ?\n+++ exited with " + std::to_string(exit_code) + " +++\n";
					break;
				}
				errout << "Program exited unexpectedly before completing system call.\n";
				break;
			}
			pretty_print::print_syscall_exit(child_tracer, syscall_number);
		}
	} catch(const tracer_exception& e) {
		errout << std::string("Tracer exception: \n") + e.what();
		exit(1);
	}
	return 0;
}

void parse_args(int argc, char **argv) {
	if(argc < 2) {
		std::string name = "./tracer/install/bin/strace";
		if(argc == 1) {
			name = std::string(argv[0]);
		}
		errout << "Usage: " + std::string(name) + " command\n";
		exit(1);
	}
	command_argv = &argv[1];
}

void exec_command(char **command_argv) {
	if(execvp(command_argv[0], command_argv) == -1) {
		errout << "execvp() failed: " + std::to_string(errno) + " " + std::string(strerror(errno));
		exit(1);
	}
}
