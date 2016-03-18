# Once done this will define
#  LibRingClient_INCLUDE_DIRS - include directories

IF(EXISTS ${LibRingClient_BUILD_DIR}/callmodel.h)
   SET(LibRingClient_FOUND true)
   SET(LibRingClient_INCLUDE_DIRS ${LibRingClient_BUILD_DIR})
   MESSAGE("lrc headers are in " ${LibRingClient_INCLUDE_DIRS})
ENDIF()
