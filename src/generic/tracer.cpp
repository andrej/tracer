#include <unistd.h>     // fork
#include <sys/wait.h>   // waitpid
#include <sys/signal.h> // kill, SIGSTOP
#include <sys/ptrace.h> // enum __ptrace_request
#include <cerrno>       // errno
#include <cstring>      // strerror
#include <vector> 
#include "tracer.hpp"

tracer::tracer()
{
}

tracer::tracer(pid_t pid)
{
	tracee.process_id = pid;
}

void tracer::_set_options(bool trace_children) {
	long ptrace_options = 0;
	//ptrace_options |= PTRACE_O_EXITKILL;
	ptrace_options |= PTRACE_O_TRACESYSGOOD;
	if(trace_children) {
	       ptrace_options |= PTRACE_O_TRACEFORK;
	       ptrace_options |= PTRACE_O_TRACEVFORK;
	       ptrace_options |= PTRACE_O_TRACECLONE;
	}
	if(ptrace(PTRACE_SETOPTIONS, tracee.process_id, 0, ptrace_options) != 0) {
		throw tracer_exception("could not set ptrace options: " + std::to_string(errno) + " " + std::string(strerror(errno)));
	}
}

void tracer::_handle_fork() {
	// Expected to be called immediately after a PTRACE_EVENT_FORK/VFORK/CLONE
	tracer_ensure_invariants();
	if(tracee.stop_reason != FORKED) {
		throw tracer_exception("handle_fork may only be called with tracee stopped immediately after a fork.");
	}
	pid_t spawned_process_id = -1;
	if(ptrace(PTRACE_GETEVENTMSG, tracee.process_id, 0, &spawned_process_id) == -1) {
		throw tracer_exception("Unable to obtain forked child process id: " + std::string(strerror(errno)));
	}
	_children.emplace_back(spawned_process_id);
	tracer& child_tracer = _children.back();
	child_tracer._await_sigstop();
}

void tracer::_await_sigstop() {
	tracer_ensure_invariants();
	/* Explanation for following vector:
	   From man ptrace, Notes "Attaching and detaching": Note
	   that if other signals are concurrently sent to this thread during
	   attach, the tracer may see the tracee enter signal-delivery-stop
	   with other signal(s) first!  The usual practice is to reinject
	   these signals until SIGSTOP is seen, then suppress SIGSTOP
	   injection. */
	std::vector<int> pending_signals;
	tracee.stop_reason = NOT_STOPPED;
	// Try to wait for raised SIGSTOP in above child.
	do {
		enum stop_reason stop = wait();
		if(stop != SIGNALED) {
			throw tracer_exception("Child stopped for unexpected reason " + std::to_string(stop) + 
			                       " (status " + std::to_string(tracee.status) + ") during attach.");
		}
		if(WSTOPSIG(tracee.status) != SIGSTOP) {
			pending_signals.push_back(WSTOPSIG(tracee.status));
			resume(SIGNALED);  // resume until we see SIGSTOP
		}
	} while(WSTOPSIG(tracee.status) != SIGSTOP);
	// Reinject signals we observed waiting for our SIGSTOP.
	for(int signal : pending_signals) {
		kill(tracee.process_id, signal);
	}
	// After all this, tracee should be in SIGNALED stop state, having
	// just received the raised SIGSTOP from above.
}


pid_t tracer::fork() {
	if(tracee.process_id != -1) {
		throw tracer_exception("Cannot fork; the tracer is already attached to a child.");
	}
	pid_t child = ::fork();
	if(child == 0) {
		if(ptrace(PTRACE_TRACEME, 0, 0, 0) != 0) {
			throw tracer_exception("Unable to accept tracing in child " + std::to_string(getpid()) + ".");
			// TODO Should we notify the parent? Can we?
		}
		raise(SIGSTOP);
		return 0;
		// Unreachable
	} else {
		tracee.process_id = child;
		_await_sigstop();
		_set_options(false);
		return child;
	}
}

void tracer::attach(pid_t pid) {
	if(ptrace(PTRACE_ATTACH, pid, 0, 0) != 0) {
		throw tracer_exception("Unable to attach to " + std::to_string(pid) + 
		                       ": " + std::to_string(errno) + " " + std::string(strerror(errno)));
	}
	_set_options(false);
	tracee.process_id = pid;
	_await_sigstop();
}

