#pragma once
#include <sys/types.h>      // pid_t
#include <sys/user.h>       // struct user_regs_struct
#include <stdexcept>        // std::runtime_error
#include <list>
#include <unordered_set>
#include "stop_reason.hpp"

#define tracer_ensure_invariants() do { \
	if(tracee.process_id == -1) { \
		throw tracer_exception("Illegal call with uninitialized tracee."); \
	} \
} while(0)

class tracer {
private:

	struct tracee {
		pid_t process_id = -1;
		enum stop_reason stop_reason = NOT_STOPPED;
		int status;
		bool in_syscall = false;
		bool registers_valid = false;
		struct user_regs_struct registers;
	};

	struct tracee tracee;

	std::list<tracer> _children;

	void _set_options(bool trace_children);

	void _await_sigstop();

	void _handle_fork();

	static long _read_registers_internal(pid_t pid, struct user_regs_struct& destination);

	static long _write_registers_internal(pid_t pid, const struct user_regs_struct& source);

	static const long max_syscall_number;
	static const char *syscall_names[];

public:

	tracer();

	tracer(pid_t pid);

	/**
	 * @brief Maximum number of arguments a system call can take on the
	 * calling architecture.
	 */
	static const int n_syscall_arguments;

	/**
	 * @brief Fork execution into tracee and tracer, with tracee's pid = 0.
	 * 
	 * This attaches and initializes the tracer in the parent process. In
	 * the child process, the tracer class will not be in a valid state or
	 * usable.
	 * 
	 * In the child (return value 0), you should probably follow this call 
	 * with an execve() call.
	 * 
	 * @return pid_t 
	 */
	pid_t fork();

	/**
	 * @brief Attach to a running process.
	 */
	void attach(pid_t pid);

	/**
	 * @brief Read-only access to a list of tracer objects for children 
	 * forked by this tracer
	 */
	inline std::list<tracer> children() const { return _children; };

	/**
	 * @brief Read-only access to tracee information
	 */
	inline pid_t process_id() const { return tracee.process_id; };
	inline enum stop_reason stop_reason() const { return tracee.stop_reason; };
	inline int status() const { return tracee.status; };
	inline bool in_syscall() const { return tracee.in_syscall; };

	/**
	 * @brief If tracee is stopped, continue its execution. Use `wait` to
	 * await the next stop of the tracee.
	 * 
	 * @param until A hint indicating when the tracee should stop next. This
	 * is merely a hint; a different stop may be observed at the next `wait`
	 * call.
	 */
	void resume(enum stop_reason until);

	/**
	 * @brief If tracee is running, block until its next stop. Return the
	 * reason for the stop.
	 * 
	 * The returned `stop_reason` is not necessarily the stop reason asked
	 * for in a previous `resume` call; the tracee might have been
	 * interrupted for a different reason. Hence it is important to check
	 * the return value, and potentially `resume/wait` in a loop until the
	 * desired stop reason is observed. The `resume_and_wait` function can
	 * do this for you.
	 */
	enum stop_reason wait();

	/**
	 * @brief Resume the tracee repeatedly until it stops for the given
	 * stop_reason `until`, or until it exits. The return value can be used
	 * to distinguish between these two cases.
	 * 
	 * @param until stop reason to wait for
	 * @param intermediate_stops number of intermediate stops that are 
	 * permissible to skip; -1 for any number of intermediate stops; 0 if
	 * the next stop observed must be the desired stop reason
	 * @return true The tracee stopped for the reason `until`
	 * @return false The tracee exited, or the number of intermediate stops
	 * was exhausted, before stopping for the reason `until`
	 */
	bool resume_and_wait(enum stop_reason until, int allow_intermediate_stops=-1);

	/**
	 * @brief Return architecture-specific registers of the tracee; if
	 * needed, a read of the registers is performed. If registers have been
	 * perviously read, and this read has not been invalidated by 
	 * continuation of the tracee, the "cached" register values are 
	 * returned.
	 */
	const struct user_regs_struct& read_registers();
	void write_registers(const struct user_regs_struct& new_registers);

	/**
	 * @brief Return the system call name for the given system call number.
	 * If no system call with the given number is known, the default string
	 * given is returned, which defaults to "unknown".
	 * 
	 * @param i 
	 * @return long 
	 */
	static std::string syscall_name_by_number(long number, std::string default_name = "unknown");

	/**
	 * @brief Reads the architecture-specific register that contains the
	 * system call number upon system call entry. The value returned is
	 * hence only meaningful as a system call number upon a system call 
	 * entry stop.
	 */
	long get_syscall_number();

	void set_syscall_number(long number);

	/**
	 * @brief Return the string name representing the currently executing
	 * system call, if number=-1, or the system call of the given number.
	 * 
	 * @param number 
	 * @return std::string 
	 */
	std::string get_syscall_name();

	long get_syscall_argument(size_t i);
	void set_syscall_argument(size_t i, long value);
	long get_syscall_return_value();
	void set_syscall_return_value(long value);
	long read_word(void *offset);
	void write_word(void *offset, long value);

};

class tracer_exception : public std::runtime_error {
public:
	tracer_exception(const std::string& what) : std::runtime_error(what) {}
};
