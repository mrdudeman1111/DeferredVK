# CMAKE generated file: DO NOT EDIT!
# Generated by "MinGW Makefiles" Generator, CMake Version 3.30

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

SHELL = cmd.exe

# The CMake executable.
CMAKE_COMMAND = C:\mingw64\bin\cmake.exe

# The command to remove a file.
RM = C:\mingw64\bin\cmake.exe -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = D:\Repos\C++Replication

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = D:\Repos\C++Replication\build

# Include any dependencies generated for this target.
include CMakeFiles/VkScene.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/VkScene.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/VkScene.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/VkScene.dir/flags.make

CMakeFiles/VkScene.dir/src/Framework.cpp.obj: CMakeFiles/VkScene.dir/flags.make
CMakeFiles/VkScene.dir/src/Framework.cpp.obj: CMakeFiles/VkScene.dir/includes_CXX.rsp
CMakeFiles/VkScene.dir/src/Framework.cpp.obj: D:/Repos/C++Replication/src/Framework.cpp
CMakeFiles/VkScene.dir/src/Framework.cpp.obj: CMakeFiles/VkScene.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=D:\Repos\C++Replication\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/VkScene.dir/src/Framework.cpp.obj"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/VkScene.dir/src/Framework.cpp.obj -MF CMakeFiles\VkScene.dir\src\Framework.cpp.obj.d -o CMakeFiles\VkScene.dir\src\Framework.cpp.obj -c D:\Repos\C++Replication\src\Framework.cpp

CMakeFiles/VkScene.dir/src/Framework.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/VkScene.dir/src/Framework.cpp.i"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E D:\Repos\C++Replication\src\Framework.cpp > CMakeFiles\VkScene.dir\src\Framework.cpp.i

CMakeFiles/VkScene.dir/src/Framework.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/VkScene.dir/src/Framework.cpp.s"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S D:\Repos\C++Replication\src\Framework.cpp -o CMakeFiles\VkScene.dir\src\Framework.cpp.s

CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj: CMakeFiles/VkScene.dir/flags.make
CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj: CMakeFiles/VkScene.dir/includes_CXX.rsp
CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj: D:/Repos/C++Replication/src/Wrappers.cpp
CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj: CMakeFiles/VkScene.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=D:\Repos\C++Replication\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj -MF CMakeFiles\VkScene.dir\src\Wrappers.cpp.obj.d -o CMakeFiles\VkScene.dir\src\Wrappers.cpp.obj -c D:\Repos\C++Replication\src\Wrappers.cpp

CMakeFiles/VkScene.dir/src/Wrappers.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/VkScene.dir/src/Wrappers.cpp.i"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E D:\Repos\C++Replication\src\Wrappers.cpp > CMakeFiles\VkScene.dir\src\Wrappers.cpp.i

CMakeFiles/VkScene.dir/src/Wrappers.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/VkScene.dir/src/Wrappers.cpp.s"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S D:\Repos\C++Replication\src\Wrappers.cpp -o CMakeFiles\VkScene.dir\src\Wrappers.cpp.s

CMakeFiles/VkScene.dir/src/main.cpp.obj: CMakeFiles/VkScene.dir/flags.make
CMakeFiles/VkScene.dir/src/main.cpp.obj: CMakeFiles/VkScene.dir/includes_CXX.rsp
CMakeFiles/VkScene.dir/src/main.cpp.obj: D:/Repos/C++Replication/src/main.cpp
CMakeFiles/VkScene.dir/src/main.cpp.obj: CMakeFiles/VkScene.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=D:\Repos\C++Replication\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/VkScene.dir/src/main.cpp.obj"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/VkScene.dir/src/main.cpp.obj -MF CMakeFiles\VkScene.dir\src\main.cpp.obj.d -o CMakeFiles\VkScene.dir\src\main.cpp.obj -c D:\Repos\C++Replication\src\main.cpp

CMakeFiles/VkScene.dir/src/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/VkScene.dir/src/main.cpp.i"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E D:\Repos\C++Replication\src\main.cpp > CMakeFiles\VkScene.dir\src\main.cpp.i

CMakeFiles/VkScene.dir/src/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/VkScene.dir/src/main.cpp.s"
	C:\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S D:\Repos\C++Replication\src\main.cpp -o CMakeFiles\VkScene.dir\src\main.cpp.s

# Object files for target VkScene
VkScene_OBJECTS = \
"CMakeFiles/VkScene.dir/src/Framework.cpp.obj" \
"CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj" \
"CMakeFiles/VkScene.dir/src/main.cpp.obj"

# External object files for target VkScene
VkScene_EXTERNAL_OBJECTS =

VkScene.exe: CMakeFiles/VkScene.dir/src/Framework.cpp.obj
VkScene.exe: CMakeFiles/VkScene.dir/src/Wrappers.cpp.obj
VkScene.exe: CMakeFiles/VkScene.dir/src/main.cpp.obj
VkScene.exe: CMakeFiles/VkScene.dir/build.make
VkScene.exe: CMakeFiles/VkScene.dir/linkLibs.rsp
VkScene.exe: CMakeFiles/VkScene.dir/objects1.rsp
VkScene.exe: CMakeFiles/VkScene.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=D:\Repos\C++Replication\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable VkScene.exe"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles\VkScene.dir\link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/VkScene.dir/build: VkScene.exe
.PHONY : CMakeFiles/VkScene.dir/build

CMakeFiles/VkScene.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles\VkScene.dir\cmake_clean.cmake
.PHONY : CMakeFiles/VkScene.dir/clean

CMakeFiles/VkScene.dir/depend:
	$(CMAKE_COMMAND) -E cmake_depends "MinGW Makefiles" D:\Repos\C++Replication D:\Repos\C++Replication D:\Repos\C++Replication\build D:\Repos\C++Replication\build D:\Repos\C++Replication\build\CMakeFiles\VkScene.dir\DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/VkScene.dir/depend
