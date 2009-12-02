# CMake project file for CU Spaceflight landing prediction.

# Update the module path to include any extra CMAKE modiles we might ship.
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

PROJECT(CUSFLANDPRED C CXX)

# Set the minimum required version of cmake
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Enable warnings and make them errors
# Also enable optimisation
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O9 -Wall -Werror")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O9 -Wall -Werror")

# On x86_64 we need to compile with -fPIC
IF(UNIX AND NOT WIN32)
  FIND_PROGRAM(CMAKE_UNAME uname /bin /usr/bin /usr/local/bin )
  IF(CMAKE_UNAME)
    EXEC_PROGRAM(uname ARGS -m OUTPUT_VARIABLE CMAKE_SYSTEM_PROCESSOR)
    SET(CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR} 
    	CACHE INTERNAL "processor type (i386 and x86_64)")
    IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
      ADD_DEFINITIONS(-fPIC)
    ENDIF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  ENDIF(CMAKE_UNAME)
ENDIF(UNIX AND NOT WIN32)

# The landing predictor itself
add_subdirectory(pred_src)

# Any test targets
add_subdirectory(test)
