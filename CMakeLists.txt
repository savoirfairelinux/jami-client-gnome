CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)

# First, check is the compiler is new enough, most version of Clang are fine
# until problems arise, checking for GCC is enough
IF (CMAKE_COMPILER_IS_GNUCC)
   EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
   IF (GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
      MESSAGE(STATUS "Found GCC version >= 4.8: " ${GCC_VERSION})
   ELSE()
      MESSAGE(FATAL_ERROR "Your version of GCC is too old, please install GCC 4.8 or later")
   ENDIF()
ENDIF()

ADD_DEFINITIONS("-std=c++1y")

# set project name and version
PROJECT(ring-client-gnome)
SET(PROJECT_VERSION_MAJOR 0)
SET(PROJECT_VERSION_MINOR 4)
SET(PROJECT_VERSION_PATCH 0)

# add the binary tree to the search path for include files
# so that we will find config.h
INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}")

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# the GResources.cmake module isn't automatically detected
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/GResources.cmake)

# make sure we're using Qt5
SET(ENABLE_QT5 true)

# for now, set debug as default
SET(CMAKE_BUILD_TYPE Debug)

# find packages
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0>=3.10)
FIND_PACKAGE(LibRingClient REQUIRED)
FIND_PACKAGE(Qt5Core REQUIRED)
PKG_CHECK_MODULES(CLUTTER REQUIRED clutter-1.0)
PKG_CHECK_MODULES(CLUTTERGTK REQUIRED clutter-gtk-1.0)
PKG_CHECK_MODULES(ICONSYMBLIC REQUIRED gnome-icon-theme-symbolic)
PKG_CHECK_MODULES(EBOOK REQUIRED libebook-1.2>=3.10)
PKG_CHECK_MODULES(LIBNOTIFY libnotify>=0.7.6) #optional

