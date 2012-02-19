if(Libgit2_INCLUDE_DIR AND Libgit2_LIBRARY)
  # Already in cache, be silent
  set(Libgit2_FIND_QUIETLY TRUE)
endif()

FIND_PATH(Libgit2_INCLUDE_DIR git2.h libgit2/include)
FIND_LIBRARY(Libgit2_LIBRARY NAMES git2 PATHS libgit2)

if(Libgit2_INCLUDE_DIR AND Libgit2_LIBRARY)
  set(Libgit2_FOUND TRUE)
endif()

if(Libgit2_FOUND)
  if(NOT Libgit2_FIND_QUIETLY)
    message(STATUS "Found libgit2: ${Libgit2_LIBRARY}")
  endif()
else()
  if(Libgit2_FIND_REQUIRED AND NOT Libgit2_FIND_QUIETLY)
    message(FATAL_ERROR "Could NOT find libgit2 library")
  endif()
endif()

mark_as_advanced(Libgit2_LIBRARY Libgit2_INCLUDE_DIR)
