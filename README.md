# The LLVM Compiler Infrastructure

This directory and its sub-directories contain source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and run-time environments.

The README briefly describes how to get started with building LLVM.
For more information on how to contribute to the LLVM project, please
take a look at the
[Contributing to LLVM](https://llvm.org/docs/Contributing.html) guide.

## Getting Started with the LLVM System

Taken from https://llvm.org/docs/GettingStarted.html.

### Overview

Welcome to the LLVM project!

The LLVM project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files.  Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer.  It also contains basic regression tests.

C-like languages use the [Clang](http://clang.llvm.org/) front end.  This
component compiles C, C++, Objective-C, and Objective-C++ code into LLVM bitcode
-- and from there into object files, using LLVM.

Other components include:
the [libc++ C++ standard library](https://libcxx.llvm.org),
the [LLD linker](https://lld.llvm.org), and more.

### Getting the Source Code and Building LLVM

The LLVM Getting Started documentation may be out of date.  The [Clang
Getting Started](http://clang.llvm.org/get_started.html) page might have more
accurate information.

This is an example work-flow and configuration to get and build the LLVM source:

1. Checkout LLVM (including related sub-projects like Clang):

     * ``git clone https://github.com/llvm/llvm-project.git``

     * Or, on windows, ``git clone --config core.autocrlf=false
    https://github.com/llvm/llvm-project.git``

2. Configure and build LLVM and Clang:

     * ``cd llvm-project``

     * ``cmake -S llvm -B build -G <generator> [options]``

        Some common build system generators are:

        * ``Ninja`` --- for generating [Ninja](https://ninja-build.org)
          build files. Most llvm developers use Ninja.
        * ``Unix Makefiles`` --- for generating make-compatible parallel makefiles.
        * ``Visual Studio`` --- for generating Visual Studio projects and
          solutions.
        * ``Xcode`` --- for generating Xcode projects.

        Some common options:

        * ``-DLLVM_ENABLE_PROJECTS='...'`` --- semicolon-separated list of the LLVM
          sub-projects you'd like to additionally build. Can include any of: clang,
          clang-tools-extra, compiler-rt,cross-project-tests, flang, libc, libclc,
          libcxx, libcxxabi, libunwind, lld, lldb, mlir, openmp, polly, or pstl.

          For example, to build LLVM, Clang, libcxx, and libcxxabi, use
          ``-DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi"``.

        * ``-DCMAKE_INSTALL_PREFIX=directory`` --- Specify for *directory* the full
          path name of where you want the LLVM tools and libraries to be installed
          (default ``/usr/local``).

        * ``-DCMAKE_BUILD_TYPE=type`` --- Valid options for *type* are Debug,
          Release, RelWithDebInfo, and MinSizeRel. Default is Debug.

        * ``-DLLVM_ENABLE_ASSERTIONS=On`` --- Compile with assertion checks enabled
          (default is Yes for Debug builds, No for all other build types).

      * ``cmake --build build [-- [options] <target>]`` or your build system specified above
        directly.

        * The default target (i.e. ``ninja`` or ``make``) will build all of LLVM.

        * The ``check-all`` target (i.e. ``ninja check-all``) will run the
          regression tests to ensure everything is in working order.

        * CMake will generate targets for each tool and library, and most
          LLVM sub-projects generate their own ``check-<project>`` target.

        * Running a serial build will be **slow**.  To improve speed, try running a
          parallel build.  That's done by default in Ninja; for ``make``, use the option
          ``-j NNN``, where ``NNN`` is the number of parallel jobs, e.g. the number of
          CPUs you have.

      * For more information see [CMake](https://llvm.org/docs/CMake.html)
  
Consult the
[Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html#getting-started-with-llvm)
page for detailed information on configuring and compiling LLVM. You can visit
[Directory Layout](https://llvm.org/docs/GettingStarted.html#directory-layout)
to learn about the layout of the source code tree.
  
## T-Tex

Clone the repository and switch to branch [ttex_update](https://github.com/mittalswastik/ttex_implementation/tree/ttex_update)
  
### T-Tex Setup: configuring the file paths in the T-Tex pass to sync with T-Tex runtime profiler

T-Tex is a multiphase security model. Execution time analysis is conveyed via the profiler to the llvm pass using binary files.

1. Modify the file path in the T-Tex pass [code](https://github.com/mittalswastik/ttex_implementation/blob/ttex_update/llvm/lib/Transforms/Scalar/TtexUpdate_2.cpp)
   * [path of the file modified by the profiler](https://github.com/mittalswastik/ttex_implementation/blob/d85b8bf9cf635960661d5cccce35c339d75316e6/llvm/lib/Transforms/Scalar/TtexUpdate_2.cpp#L1462) to any comfortable path.
       * This is the file updated by the profiler after execution time analysis which is read by the llvm pass during subsequent phases of compilation
   * [path of file modified by the compiler](https://github.com/mittalswastik/ttex_implementation/blob/d85b8bf9cf635960661d5cccce35c339d75316e6/llvm/lib/Transforms/Scalar/TtexUpdate_2.cpp#L1939)
  
### T-Tex Setup: Installing Llvm/Clang with T-Tex security

1. Clone the respository
2. Modify the BUILD_PATH (according to your LLVM cloned path) in ["scr" script](https://github.com/mittalswastik/ttex_implementation/blob/ttex_update/scr)
   * Eg: If cloned repository is /home/ttex_implementation - then BUILD_PATH is /home/ttex_implementation/llvm-project/build
   * command [``sudo ninja -j4``](https://github.com/mittalswastik/ttex_implementation/blob/39e2e21eb371cf1a7e0d9d9c41dc87b3714daf3c/scr#L18) can be modified to ``sudo ninja -jn`` where n is the number of threads compiling llvm (Higher n results in faster build but slower linking)
4. execute
   * ``chmod +x scr``
   * ``./scr``
   * Compilation error generally suggest missing packages which will require explicit installation.
  
#### Follow [ttex_benchmark](https://github.ncsu.edu/smittal6/ttex_benchmark) for Testing
