CMAKE_MINIMUM_REQUIRED(VERSION 3.1.0)

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

set (CMAKE_CXX_STANDARD 17)

# set project name and version
PROJECT(jami-client-gnome)

IF(EXISTS "${PROJECT_SOURCE_DIR}/version.txt")
   FILE (STRINGS "version.txt" PROJECT_VERSION)
ELSE()
   SET (PROJECT_VERSION "development")
ENDIF()

# set build date
IF( NOT ("$ENV{SOURCE_DATE_EPOCH}" STREQUAL ""))
  EXECUTE_PROCESS(
    COMMAND date -u -d "1970-01-01 UTC $ENV{SOURCE_DATE_EPOCH} seconds" "+%Y-%m-%d %H:%M:%S UTC"
    OUTPUT_VARIABLE CLIENT_BUILD_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
ELSEIF( "${CLIENT_BUILD_DATE}" STREQUAL "")
  EXECUTE_PROCESS(
    COMMAND date -u "+%Y-%m-%d %H:%M:%S UTC"
    OUTPUT_VARIABLE CLIENT_BUILD_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
ENDIF()

# add the binary tree to the search path for include files
# so that we will find config.h
INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}")

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# include custom cmake modules
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/GResources.cmake)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/GSettings.cmake)

# make sure we're using Qt5
SET(ENABLE_QT5 true)

# for now, set debug as default
SET(CMAKE_BUILD_TYPE Debug)

# during packaging the CMAKE_INSTALL_PREFIX is used to specify where to install the files, but it
# may not be the same as where the package will put them, use JAMI_INSTALL_PREFIX to let the client
# know at runtime where to expect installed files to be located
SET(JAMI_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}
  CACHE PATH "prefix where the package will be installed on the user's system (eg: /usr/local); defaults to the CMAKE_INSTALL_PREFIX"
)

# Check if LRC's location is manually specified with -DLibRingClient_PROJECT_DIR
IF(LibRingClient_PROJECT_DIR)
   SET(LIB_RING_CLIENT_INCLUDE_DIR ${LibRingClient_PROJECT_DIR}/src)
   # TODO: maybe allow a way to specify a different build dir
   SET(LIB_RING_LIBRARY_INCLUDE_DIR ${LibRingClient_PROJECT_DIR}/build)

   # check for the existence of the directories;
   # if the dirs have missing or outdated files then we will just get build/link errors
   IF(EXISTS "${LIB_RING_CLIENT_INCLUDE_DIR}/")
      MESSAGE("will expect lrc headers in ${LIB_RING_CLIENT_INCLUDE_DIR}")
      IF(EXISTS "${LIB_RING_LIBRARY_INCLUDE_DIR}/")
         MESSAGE("will expect lrc libraries in " ${LIB_RING_LIBRARY_INCLUDE_DIR})
         SET(LibRingClient_FOUND true)
         SET(LIB_RING_CLIENT_LIBRARY ringclient)
      ELSE()
         MESSAGE( FATAL_ERROR "${LIB_RING_LIBRARY_INCLUDE_DIR} does not exist")
      ENDIF(EXISTS "${LIB_RING_LIBRARY_INCLUDE_DIR}/")
   ELSE()
      MESSAGE( FATAL_ERROR "${LIB_RING_CLIENT_INCLUDE_DIR} does not exist")
   ENDIF(EXISTS "${LIB_RING_CLIENT_INCLUDE_DIR}/")
ELSE(LibRingClient_PROJECT_DIR)
   FIND_PACKAGE(LibRingClient REQUIRED)
ENDIF(LibRingClient_PROJECT_DIR)

# find packages
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0>=3.10)
PKG_CHECK_MODULES(GLIB REQUIRED glib-2.0>=2.40)
FIND_PACKAGE(Qt5Core REQUIRED)
PKG_CHECK_MODULES(CLUTTER REQUIRED clutter-1.0)
PKG_CHECK_MODULES(CLUTTERGTK REQUIRED clutter-gtk-1.0)
PKG_CHECK_MODULES(LIBNOTIFY libnotify>=0.7.6) #optional
IF( LIBNOTIFY_FOUND )
    pkg_check_modules(CANBERRA REQUIRED libcanberra-gtk3>=0.25)
