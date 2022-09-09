#include <iostream>
#include <stdlib.h>
#include "tracer.hpp"

tracer the_tracer;

void tracee_process() {
	/* The following will cause a `write()` system call to stdout, which
	   we will observe in the tracer. */
	std::cout << "Hello, World!";
	exit(1);
}

void tracer_process() {
	/* `resume_and_wait` waits for tracee to execute a system call.
	   The argument stop_reason::SYSCALL_ENTRY tells the tracer that it can
	   continue execution of the tracee until it stops because a system call
	   is about to be executed. This will hence step over other stop
	   reasons, such as signals. */
	while(the_tracer.resume_and_wait(stop_reason::SYSCALL_ENTRY)) {
		std::cout << "About to execute system call:\n";
		std::cout << the_tracer.get_syscall_name() << "\n";

		/* Now we wait for the system call to complete in kernel-space. */
		bool exited_syscall = the_tracer.resume_and_wait(stop_reason::SYSCALL_EXIT);
		/* The final `exit_group` system call never returns. */
		if(!exited_syscall) {
			break;
		}
		std::cout << "Return value:\n";
		std::cout << std::to_string(the_tracer.get_syscall_return_value()) << "\n";
	}

	/* resume_and_wait() returns `false` if the tracee exits, which makes
	   it easy to use as a while loop condition. Here, we have exited the
	   loop, hence we know that the tracee has completed execution. */
	std::cout << "Tracee completed execution.\n";
}

int main(int argc, char **argv) {
	if(the_tracer.fork() == 0) {
		tracee_process();
	} else {
		tracer_process();
	}
}