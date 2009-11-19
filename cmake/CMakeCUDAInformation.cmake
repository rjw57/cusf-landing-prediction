# Modified from CMakeCInformation.cmake

# This file sets the basic flags for the CUDA language in CMake.
# It also loads the available platform file for the system-compiler
# if it exists.
# It also loads a system - compiler - processor (or target hardware)
# specific file, which is mainly useful for crosscompiling and embedded systems.

# some compilers use different extensions (e.g. sdcc uses .rel)
# so set the extension here first so it can be overridden by the compiler specific file
IF(UNIX)
  SET(CMAKE_CUDA_OUTPUT_EXTENSION .o)
ELSE(UNIX)
  SET(CMAKE_CUDA_OUTPUT_EXTENSION .obj)
ENDIF(UNIX)

GET_FILENAME_COMPONENT(CMAKE_BASE_NAME ${CMAKE_CUDA_COMPILER} NAME_WE)
IF(CMAKE_COMPILER_IS_NVCC)
  SET(CMAKE_BASE_NAME nvcc)
ENDIF(CMAKE_COMPILER_IS_NVCC)


# load a hardware specific file, mostly useful for embedded compilers
IF(CMAKE_SYSTEM_PROCESSOR)
  IF(CMAKE_CUDA_COMPILER_ID)
    INCLUDE(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_CUDA_COMPILER_ID}-C-${CMAKE_SYSTEM_PROCESSOR} OPTIONAL RESULT_VARIABLE _INCLUDED_FILE)
  ENDIF(CMAKE_CUDA_COMPILER_ID)
  IF (NOT _INCLUDED_FILE)
    INCLUDE(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME}-${CMAKE_SYSTEM_PROCESSOR} OPTIONAL)
  ENDIF (NOT _INCLUDED_FILE)
ENDIF(CMAKE_SYSTEM_PROCESSOR)


# load the system- and compiler specific files
IF(CMAKE_CUDA_COMPILER_ID)
  INCLUDE(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_CUDA_COMPILER_ID}-C OPTIONAL RESULT_VARIABLE _INCLUDED_FILE)
ENDIF(CMAKE_CUDA_COMPILER_ID)
IF (NOT _INCLUDED_FILE)
  INCLUDE(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME} OPTIONAL)
ENDIF (NOT _INCLUDED_FILE)


# This should be included before the _INIT variables are
# used to initialize the cache.  Since the rule variables 
# have if blocks on them, users can still define them here.
# But, it should still be after the platform file so changes can
# be made to those values.

IF(CMAKE_USER_MAKE_RULES_OVERRIDE)
   INCLUDE(${CMAKE_USER_MAKE_RULES_OVERRIDE})
ENDIF(CMAKE_USER_MAKE_RULES_OVERRIDE)

IF(CMAKE_USER_MAKE_RULES_OVERRIDE_C)
   INCLUDE(${CMAKE_USER_MAKE_RULES_OVERRIDE_C})
ENDIF(CMAKE_USER_MAKE_RULES_OVERRIDE_C)


# for most systems a module is the same as a shared library
# so unless the variable CMAKE_MODULE_EXISTS is set just
# copy the values from the LIBRARY variables
IF(NOT CMAKE_MODULE_EXISTS)
  SET(CMAKE_SHARED_MODULE_CUDA_FLAGS ${CMAKE_SHARED_LIBRARY_CUDA_FLAGS})
  SET(CMAKE_SHARED_MODULE_CREATE_CUDA_FLAGS ${CMAKE_SHARED_LIBRARY_CREATE_CUDA_FLAGS})
ENDIF(NOT CMAKE_MODULE_EXISTS)

SET(CMAKE_CUDA_FLAGS_INIT "$ENV{CFLAGS} ${CMAKE_CUDA_FLAGS_INIT}")
# avoid just having a space as the initial value for the cache 
IF(CMAKE_CUDA_FLAGS_INIT STREQUAL " ")
  SET(CMAKE_CUDA_FLAGS_INIT)
ENDIF(CMAKE_CUDA_FLAGS_INIT STREQUAL " ")
SET (CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS_INIT}" CACHE STRING
     "Flags used by the compiler during all build types.")

IF(NOT CMAKE_NOT_USING_CONFIG_FLAGS)
# default build type is none
  IF(NOT CMAKE_NO_BUILD_TYPE)
    SET (CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE_INIT} CACHE STRING 
      "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_CUDA_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
  ENDIF(NOT CMAKE_NO_BUILD_TYPE)
  SET (CMAKE_CUDA_FLAGS_DEBUG "${CMAKE_CUDA_FLAGS_DEBUG_INIT}" CACHE STRING
    "Flags used by the compiler during debug builds.")
  SET (CMAKE_CUDA_FLAGS_MINSIZEREL "${CMAKE_CUDA_FLAGS_MINSIZEREL_INIT}" CACHE STRING
    "Flags used by the compiler during release minsize builds.")
  SET (CMAKE_CUDA_FLAGS_RELEASE "${CMAKE_CUDA_FLAGS_RELEASE_INIT}" CACHE STRING
    "Flags used by the compiler during release builds (/MD /Ob1 /Oi /Ot /Oy /Gs will produce slightly less optimized but smaller files).")
  SET (CMAKE_CUDA_FLAGS_RELWITHDEBINFO "${CMAKE_CUDA_FLAGS_RELWITHDEBINFO_INIT}" CACHE STRING
    "Flags used by the compiler during Release with Debug Info builds.")
ENDIF(NOT CMAKE_NOT_USING_CONFIG_FLAGS)

