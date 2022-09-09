# `libtracer` -- A lightweight idiomatic C++ wrapper around Linux `ptrace`

[ptrace][1] is a powerful tool for interacting with executing processes.
Unfortunately, it exposes a range of idiosyncracies across different
architectures, and requires an unsightly amount of boilerplate code for
even simple tasks. This library aims to address these issues with the goal
of ideally not adding any overhead to most use cases.

*`libtracer` is still in alpha stage. Any bug reports, feature requests or
contributions [are welcomed][2].*

This library currently...

- ...performs error handling to produce C++ exceptions when something goes 
  wrong.
- ...provides clean, easy-to-understand `stop_reason`s, such as `SYSCALL_ENTRY`
  or `EXITED` whenever the child process stops, so you no longer have to compare
  the status of `waitpid()` with weird values like `SIGTRAP | 0x80`.
- ...provides convenient functions to resume your process until it has certainly
  stopped for your desired reason. For example, to block until the child process
  is entering a system call, simply call 
   `resume_and_wait(stop_reason::SYSCALL_ENTRY)`.
- ...abstracts away idiosyncracies in accessing registers, by providing
  `read_registers()` and `write_registers()` methods.
- ...abstracts away idiosyncracies about where system call information is stored
  by providing methods like `get_syscall_number()` or 
  `set_syscall_return_value()`.
- ...provides string names for all system calls on your architecture.

Planned features include...

- ...easier-to-use memory read/write functions at finer and larger granularities 
  than word size.
- ...swapping the backend of libtracer for something non-ptrace based, such as
  eBPF.
- ...system call canonicalization; get architecture-independent libtracer-
  specific "system call numbers" and arguments that can be exchanged between
  machines of different architectures.


## Currently Supported Architectures

- `x86_64` aka `AMD64`
- `ARM64` aka `Aarch64`


## Examples

### "Hello World"

In the general case, a simple program using `libtracer` will fork a child
process to be traced, or attach to one, and then continuously loop over
tracee events while calling the `resume()` or `resume_and_wait()` functions.

For example, the following is one of the simplest conceivable tracer programs.

    #include <iostream>
    #include "tracer.hpp"

    tracer the_tracer;

    int main(int argc, char **argv) {
      if(the_tracer.fork() == 0) {
        std::cout << "Hello, World!";
      } else {
        while(the_tracer.resume_and_wait(stop_reason::SYSCALL_ENTRY)) {
          std::cout << the_tracer.get_syscall_name() << "\n";
        }
      }
    }

This will print something like:

    fstat
    write
    Hello, World!
    exit_group 

To stop at every signal instead, use `stop_reason::SIGNALED` as the argument
to `resume_and_wait()`. These calls may throw a `tracer_exception`, that you
might want to handle as well. You do not have to use the `tracer`-provided
`fork()` function; that is just for convenience.

A only very slightly more advanced example is included in 
`examples/hello_world`. 

### strace clone

The `libtracer` source ships with a `strace`-like program that showcases some
of the tracer's functionality. If you plan to use `libtracer` yourself, looking
at the source in `examples/strace/strace.cpp` and 
`examples/strace/pretty_printing.cpp` might be a good starting point
to see how this application makes use of `libtracer`.


## Installation

Building is as simple as it gets:

    cd tracer
    make
  
After this, the `libtracer.so` library will be in `install/lib`, as well as 
some example programs in `install/bin`.

To use the library in your own program thereafter, just link with `libtracer.so`
and include the headers from `include/`.

It should also be fairly straightforward to compile and link `libtracer` into
your project directly. No special compilation flags are needed for `libtracer`,
so you can just link your program with the object files in `build/` as well.


## Credits / License

This library is free to use under the three clause BSD license.

If your published academic project uses `libtracer`, please consider crediting
me as follows:

    Andre Rösti. "libtracer - A Lightweight Idiomatic C++ Wrapper Around Linux ptrace", GitHub repository, 2022. [https://github.com/andrej/libtracer]. 

[1]: https://man7.org/linux/man-pages/man2/ptrace.2.html
[2]: https://github.com/andrej/tracer