void tracer::resume(enum stop_reason until) {
	tracer_ensure_invariants();
	if(tracee.stop_reason == NOT_STOPPED) {
		throw tracer_exception("Cannot `resume` a tracee that is not currently stopped.");
	}
	if(until == NOT_STOPPED) {
		throw tracer_exception("`resume` can not be called with a `NOT_STOPPED` until argument.");
	}
	const enum __ptrace_request ptrace_request = ptrace_request_for_stop_reason(until);
	tracee.registers_valid = false;
	tracee.stop_reason = NOT_STOPPED;
	ptrace(ptrace_request, tracee.process_id, 0, 0);
}

enum stop_reason tracer::wait() {
	tracer_ensure_invariants();
	if(tracee.stop_reason != NOT_STOPPED) {
		throw tracer_exception("Cannot `wait` for a tracee that is already stopped.");
	}
	int status = 0;
	int wait_return = -1;
	do {  // Retry `waitpid` if interrupted by signal
		wait_return = waitpid(tracee.process_id, &status, 0);
	} while(wait_return != tracee.process_id && errno == EINTR);
	if(wait_return != tracee.process_id) {
		// Must be either ECHILD or EINVAL
		if(errno == ECHILD) {
			if(tracee.stop_reason != EXITED) {
				throw tracer_exception("No tracee " + std::to_string(tracee.process_id) + ", or not a "
				                       "child of this process, and no exit of tracee was observed "
						       "through tracer class.");
			} else {
				return EXITED;
			}
		}
		throw tracer_exception("waitpid returned unexpected error " + std::string(strerror(errno)));
	}
	const enum stop_reason stop_reason = stop_reason_for_wait_status(status, tracee.in_syscall);
	if(stop_reason == NOT_STOPPED) {
		throw tracer_exception("Unknown/unhandled stop reason: " + std::to_string(status));
	}
	tracee.status = status;
	tracee.stop_reason = stop_reason;
	if(tracee.stop_reason == SYSCALL_ENTRY || tracee.stop_reason == SYSCALL_EXIT) {
		tracee.in_syscall = !tracee.in_syscall;
	} else if(tracee.stop_reason == FORKED) {
		_handle_fork();
	}
	return tracee.stop_reason;
}

bool tracer::resume_and_wait(enum stop_reason until, int intermediate_stops) {
	tracer_ensure_invariants();
	int stops = 0;
	do {
		resume(until);
		wait();
		stops++;
	} while(tracee.stop_reason != until 
	        && tracee.stop_reason != EXITED 
		&& (intermediate_stops == -1 || intermediate_stops >= stops));
	return tracee.stop_reason == until;
}

const struct user_regs_struct& tracer::read_registers() {
	tracer_ensure_invariants();
	if(tracee.registers_valid) {
		return tracee.registers;
	}
	if(_read_registers_internal(tracee.process_id, tracee.registers) != 0) {
		tracee.registers_valid = false;
		throw tracer_exception("Could not read registers: " + std::string(strerror(errno)));
	}
	tracee.registers_valid = true;
	return tracee.registers;
}

void tracer::write_registers(const struct user_regs_struct& new_registers) {
	tracer_ensure_invariants();
	if(_write_registers_internal(tracee.process_id, new_registers) != 0) {
		tracee.registers_valid = false;
		throw tracer_exception("Could not write registers: " + std::string(strerror(errno)));
	}
	tracee.registers = new_registers;
	tracee.registers_valid = true;
}

long tracer::read_word(void *offset) {
	tracer_ensure_invariants();
	errno = 0;
	long ret = ptrace(PTRACE_PEEKDATA, tracee.process_id, offset, 0);
	if(errno != 0) {
		throw tracer_exception("Unable to peek data at " + std::to_string((long)offset) + ": " + std::to_string(errno) + " " + std::string(strerror(errno)));
	}
	return ret;
}

void tracer::write_word(void *offset, long value) {
	tracer_ensure_invariants();
	if(ptrace(PTRACE_POKEDATA, tracee.process_id, offset, value) != 0) {
		throw tracer_exception("Unable to poke data at " + std::to_string((long)offset) + ": " + std::to_string(errno) + " " + std::string(strerror(errno)));
	}
}

std::string tracer::get_syscall_name() {
	tracer_ensure_invariants();
	long number = get_syscall_number();
	return syscall_name_by_number(number);
}

std::string tracer::syscall_name_by_number(long number, std::string default_name) {
	if(number < 0 || number > max_syscall_number) {
		return default_name;
	}
	const char *out = syscall_names[number];
	if(out == NULL) {
		return default_name;
	}
	return std::string(out);
}