ENDIF()
FIND_PACKAGE(Gettext) #optional for translations
PKG_CHECK_MODULES(LIBQRENCODE libqrencode>=3.4)
PKG_CHECK_MODULES(LIBNM libnm>=1.0) #optional to detect changes in the network

# Both appindicators are optional. ayatana-appindicator is better
# maintained and it works best across all desktop environments.
PKG_CHECK_MODULES(APPINDICATOR ayatana-appindicator3-0.1)
IF( APPINDICATOR_FOUND )
    SET(HAVE_AYATANAAPPINDICATOR 1)
    SET(HAVE_APPINDICATOR 1)
ELSE()
    PKG_CHECK_MODULES(APPINDICATOR appindicator3-0.1)
    IF( APPINDICATOR_FOUND )
        SET(HAVE_AYATANAAPPINDICATOR 0)
        SET(HAVE_APPINDICATOR 1)
    ELSE()
        SET(HAVE_AYATANAAPPINDICATOR 0)
        SET(HAVE_APPINDICATOR 0)
    ENDIF()
ENDIF()

PKG_CHECK_MODULES(WEBKIT webkit2gtk-4.0)
IF(NOT WEBKIT_FOUND)
    PKG_CHECK_MODULES(WEBKIT REQUIRED webkit2gtk-3.0)
ENDIF()

