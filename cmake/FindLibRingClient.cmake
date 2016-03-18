# Once done this will define
#  LibRingClient_INCLUDE_DIRS - include directories

IF(EXISTS ${LibRingClient_DIR}/src/callmodel.h)
   SET(LibRingClient_FOUND true)
   SET(LibRingClient_INCLUDE_DIRS ${LibRingClient_DIR}/src)
   SET(LibRingClient_LIBRARY_DIRS ${LibRingClient_DIR}/build)
   SET(LIB_RING_CLIENT_LIBRARY ringclient)
   MESSAGE("lrc headers are in " ${LibRingClient_INCLUDE_DIRS})
   MESSAGE("lrc libraries are in " ${LibRingClient_LIBRARY_DIRS})
ENDIF()
