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
CMAKE_SOURCE_DIR = /users/nvarsha/CSCI5572-AOS/rdma_new

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /users/nvarsha/CSCI5572-AOS/rdma_new

# Include any dependencies generated for this target.
include CMakeFiles/client.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/client.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/client.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/client.dir/flags.make

CMakeFiles/client.dir/utils.c.o: CMakeFiles/client.dir/flags.make
CMakeFiles/client.dir/utils.c.o: utils.c
CMakeFiles/client.dir/utils.c.o: CMakeFiles/client.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/nvarsha/CSCI5572-AOS/rdma_new/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/client.dir/utils.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/client.dir/utils.c.o -MF CMakeFiles/client.dir/utils.c.o.d -o CMakeFiles/client.dir/utils.c.o -c /users/nvarsha/CSCI5572-AOS/rdma_new/utils.c

CMakeFiles/client.dir/utils.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/client.dir/utils.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/nvarsha/CSCI5572-AOS/rdma_new/utils.c > CMakeFiles/client.dir/utils.c.i

CMakeFiles/client.dir/utils.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/client.dir/utils.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/nvarsha/CSCI5572-AOS/rdma_new/utils.c -o CMakeFiles/client.dir/utils.c.s

CMakeFiles/client.dir/client.c.o: CMakeFiles/client.dir/flags.make
CMakeFiles/client.dir/client.c.o: client.c
CMakeFiles/client.dir/client.c.o: CMakeFiles/client.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/users/nvarsha/CSCI5572-AOS/rdma_new/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/client.dir/client.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/client.dir/client.c.o -MF CMakeFiles/client.dir/client.c.o.d -o CMakeFiles/client.dir/client.c.o -c /users/nvarsha/CSCI5572-AOS/rdma_new/client.c

CMakeFiles/client.dir/client.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/client.dir/client.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /users/nvarsha/CSCI5572-AOS/rdma_new/client.c > CMakeFiles/client.dir/client.c.i

CMakeFiles/client.dir/client.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/client.dir/client.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /users/nvarsha/CSCI5572-AOS/rdma_new/client.c -o CMakeFiles/client.dir/client.c.s

# Object files for target client
client_OBJECTS = \
"CMakeFiles/client.dir/utils.c.o" \
"CMakeFiles/client.dir/client.c.o"

# External object files for target client
client_EXTERNAL_OBJECTS =

bin/client: CMakeFiles/client.dir/utils.c.o
bin/client: CMakeFiles/client.dir/client.c.o
bin/client: CMakeFiles/client.dir/build.make
bin/client: /usr/lib/aarch64-linux-gnu/libibverbs.so
bin/client: /usr/lib/aarch64-linux-gnu/librdmacm.so
bin/client: CMakeFiles/client.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/users/nvarsha/CSCI5572-AOS/rdma_new/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable bin/client"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/client.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/client.dir/build: bin/client
.PHONY : CMakeFiles/client.dir/build

CMakeFiles/client.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/client.dir/cmake_clean.cmake
.PHONY : CMakeFiles/client.dir/clean

CMakeFiles/client.dir/depend:
	cd /users/nvarsha/CSCI5572-AOS/rdma_new && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /users/nvarsha/CSCI5572-AOS/rdma_new /users/nvarsha/CSCI5572-AOS/rdma_new /users/nvarsha/CSCI5572-AOS/rdma_new /users/nvarsha/CSCI5572-AOS/rdma_new /users/nvarsha/CSCI5572-AOS/rdma_new/CMakeFiles/client.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/client.dir/depend
