# Find OGRE includes and library
#
# This module defines
#  OGRE_INCLUDE_DIRS
#  OGRE_LIBRARIES, the libraries to link against to use OGRE.
#  OGRE_LIBRARY_DIRS, the location of the libraries
#  OGRE_FOUND, If false, do not try to use OGRE
#
# Copyright © 2008, Matt Williams
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(FindPackageMessage)

set(PREFIX "C:/Program Files/OGRE3D")
get_filename_component(OGRE_LIBRARY_DIRS "${PREFIX}/lib" ABSOLUTE)
get_filename_component(OGRE_INCLUDE_DIRS "${PREFIX}/include/OGRE" ABSOLUTE)
set(OGRE_LIBRARIES "OgreMain")

message(STATUS "Found OGRE")
message(STATUS "  libraries : '${OGRE_LIBRARIES}' from ${OGRE_LIBRARY_DIRS}")
message(STATUS "  includes  : ${OGRE_INCLUDE_DIRS}")
