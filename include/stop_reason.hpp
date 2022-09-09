#pragma once
#include <sys/ptrace.h>

/**
 * @brief The stop_reason is used to indicate why a tracee is currently
 * stopped after a `wait()`, or to command how long to resume the 
 * variant for when using `resume()`. 
 * 
 * The stop reasons form a partial order. 'Stop reason a subsumes stop
 * reason b' means that, if the variant stopped for reason 'a', it would
 * have also stopped if reason 'b' was given to the resume request. For 
 * example, when a variant stops due to entering a system call, it would
 * have also stopped due to a single step instruction.
 */
enum stop_reason { 
	EXITED,         // The tracee has terminated execution
	FORKED,         // Spawned a child, either through fork, vfork or clone
	SYSCALL_ENTRY,  // The tracee is about to transfer to the kernel for a system call
	SYSCALL_EXIT,   // The kernel is about to transfer control back to the tracee after a system call
	SIGNALED,       // Tracer intercepted a signal to be sent to tracee
	STEPPED,        // Tracee executed a single instruction
	NOT_STOPPED,    // The tracee is currently running
};

/**
 * @brief Translate the given `status` returned from wait into a stop_reason. 
 * It is the caller's responsibility to keep track of the `in_syscall` switch.
 * 
 * Returns NOT_STOPPED if the given wait status is not handled.
 */
enum stop_reason stop_reason_for_wait_status(int status, bool in_syscall=false);

/**
 * @brief Assuming you will issue the given ptrace request next, this returns
 * the soonest next stop reason you can expect from a wait call. The actually
 * observed stop reason may subsume the return value given here, i.e. if this
 * returns a, you may observe b, but it will always hold that a<b according to
 * operator<. If the ptrace request is not a resume request, NOT_STOPPED
 * is returned.
 */
enum stop_reason stop_reason_for_ptrace_request(enum __ptrace_request request, bool in_syscall=false);

/**
 * @brief Return a conservative ptrace resume request for the given stop reason,
 * i.e. if we resume ptrace with the given stop reason, we will *always* catch
 * the next stop for the reason given in argument, but we might also stop
 * earlier.
 * 
 * @param reason 
 * @return enum __ptrace_request 
 */
enum __ptrace_request ptrace_request_for_stop_reason(enum stop_reason reason);

/**
 * @brief Defines the partial order on stop_reasons.
 */
bool operator<(enum stop_reason a, enum stop_reason b);
