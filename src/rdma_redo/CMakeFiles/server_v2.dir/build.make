# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /users/nvarsha/CSCI5572-AOS/src/rdma_redo

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /users/nvarsha/CSCI5572-AOS/src/rdma_redo

# Include any dependencies generated for this target.
include CMakeFiles/server_v2.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/server_v2.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/server_v2.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/server_v2.dir/flags.make

CMakeFiles/server_v2.dir/utils.c.o: CMakeFiles/server_v2.dir/flags.make
CMakeFiles/server_v2.dir/utils.c.o: utils.c
CMakeFiles/server_v2.dir/utils.c.o: CMakeFiles/server_v2.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/server_v2.dir/utils.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/server_v2.dir/utils.c.o -MF CMakeFiles/server_v2.dir/utils.c.o.d -o CMakeFiles/server_v2.dir/utils.c.o -c /users/nvarsha/CSCI5572-AOS/src/rdma_redo/utils.c

CMakeFiles/server_v2.dir/utils.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/server_v2.dir/utils.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/nvarsha/CSCI5572-AOS/src/rdma_redo/utils.c > CMakeFiles/server_v2.dir/utils.c.i

CMakeFiles/server_v2.dir/utils.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/server_v2.dir/utils.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/nvarsha/CSCI5572-AOS/src/rdma_redo/utils.c -o CMakeFiles/server_v2.dir/utils.c.s

CMakeFiles/server_v2.dir/server_v2.c.o: CMakeFiles/server_v2.dir/flags.make
CMakeFiles/server_v2.dir/server_v2.c.o: server_v2.c
CMakeFiles/server_v2.dir/server_v2.c.o: CMakeFiles/server_v2.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/server_v2.dir/server_v2.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/server_v2.dir/server_v2.c.o -MF CMakeFiles/server_v2.dir/server_v2.c.o.d -o CMakeFiles/server_v2.dir/server_v2.c.o -c /users/nvarsha/CSCI5572-AOS/src/rdma_redo/server_v2.c

CMakeFiles/server_v2.dir/server_v2.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/server_v2.dir/server_v2.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/nvarsha/CSCI5572-AOS/src/rdma_redo/server_v2.c > CMakeFiles/server_v2.dir/server_v2.c.i

CMakeFiles/server_v2.dir/server_v2.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/server_v2.dir/server_v2.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/nvarsha/CSCI5572-AOS/src/rdma_redo/server_v2.c -o CMakeFiles/server_v2.dir/server_v2.c.s

# Object files for target server_v2
server_v2_OBJECTS = \
"CMakeFiles/server_v2.dir/utils.c.o" \
"CMakeFiles/server_v2.dir/server_v2.c.o"

# External object files for target server_v2
server_v2_EXTERNAL_OBJECTS =

server_v2: CMakeFiles/server_v2.dir/utils.c.o
server_v2: CMakeFiles/server_v2.dir/server_v2.c.o
server_v2: CMakeFiles/server_v2.dir/build.make
server_v2: /usr/lib/x86_64-linux-gnu/libibverbs.so
server_v2: /usr/lib/x86_64-linux-gnu/librdmacm.so
server_v2: /usr/local/lib/libhiredis.so
server_v2: CMakeFiles/server_v2.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable server_v2"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/server_v2.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/server_v2.dir/build: server_v2
.PHONY : CMakeFiles/server_v2.dir/build

CMakeFiles/server_v2.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/server_v2.dir/cmake_clean.cmake
.PHONY : CMakeFiles/server_v2.dir/clean

CMakeFiles/server_v2.dir/depend:
	cd /users/nvarsha/CSCI5572-AOS/src/rdma_redo && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles/server_v2.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/server_v2.dir/depend