# include libs
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${LIB_RING_CLIENT_INCLUDE_DIR})
INCLUDE_DIRECTORIES(SYSTEM ${Qt5Core_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES(${CLUTTER_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CLUTTERGTK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${EBOOK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${LIBNOTIFY_INCLUDE_DIRS})

# link libs
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
LINK_DIRECTORIES(${LIB_RING_LIBRARY_INCLUDE_DIR})
LINK_DIRECTORIES(${Qt5Core_LIBRARY_DIRS} )
LINK_DIRECTORIES(${CLUTTER_LIBRARY_DIRS})
LINK_DIRECTORIES(${CLUTTERGTK_LIBRARY_DIRS})
LINK_DIRECTORIES(${EBOOK_LIBRARY_DIRS})
LINK_DIRECTORIES(${LIBNOTIFY_LIBRARY_DIRS})

# lib compiler flags
ADD_DEFINITIONS(${GTK3_CFLAGS})
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})
ADD_DEFINITIONS(${LIB_RING_LIBRARY_CFLAGS})
ADD_DEFINITIONS(${Qt5Core_CFLAGS})
ADD_DEFINITIONS(${CLUTTER_CFLAGS})
ADD_DEFINITIONS(${CLUTTERGTK_CFLAGS})
ADD_DEFINITIONS(${EBOOK_CFLAGS})
ADD_DEFINITIONS(${LIBNOTIFY_CFLAGS})

IF(NOT ${ENABLE_STATIC} MATCHES false)
  SET(QT5_MODULE_PATH ${QT5_PATH}/lib/cmake)
  FIND_PACKAGE(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED Core DBus)
  FIND_PACKAGE(Qt5DBus REQUIRED)
  INCLUDE_DIRECTORIES(SYSTEM ${Qt5DBus_INCLUDE_DIRS} )
  LINK_DIRECTORIES(${Qt5DBus_LIBRARY_DIRS} )
  ADD_DEFINITIONS(${Qt5DBus_CFLAGS})
ENDIF()

# warnings
ADD_DEFINITIONS(
   -Wall
   -Wextra
   -Wno-cpp
   -Wmissing-noreturn
   -Wpointer-arith
   -Wcast-align
   -Wwrite-strings
   -Wformat-nonliteral
   -Wformat-security
   -Wswitch-enum
   -Winit-self
   -Wmissing-include-dirs
   -Wundef
   -Wmissing-format-attribute
   -Wno-reorder
   -Wunused
   -Wuninitialized
   -Woverloaded-virtual
   -Wsuggest-attribute=const
   -Wunused-value
   -Wdouble-promotion
   -Wnonnull
   -Wsequence-point
   -Wunused-but-set-parameter
   -Wsizeof-pointer-memaccess
   -Wvarargs
)

# more warnings
IF (CMAKE_COMPILER_IS_GNUCC)
   IF (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
      ADD_DEFINITIONS(
         -Wconditionally-supported
         -Wdate-time
         -Wdelete-incomplete
         -Wfloat-conversion
      )
   ENDIF()

   IF (GCC_VERSION VERSION_GREATER 5.0 OR GCC_VERSION VERSION_EQUAL 5.0)
      ADD_DEFINITIONS(
         -Wsuggest-override
         -Wsuggest-final-types
         -Wsuggest-final-methods
         -Wbool-compare
         -Wformat-signedness
         -Wlogical-not-parentheses
         -Wnormalized
         -Wshift-count-negative
         -Wshift-count-overflow
         -Wsized-deallocation
         -Wsizeof-array-argument
      )
   ENDIF()

ENDIF()

# source files to compile
SET( SRC_FILES
   src/main.cpp
   src/ring_client.h
   src/ring_client.cpp
   src/ring_client_options.h
   src/ring_client_options.c
   src/ringmainwindow.h
   src/ringmainwindow.cpp
   src/incomingcallview.h
   src/incomingcallview.cpp
   src/currentcallview.h
   src/currentcallview.cpp
   src/models/gtkaccessproxymodel.h
   src/models/gtkaccessproxymodel.cpp
   src/models/gtkqtreemodel.h
   src/models/gtkqtreemodel.cpp
   src/utils/drawing.h
   src/utils/drawing.c
   src/video/video_widget.h
   src/video/video_widget.cpp
   src/backends/minimalhistorybackend.h
   src/backends/minimalhistorybackend.cpp
   src/models/gtkqsortfiltertreemodel.h
   src/models/gtkqsortfiltertreemodel.cpp
   src/accountview.h
   src/accountview.cpp
   src/accountgeneraltab.h
   src/accountgeneraltab.cpp
   src/accountaudiotab.h
   src/accountaudiotab.cpp
   src/accountvideotab.h
   src/accountvideotab.cpp
   src/accountadvancedtab.h
   src/accountadvancedtab.cpp
   src/accountsecuritytab.h
   src/accountsecuritytab.cpp
   src/models/activeitemproxymodel.h
   src/models/activeitemproxymodel.cpp
   src/defines.h
   src/video/xrectsel.h
   src/video/xrectsel.c
   src/dialogs.h
   src/dialogs.c
   src/mediasettingsview.h
   src/mediasettingsview.cpp
   src/generalsettingsview.h
   src/generalsettingsview.cpp
   src/backends/edscontactbackend.h
   src/backends/edscontactbackend.cpp
   src/delegates/pixbufdelegate.h
   src/delegates/pixbufdelegate.cpp
   src/callsview.h
   src/callsview.cpp
   src/contactsview.h
   src/contactsview.cpp
   src/frequentcontactsview.h
   src/frequentcontactsview.cpp
   src/historyview.h
   src/historyview.cpp
   src/utils/models.h
   src/utils/models.cpp
   src/utils/calling.h
   src/utils/calling.cpp
   src/ringnotify.h
   src/ringnotify.cpp
)

# compile glib resource files to c code
GLIB_COMPILE_RESOURCES( GLIB_RESOURCES_RING
   SOURCE
      pixmaps/pixmaps.gresource.xml
      ui/ui.gresource.xml
)

# IF(${ENABLE_TEST} MATCHES true)
#    ADD_SUBDIRECTORY( src/test)
# ENDIF()

# for translations
# FIND_PACKAGE ( Gettext REQUIRED )
# ADD_SUBDIRECTORY( po )

ADD_EXECUTABLE(gnome-ring ${GLIB_RESOURCES_RING} ${SRC_FILES})

IF(NOT ${ENABLE_STATIC} MATCHES false)
TARGET_LINK_LIBRARIES(gnome-ring
   ${LIB_RING_CLIENT_LIBRARY}
   ${GTK3_LIBRARIES}
   ${Qt5Core_LIBRARIES}
   ${Qt5DBus_LIBRARIES}
   ${CLUTTER_LIBRARIES}
   ${CLUTTERGTK_LIBRARIES}
   ${EBOOK_LIBRARIES}
   ${LIBNOTIFY_LIBRARIES}
   -lpthread
   -lrt
   )
ELSE()
TARGET_LINK_LIBRARIES(gnome-ring
   ${LIB_RING_CLIENT_LIBRARY}
   ${GTK3_LIBRARIES}
   ${Qt5Core_LIBRARIES}
   ${CLUTTER_LIBRARIES}
   ${CLUTTERGTK_LIBRARIES}
   ${EBOOK_LIBRARIES}
   ${LIBNOTIFY_LIBRARIES}
   )
ENDIF()

# configure libnotify variable for config.h file
IF( LIBNOTIFY_FOUND )
   SET(USE_LIBNOTIFY 1)
ELSE()
   SET(USE_LIBNOTIFY 0)
ENDIF()

# create config header file to pass cmake settings to source code
CONFIGURE_FILE (
   "${PROJECT_SOURCE_DIR}/src/config.h.in"
   "${PROJECT_BINARY_DIR}/config.h"
)

# generate .desktop file
CONFIGURE_FILE (
   "${PROJECT_SOURCE_DIR}/gnome-ring.desktop.in"
   "${PROJECT_BINARY_DIR}/gnome-ring.desktop"
)

INSTALL(TARGETS gnome-ring
   RUNTIME DESTINATION bin
)

INSTALL(FILES ${PROJECT_BINARY_DIR}/gnome-ring.desktop
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications
)

INSTALL(FILES pixmaps/ring.svg
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps
)

INSTALL(FILES gnome-ring.appdata.xml
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/appdata/
)

# add a target to generate API documentation with Doxygen
FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile @ONLY)
    ADD_CUSTOM_TARGET(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )

    # create doc/README.md symlink to README since Doxygen doesn't understand file with no extension
    ADD_CUSTOM_COMMAND(
        TARGET doc
        PRE_BUILD
        COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/README ${CMAKE_CURRENT_SOURCE_DIR}/doc/README.md
    )
ENDIF(DOXYGEN_FOUND)
