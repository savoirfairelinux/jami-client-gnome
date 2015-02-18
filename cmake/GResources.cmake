# The code below was taken from the Venom project which is released under the
# GNU General Public License, version 3
#
# Copyright (C) 2013 Venom authors and contributors
#
# See https://github.com/naxuroqa/Venom for a list of contributors.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#
# Additional permission under GNU GPL version 3 section 7:
#
# If you modify this program, or any covered work, by linking or
# combining it with the OpenSSL project's OpenSSL library (or a
# modified version of that library), containing parts covered by the
# terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
# grants you additional permission to convey the resulting work.
# Corresponding Source for a non-source form of such a combination
# shall include the source code for the parts of OpenSSL used as well
# as that of the covered work.
#

FIND_PROGRAM(GLIB_COMPILE_RESOURCES_EXECUTABLE NAMES glib-compile-resources)
MARK_AS_ADVANCED(GLIB_COMPILE_RESOURCES_EXECUTABLE)

INCLUDE(CMakeParseArguments)

FUNCTION(GLIB_COMPILE_RESOURCES output)
  CMAKE_PARSE_ARGUMENTS(ARGS "" "SOURCE" ${ARGN})
  SET(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  SET(out_files "")

  FOREACH(src ${ARGS_SOURCE} ${ARGS_UNPARSED_ARGUMENTS})
    SET(in_file "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    GET_FILENAME_COMPONENT(WORKING_DIR ${in_file} PATH)
    STRING(REPLACE ".xml" ".c" src ${src})
    SET(out_file "${DIRECTORY}/${src}")
    GET_FILENAME_COMPONENT(OUPUT_DIR ${out_file} PATH)
    FILE(MAKE_DIRECTORY ${OUPUT_DIR})
    LIST(APPEND out_files "${DIRECTORY}/${src}")

    #FIXME implicit depends currently not working
    EXECUTE_PROCESS(
      COMMAND
        ${GLIB_COMPILE_RESOURCES_EXECUTABLE}
          "--generate-dependencies"
          ${in_file}
      WORKING_DIRECTORY ${WORKING_DIR}
      OUTPUT_VARIABLE in_file_dep
    )
    STRING(REGEX REPLACE "(\r?\n)" ";" in_file_dep "${in_file_dep}")
    SET(in_file_dep_path "")
    FOREACH(dep ${in_file_dep})
      LIST(APPEND in_file_dep_path "${WORKING_DIR}/${dep}")
    ENDFOREACH(dep ${in_file_dep})
    ADD_CUSTOM_COMMAND(
      OUTPUT ${out_file}
      WORKING_DIRECTORY ${WORKING_DIR}
      COMMAND
        ${GLIB_COMPILE_RESOURCES_EXECUTABLE}
      ARGS
        "--generate-source"
        "--target=${out_file}"
        ${in_file}
      DEPENDS
        ${in_file};${in_file_dep_path}
    )
  ENDFOREACH(src ${ARGS_SOURCES} ${ARGS_UNPARSED_ARGUMENTS})
  SET(${output} ${out_files} PARENT_SCOPE)
ENDFUNCTION(GLIB_COMPILE_RESOURCES)
