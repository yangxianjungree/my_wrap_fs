cmake_minimum_required(VERSION 3.15)
project(my_wrap_fs C)

set(CMAKE_C_STANDARD 11)

execute_process(COMMAND uname -r COMMAND tr -d '\n'
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		OUTPUT_VARIABLE out_release)


# string(REPLACE "\n" "" release ${out_release})
set(linux_include_path "/lib/modules/${out_release}/build")
message("linux source headers' root path is: ${linux_include_path}")

# Include linux's header
include_directories(${linux_include_path})
include_directories(${linux_include_path}/arch/x86/include)
include_directories(${linux_include_path}/include)
include_directories(${linux_include_path}/include/linux)


add_subdirectory(001_module_init)