#include <stdexcept>
#include <sys/wait.h>       // WIFEXITED
#include "stop_reason.hpp"

enum stop_reason stop_reason_for_wait_status(int status, bool in_syscall) {
	if(WIFEXITED(status)) { // process terminated regularly
		return EXITED;
	} else if(WIFSIGNALED(status)) {  // process terminated by unhandled signal
		return EXITED;
	} else if(WIFSTOPPED(status)) {
		if(WSTOPSIG(status) == (SIGTRAP | 0x80)) {  // Syscall Stop
			/* from man syscall, section "Syscall-stops":
			   That is, signal-delivery-stop never happens between 
			   syscall-enter-stop and syscall-exit-stop; it happens 
			   after syscall-exit-stop. */
			return (in_syscall ? SYSCALL_EXIT : SYSCALL_ENTRY);
		} else if(WSTOPSIG(status) == SIGTRAP) {
			const int ptrace_event = (((status>>8) & 0xffff) & ~SIGTRAP) >> 8;
			switch(ptrace_event) {
				case 0:  // nothing set in high byte, i.e. just a regular trap
					return SIGNALED;
				case PTRACE_EVENT_FORK:
				case PTRACE_EVENT_VFORK:
				case PTRACE_EVENT_CLONE:
				        /* From man ptrace, Options, PTRACE_O_TRACEFORK:
					   Stop the tracee at the next fork(2) 
					   and automatically start tracing the 
					   newly cloned process, which will 
					   start with a SIGSTOP, or 
					   PTRACE_EVENT_STOP if PTRACE_SEIZE 
					   was used. */
					return FORKED;
				default:
					return NOT_STOPPED;
			}
		} else {
			return SIGNALED;
		}
	}
	return NOT_STOPPED;
}

enum stop_reason stop_reason_for_ptrace_request(enum __ptrace_request request, bool in_syscall) {
	switch(request) {
		case PTRACE_SINGLESTEP:
			return STEPPED;
		case PTRACE_SYSCALL:
			return (in_syscall ? SYSCALL_EXIT : SYSCALL_ENTRY);
		case PTRACE_CONT:
			return SIGNALED;
		default:
			return NOT_STOPPED;
	}
}

enum __ptrace_request ptrace_request_for_stop_reason(enum stop_reason until) {
	switch(until) {
		case STEPPED:
			return PTRACE_SINGLESTEP;
		case SYSCALL_ENTRY:
		case SYSCALL_EXIT:
			return PTRACE_SYSCALL;
		case SIGNALED:
		case EXITED:
			return PTRACE_CONT;
		case NOT_STOPPED:  // makes no sense
		default:
			return PTRACE_DETACH;
	}
}

bool operator<(enum stop_reason a, enum stop_reason b) {
	/* Return true if, stop reason a subsumes stop reason b, i.e.
	   "if it stopped for b, it would have also stopped for a,
	   and b =/= a."

	   Another intuition: a is generally a shorter interval stop than b,
	   e.g. STEPPED is always a shorter or equal inerval as SYSCALL.

	   EXITED
	      |
	   STEPPED

	          FORKED
	            |
	   SYSCALL_ENTRY / SYSCALL_EXIT
	            |
	        SIGNALED
	            |
	         STEPPED
	*/
	switch(b) {
		case EXITED:
			return a == STEPPED;
		case FORKED:
			return a == SYSCALL_ENTRY || a == SYSCALL_EXIT || a == SIGNALED || a == STEPPED;
		case SYSCALL_ENTRY:
		case SYSCALL_EXIT:
			return a == SIGNALED || a == STEPPED;
		case SIGNALED:
			return a == STEPPED;
		default:
			return false;
	}
}