IF(CMAKE_CUDA_STANDARD_LIBRARIES_INIT)
  SET(CMAKE_CUDA_STANDARD_LIBRARIES "${CMAKE_CUDA_STANDARD_LIBRARIES_INIT}"
    CACHE STRING "Libraries linked by defalut with all CUDA applications.")
  MARK_AS_ADVANCED(CMAKE_CUDA_STANDARD_LIBRARIES)
ENDIF(CMAKE_CUDA_STANDARD_LIBRARIES_INIT)

INCLUDE(CMakeCommonLanguageInclude)

SET(CMAKE_SHARED_LIBRARY_CUDA_FLAGS "")            # -pic 
SET(CMAKE_SHARED_LIBRARY_CREATE_CUDA_FLAGS "-shared")       # -shared
SET(CMAKE_SHARED_LIBRARY_LINK_CUDA_FLAGS "")         # +s, flag for exe link to use shared lib
SET(CMAKE_SHARED_LIBRARY_RUNTIME_CUDA_FLAG "")       # -rpath
SET(CMAKE_SHARED_LIBRARY_RUNTIME_CUDA_FLAG_SEP "")   # : or empty
SET(CMAKE_INCLUDE_FLAG_CUDA "-I")       # -I
SET(CMAKE_INCLUDE_FLAG_CUDA_SEP "")     # , or empty

# now define the following rule variables

# CMAKE_CUDA_CREATE_SHARED_LIBRARY
# CMAKE_CUDA_CREATE_SHARED_MODULE
# CMAKE_CUDA_COMPILE_OBJECT
# CMAKE_CUDA_LINK_EXECUTABLE

# variables supplied by the generator at use time
# <TARGET>
# <TARGET_BASE> the target without the suffix
# <OBJECTS>
# <OBJECT>
# <LINK_LIBRARIES>
# <FLAGS>
# <LINK_FLAGS>

# CUDA compiler information
# <CMAKE_CUDA_COMPILER>  
# <CMAKE_SHARED_LIBRARY_CREATE_CUDA_FLAGS>
# <CMAKE_SHARED_MODULE_CREATE_CUDA_FLAGS>
# <CMAKE_CUDA_LINK_FLAGS>

# create a CUDA shared library
IF(NOT CMAKE_CUDA_CREATE_SHARED_LIBRARY)
  SET(CMAKE_CUDA_CREATE_SHARED_LIBRARY
      "<CMAKE_CUDA_COMPILER> <CMAKE_SHARED_LIBRARY_CUDA_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CUDA_FLAGS> <CMAKE_SHARED_LIBRARY_SONAME_CUDA_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
ENDIF(NOT CMAKE_CUDA_CREATE_SHARED_LIBRARY)

# create a CUDA shared module just copy the shared library rule
IF(NOT CMAKE_CUDA_CREATE_SHARED_MODULE)
  SET(CMAKE_CUDA_CREATE_SHARED_MODULE ${CMAKE_CUDA_CREATE_SHARED_LIBRARY})
ENDIF(NOT CMAKE_CUDA_CREATE_SHARED_MODULE)

# Create a static archive incrementally for large object file counts.
# If CMAKE_CUDA_CREATE_STATIC_LIBRARY is set it will override these.
SET(CMAKE_CUDA_ARCHIVE_CREATE "<CMAKE_AR> cr <TARGET> <LINK_FLAGS> <OBJECTS>")
SET(CMAKE_CUDA_ARCHIVE_APPEND "<CMAKE_AR> r  <TARGET> <LINK_FLAGS> <OBJECTS>")
SET(CMAKE_CUDA_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")

# compile a CUDA file into an object file
IF(NOT CMAKE_CUDA_COMPILE_OBJECT)
  SET(CMAKE_CUDA_COMPILE_OBJECT
    "<CMAKE_CUDA_COMPILER> <DEFINES> <FLAGS> -o <OBJECT>   -c <SOURCE>")
ENDIF(NOT CMAKE_CUDA_COMPILE_OBJECT)

IF(NOT CMAKE_CUDA_LINK_EXECUTABLE)
  SET(CMAKE_CUDA_LINK_EXECUTABLE
    "<CMAKE_CUDA_COMPILER> <FLAGS> <CMAKE_CUDA_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
ENDIF(NOT CMAKE_CUDA_LINK_EXECUTABLE)

IF(NOT CMAKE_EXECUTABLE_RUNTIME_CUDA_FLAG)
  SET(CMAKE_EXECUTABLE_RUNTIME_CUDA_FLAG ${CMAKE_SHARED_LIBRARY_RUNTIME_CUDA_FLAG})
ENDIF(NOT CMAKE_EXECUTABLE_RUNTIME_CUDA_FLAG)

IF(NOT CMAKE_EXECUTABLE_RUNTIME_CUDA_FLAG_SEP)
  SET(CMAKE_EXECUTABLE_RUNTIME_CUDA_FLAG_SEP ${CMAKE_SHARED_LIBRARY_RUNTIME_CUDA_FLAG_SEP})
ENDIF(NOT CMAKE_EXECUTABLE_RUNTIME_CUDA_FLAG_SEP)

IF(NOT CMAKE_EXECUTABLE_RPATH_LINK_CUDA_FLAG)
  SET(CMAKE_EXECUTABLE_RPATH_LINK_CUDA_FLAG ${CMAKE_SHARED_LIBRARY_RPATH_LINK_CUDA_FLAG})
ENDIF(NOT CMAKE_EXECUTABLE_RPATH_LINK_CUDA_FLAG)

MARK_AS_ADVANCED(
CMAKE_CUDA_FLAGS
CMAKE_CUDA_FLAGS_DEBUG
CMAKE_CUDA_FLAGS_MINSIZEREL
CMAKE_CUDA_FLAGS_RELEASE
CMAKE_CUDA_FLAGS_RELWITHDEBINFO
)
SET(CMAKE_CUDA_INFORMATION_LOADED 1)


