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
include CMakeFiles/client_write.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/client_write.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/client_write.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/client_write.dir/flags.make

CMakeFiles/client_write.dir/utils.c.o: CMakeFiles/client_write.dir/flags.make
CMakeFiles/client_write.dir/utils.c.o: utils.c
CMakeFiles/client_write.dir/utils.c.o: CMakeFiles/client_write.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/client_write.dir/utils.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/client_write.dir/utils.c.o -MF CMakeFiles/client_write.dir/utils.c.o.d -o CMakeFiles/client_write.dir/utils.c.o -c /users/nvarsha/CSCI5572-AOS/src/rdma_redo/utils.c

CMakeFiles/client_write.dir/utils.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/client_write.dir/utils.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/nvarsha/CSCI5572-AOS/src/rdma_redo/utils.c > CMakeFiles/client_write.dir/utils.c.i

CMakeFiles/client_write.dir/utils.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/client_write.dir/utils.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/nvarsha/CSCI5572-AOS/src/rdma_redo/utils.c -o CMakeFiles/client_write.dir/utils.c.s

CMakeFiles/client_write.dir/client_write.c.o: CMakeFiles/client_write.dir/flags.make
CMakeFiles/client_write.dir/client_write.c.o: client_write.c
CMakeFiles/client_write.dir/client_write.c.o: CMakeFiles/client_write.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/client_write.dir/client_write.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/client_write.dir/client_write.c.o -MF CMakeFiles/client_write.dir/client_write.c.o.d -o CMakeFiles/client_write.dir/client_write.c.o -c /users/nvarsha/CSCI5572-AOS/src/rdma_redo/client_write.c

CMakeFiles/client_write.dir/client_write.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/client_write.dir/client_write.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/nvarsha/CSCI5572-AOS/src/rdma_redo/client_write.c > CMakeFiles/client_write.dir/client_write.c.i

CMakeFiles/client_write.dir/client_write.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/client_write.dir/client_write.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/nvarsha/CSCI5572-AOS/src/rdma_redo/client_write.c -o CMakeFiles/client_write.dir/client_write.c.s

# Object files for target client_write
client_write_OBJECTS = \
"CMakeFiles/client_write.dir/utils.c.o" \
"CMakeFiles/client_write.dir/client_write.c.o"

# External object files for target client_write
client_write_EXTERNAL_OBJECTS =

client_write: CMakeFiles/client_write.dir/utils.c.o
client_write: CMakeFiles/client_write.dir/client_write.c.o
client_write: CMakeFiles/client_write.dir/build.make
client_write: /usr/lib/x86_64-linux-gnu/libibverbs.so
client_write: /usr/lib/x86_64-linux-gnu/librdmacm.so
client_write: /usr/local/lib/libhiredis.so
client_write: CMakeFiles/client_write.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable client_write"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/client_write.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/client_write.dir/build: client_write
.PHONY : CMakeFiles/client_write.dir/build

CMakeFiles/client_write.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/client_write.dir/cmake_clean.cmake
.PHONY : CMakeFiles/client_write.dir/clean

CMakeFiles/client_write.dir/depend:
	cd /users/nvarsha/CSCI5572-AOS/src/rdma_redo && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo /users/nvarsha/CSCI5572-AOS/src/rdma_redo/CMakeFiles/client_write.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/client_write.dir/depend

