# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.7

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/sblin/Projets/ring-project/client-gnome

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/sblin/Projets/ring-project/client-gnome/build-local

# Utility rule file for pofiles_42.

# Include the progress variables for this target.
include CMakeFiles/pofiles_42.dir/progress.make

CMakeFiles/pofiles_42: uk.gmo


uk.gmo: ../po/uk.po
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/sblin/Projets/ring-project/client-gnome/build-local/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating uk.gmo"
	cd /home/sblin/Projets/ring-project/client-gnome && /usr/bin/msgfmt -o /home/sblin/Projets/ring-project/client-gnome/build-local/uk.gmo /home/sblin/Projets/ring-project/client-gnome/po/uk.po

pofiles_42: CMakeFiles/pofiles_42
pofiles_42: uk.gmo
pofiles_42: CMakeFiles/pofiles_42.dir/build.make

.PHONY : pofiles_42

# Rule to build all files generated by this target.
CMakeFiles/pofiles_42.dir/build: pofiles_42

.PHONY : CMakeFiles/pofiles_42.dir/build

CMakeFiles/pofiles_42.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/pofiles_42.dir/cmake_clean.cmake
.PHONY : CMakeFiles/pofiles_42.dir/clean

CMakeFiles/pofiles_42.dir/depend:
	cd /home/sblin/Projets/ring-project/client-gnome/build-local && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sblin/Projets/ring-project/client-gnome /home/sblin/Projets/ring-project/client-gnome /home/sblin/Projets/ring-project/client-gnome/build-local /home/sblin/Projets/ring-project/client-gnome/build-local /home/sblin/Projets/ring-project/client-gnome/build-local/CMakeFiles/pofiles_42.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/pofiles_42.dir/depend