# include libs
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${LIB_RING_CLIENT_INCLUDE_DIR})
INCLUDE_DIRECTORIES(SYSTEM ${Qt5Core_INCLUDE_DIRS} )
INCLUDE_DIRECTORIES(${CLUTTER_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CLUTTERGTK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${LIBNOTIFY_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${APPINDICATOR_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${LIBNM_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${WEBKIT_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CANBERRA_INCLUDE_DIRS})

# link libs
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
LINK_DIRECTORIES(${LIB_RING_LIBRARY_INCLUDE_DIR})
LINK_DIRECTORIES(${Qt5Core_LIBRARY_DIRS} )
LINK_DIRECTORIES(${CLUTTER_LIBRARY_DIRS})
LINK_DIRECTORIES(${CLUTTERGTK_LIBRARY_DIRS})
LINK_DIRECTORIES(${LIBNOTIFY_LIBRARY_DIRS})
LINK_DIRECTORIES(${APPINDICATOR_LIBRARY_DIRS})
LINK_DIRECTORIES(${LIBNM_LIBRARY_DIRS})
LINK_DIRECTORIES(${WEBKIT_LIBRARY_DIRS})
LINK_DIRECTORIES(${CANBERRA_LIBRARY_DIRS})

# lib compiler flags
ADD_DEFINITIONS(${GTK3_CFLAGS})
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})
ADD_DEFINITIONS(${LIB_RING_LIBRARY_CFLAGS})
ADD_DEFINITIONS(${Qt5Core_CFLAGS})
ADD_DEFINITIONS(${CLUTTER_CFLAGS})
ADD_DEFINITIONS(${CLUTTERGTK_CFLAGS})
ADD_DEFINITIONS(${LIBNOTIFY_CFLAGS})
ADD_DEFINITIONS(${APPINDICATOR_CFLAGS})
ADD_DEFINITIONS(${LIBNM_CFLAGS})
ADD_DEFINITIONS(${WEBKIT_CFLAGS})
ADD_DEFINITIONS(${CANBERRA_CFLAGS})

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

# get git version on every build
SET(GIT_REVISION_INPUT_FILE ${PROJECT_SOURCE_DIR}/src/revision.h.in)
SET(GIT_REVISION_OUTPUT_FILE ${PROJECT_BINARY_DIR}/revision.h)

ADD_CUSTOM_TARGET(
   git_revision ALL
   COMMAND ${CMAKE_COMMAND}
      -DGIT_REVISION_INPUT_FILE=${GIT_REVISION_INPUT_FILE}
      -DGIT_REVISION_OUTPUT_FILE=${GIT_REVISION_OUTPUT_FILE}
      -DCLIENT_BUILD_DATE=${CLIENT_BUILD_DATE}
      -P ${CMAKE_SOURCE_DIR}/cmake/GitRevision.cmake
   COMMENT "generating ${GIT_REVISION_OUTPUT_FILE}"
)

ADD_CUSTOM_COMMAND(
   OUTPUT ${GIT_REVISION_OUTPUT_FILE}
   DEPENDS git_revision
)

# source files to compile
SET( SRC_FILES
   src/main.cpp
   src/client.h
   src/client.cpp
   src/client_options.h
   src/client_options.c
   src/mainwindow.h
   src/mainwindow.cpp
   src/incomingcallview.h
   src/incomingcallview.cpp
   src/currentcallview.h
   src/currentcallview.cpp
   src/models/gtkaccessproxymodel.h
   src/models/gtkaccessproxymodel.cpp
   src/models/gtkqtreemodel.h
   src/models/gtkqtreemodel.cpp
   src/utils/drawing.h
   src/utils/drawing.cpp
   src/video/video_widget.h
   src/video/video_widget.cpp
   src/accountcreationwizard.h
   src/accountcreationwizard.cpp
   src/accountmigrationview.h
   src/accountmigrationview.cpp
   src/usernameregistrationbox.h
   src/usernameregistrationbox.cpp
   src/models/activeitemproxymodel.h
   src/models/activeitemproxymodel.cpp
   src/defines.h
   src/video/xrectsel.h
   src/video/xrectsel.c
   src/dialogs.h
   src/dialogs.cpp
   src/mediasettingsview.h
   src/mediasettingsview.cpp
   src/pluginsettingsview.h
   src/pluginsettingsview.cpp
   src/newaccountsettingsview.h
   src/newaccountsettingsview.cpp
   src/generalsettingsview.h
   src/generalsettingsview.cpp
   src/native/pixbufmanipulator.h
   src/native/pixbufmanipulator.cpp
   src/utils/models.h
   src/utils/models.cpp
   src/notifier.h
   src/notifier.cpp
   src/utils/files.h
   src/utils/files.cpp
   ${GIT_REVISION_OUTPUT_FILE}
   src/native/dbuserrorhandler.h
   src/native/dbuserrorhandler.cpp
   src/welcomeview.h
   src/welcomeview.cpp
   src/webkitchatcontainer.h
   src/webkitchatcontainer.cpp
   src/chatview.h
   src/messagingwidget.h
   src/messagingwidget.cpp
   src/chatview.cpp
   src/avatarmanipulation.h
   src/avatarmanipulation.cpp
   src/cc-crop-area.h
   src/cc-crop-area.c
   src/conversationsview.h
   src/conversationsview.cpp
   src/conversationpopupmenu.h
   src/conversationpopupmenu.cpp
   src/accountinfopointer.h
   src/profileview.h
   src/profileview.cpp
   # Generated marshals
   src/marshals.h
   src/marshals.cpp
)

# compile glib resource files to c code
GLIB_COMPILE_RESOURCES( GLIB_RESOURCES_RING
   SOURCE
      pixmaps/pixmaps.gresource.xml
      ui/ui.gresource.xml
      web/web.gresource.xml
      sounds/sounds.gresource.xml
)

# IF(${ENABLE_TEST} MATCHES true)
#    ADD_SUBDIRECTORY( src/test)
# ENDIF()

# for translations
IF (GETTEXT_FOUND)

  # get all translation files; they're expected to be named using the following convention:
  # '<lang code>[_<country code].po'
  # eg: fr_CA.po (the country code is optional)
  FILE(GLOB PO_FILES ${CMAKE_SOURCE_DIR}/po/*.po)

  # Install each of the translation files
  FOREACH(PO_FILE IN LISTS PO_FILES)
    # extract the language (and optional country code) from the filename
    GET_FILENAME_COMPONENT(LANG ${PO_FILE} NAME_WE)
    GETTEXT_PROCESS_PO_FILES(${LANG} ALL PO_FILES ${PO_FILE})

    SET(TRANSLATIONS_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/share/locale/${LANG}/LC_MESSAGES)

    # install to share/locale and rename each to ring-client-gnome.mo
    INSTALL(
      FILES ${PROJECT_BINARY_DIR}/${LANG}.gmo
      DESTINATION ${TRANSLATIONS_INSTALL_DIR}
      RENAME ${PROJECT_NAME}.mo
    )
  ENDFOREACH(PO_FILE)

ENDIF(GETTEXT_FOUND)

# install and compile glib gsettings schema
add_schema("net.jami.Jami.gschema.xml" GSCHEMA_RING)

ADD_EXECUTABLE(jami-gnome ${GLIB_RESOURCES_RING} ${GSCHEMA_RING} ${SRC_FILES})

IF(NOT ${ENABLE_STATIC} MATCHES false)
TARGET_LINK_LIBRARIES(jami-gnome
   ${LIB_RING_CLIENT_LIBRARY}
   ${GTK3_LIBRARIES}
   ${Qt5Core_LIBRARIES}
   ${Qt5DBus_LIBRARIES}
   ${CLUTTER_LIBRARIES}
   ${CLUTTERGTK_LIBRARIES}
   ${LIBNOTIFY_LIBRARIES}
   ${APPINDICATOR_LIBRARIES}
   ${LIBNM_LIBRARIES}
   ${WEBKIT_LIBRARIES}
   ${LIBQRENCODE_LIBRARIES}
   ${CANBERRA_LIBRARIES}
   -lpthread
   -lrt
   )
ELSE()
TARGET_LINK_LIBRARIES(jami-gnome
   ${LIB_RING_CLIENT_LIBRARY}
   ${GTK3_LIBRARIES}
   ${Qt5Core_LIBRARIES}
   ${CLUTTER_LIBRARIES}
   ${CLUTTERGTK_LIBRARIES}
   ${LIBNOTIFY_LIBRARIES}
   ${APPINDICATOR_LIBRARIES}
   ${LIBNM_LIBRARIES}
   ${WEBKIT_LIBRARIES}
   ${LIBQRENCODE_LIBRARIES}
   ${CANBERRA_LIBRARIES}
   )
ENDIF()

# configure libnotify variable for config.h file
IF( LIBNOTIFY_FOUND )
   SET(USE_LIBNOTIFY 1)
   SET(USE_CANBERRA 1)
ELSE()
   SET(USE_LIBNOTIFY 0)
ENDIF()

# configure libnm variable for config.h file
IF( LIBNM_FOUND )
   SET(USE_LIBNM 1)
ELSE()
   SET(USE_LIBNM 0)
ENDIF()

# configure libcanberra variable for config.h file
IF( CANBERRA_FOUND )
   SET(USE_CANBERRA 1)
ELSE()
   SET(USE_CANBERRA 0)
ENDIF()

# create config header file to pass cmake settings to source code
CONFIGURE_FILE (
   "${PROJECT_SOURCE_DIR}/src/config.h.in"
   "${PROJECT_BINARY_DIR}/config.h"
)

INSTALL(TARGETS jami-gnome
   RUNTIME DESTINATION bin
)

# install .desktop in XDG desktop dir so that it is recognized by the system
INSTALL(FILES ${PROJECT_SOURCE_DIR}/jami-gnome.desktop
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications
)

# install audio files
INSTALL(FILES ${PROJECT_SOURCE_DIR}/sounds/ringtone_notify.wav
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/sounds/jami-gnome
)

# install .desktop in the jami-gnome data dir, so that it can be copied to the
# autostart dir by the client
INSTALL(FILES ${PROJECT_SOURCE_DIR}/jami-gnome.desktop.autostart
   DESTINATION
      ${CMAKE_INSTALL_PREFIX}/share/jami-gnome
   PERMISSIONS
      WORLD_READ
      OWNER_WRITE
      OWNER_READ
      GROUP_READ
   RENAME
      jami-gnome.desktop
)

INSTALL(FILES pixmaps/jami-gnome.svg
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps
)

INSTALL(FILES jami-gnome.appdata.xml
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/metainfo/
)

# install shell script which will launch Jami
INSTALL(
   FILES
      "${PROJECT_SOURCE_DIR}/src/jami"
   DESTINATION
      ${CMAKE_INSTALL_PREFIX}/bin
   PERMISSIONS
      WORLD_READ
      OWNER_WRITE
      OWNER_READ
      GROUP_READ
      OWNER_EXECUTE
      GROUP_EXECUTE
      WORLD_EXECUTE
)

INSTALL(CODE "
    EXECUTE_PROCESS(COMMAND rm ${CMAKE_INSTALL_PREFIX}/bin/ring.cx -f)
    EXECUTE_PROCESS(COMMAND ln -s ${CMAKE_INSTALL_PREFIX}/bin/jami ${CMAKE_INSTALL_PREFIX}/bin/ring.cx)
    ")

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

# uninstall target
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
