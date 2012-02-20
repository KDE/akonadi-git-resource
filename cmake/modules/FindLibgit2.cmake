# Find Libgit2
#
# This module defines
#  Libgit2_FOUND - whether the qsjon library was found
#  Libgit2_LIBRARIES - the qjson library
#  Libgit2_INCLUDE_DIR - the include path of the qjson library
#
# Based on FindQJSON.cmake from the akonadi-facebook resource by Thomas McGuire and Pino Toscano
#

if (Libgit2_INCLUDE_DIR AND Libgit2_LIBRARIES AND Libgit2_VERSION)

  # Already in cache
  set (Libgit2_FOUND TRUE)

else (Libgit2_INCLUDE_DIR AND Libgit2_LIBRARIES AND Libgit2_VERSION)

  if (NOT WIN32)
    set (_pc_libgit2_string "libgit2")
    if (Libgit2_FIND_VERSION_EXACT)
      set (_pc_libgit2_string "${_pc_libgit2_string}=")
    else (Libgit2_FIND_VERSION_EXACT)
      set (_pc_libgit2_string "${_pc_libgit2_string}>=")
    endif (Libgit2_FIND_VERSION_EXACT)
    if (Libgit2_FIND_VERSION_COUNT GREATER 0)
      set (_pc_libgit2_string "${_pc_libgit2_string}${Libgit2_FIND_VERSION}")
    else (Libgit2_FIND_VERSION_COUNT GREATER 0)
      set (_pc_libgit2_string "libgit2")
    endif (Libgit2_FIND_VERSION_COUNT GREATER 0)
    # use pkg-config to get the values of Libgit2_INCLUDE_DIRS
    # and Libgit2_LIBRARY_DIRS to add as hints to the find commands.
    include (FindPkgConfig)
    pkg_check_modules (PC_Libgit2 ${_pc_libgit2_string})
    set (Libgit2_VERSION "${PC_Libgit2_VERSION}")
  endif (NOT WIN32)

  find_library (Libgit2_LIBRARIES
    NAMES
    git2
    PATHS
    ${PC_Libgit2_LIBRARY_DIRS}
    ${LIB_INSTALL_DIR}
    ${KDE4_LIB_DIR}
  )

  find_path (Libgit2_INCLUDE_DIR
    NAMES
    git2.h
    PATHS
    ${PC_Libgit2_INCLUDE_DIRS}
    ${INCLUDE_INSTALL_DIR}
    ${KDE4_INCLUDE_DIR}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Libgit2
                                    REQUIRED_VARS Libgit2_LIBRARIES Libgit2_INCLUDE_DIR Libgit2_VERSION
                                    VERSION_VAR Libgit2_VERSION
  )

endif (Libgit2_INCLUDE_DIR AND Libgit2_LIBRARIES AND Libgit2_VERSION)