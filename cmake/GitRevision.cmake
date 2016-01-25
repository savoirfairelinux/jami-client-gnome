#  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
#  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#

FIND_PACKAGE(Git)

IF(GIT_FOUND)
   EXECUTE_PROCESS(
      COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
      OUTPUT_VARIABLE RING_CLIENT_REVISION
      OUTPUT_STRIP_TRAILING_WHITESPACE
   )
ENDIF()

IF( "${RING_CLIENT_REVISION}" STREQUAL "")
   SET(RING_CLIENT_REVISION "unknown")
ENDIF()

MESSAGE( STATUS "GIT_REVISION_INPUT_FILE: " ${GIT_REVISION_INPUT_FILE} )
MESSAGE( STATUS "GIT_REVISION_OUTPUT_FILE: " ${GIT_REVISION_OUTPUT_FILE} )

# generate revision.h file
CONFIGURE_FILE (
   ${GIT_REVISION_INPUT_FILE}
   ${GIT_REVISION_OUTPUT_FILE}
)
