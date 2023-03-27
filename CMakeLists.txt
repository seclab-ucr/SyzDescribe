cmake_minimum_required(VERSION 3.4.3)
project(SyzDescribe CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-O3 -pthread -Wno-unused-command-line-argument")

# Use the llvm-config binary to get the information needed.
# Try to detect it in the user's environment.
# The user can force a particular binary by passing `-DLLVM_CONFIG_BINARY=/path/to/llvm-config` to CMake.
find_program(LLVM_CONFIG_BINARY NAMES llvm-config)
message(STATUS "LLVM_CONFIG_BINARY: ${LLVM_CONFIG_BINARY}")

function(_run_llvm_config output_var)
    set(_command "${LLVM_CONFIG_BINARY}" ${ARGN})
    execute_process(COMMAND ${_command}
            RESULT_VARIABLE _exit_code
            OUTPUT_VARIABLE ${output_var}
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            )
    if (NOT ("${_exit_code}" EQUAL "0"))
        message(FATAL_ERROR "Failed running ${_command}")
    endif ()
    set(${output_var} ${${output_var}} PARENT_SCOPE)
endfunction()

_run_llvm_config(_llvm_cxx_flags "--cxxflags")
_run_llvm_config(_llvm_ld_flags "--ldflags")
_run_llvm_config(_llvm_libs "--libs")
_run_llvm_config(_llvm_system_libs "--system-libs")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_llvm_cxx_flags} ${_llvm_ld_flags} ${_llvm_libs} ${_llvm_system_libs}")

message(STATUS "_llvm_cxx_flags: ${_llvm_cxx_flags}")
message(STATUS "_llvm_ld_flags: ${_llvm_ld_flags}")
message(STATUS "_llvm_libs: ${_llvm_libs}")
message(STATUS "_llvm_system_libs: ${_llvm_system_libs}")

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(YOUR_PKG REQUIRED IMPORTED_TARGET grpc grpc++ protobuf)

add_subdirectory(lib)
add_subdirectory(tools)