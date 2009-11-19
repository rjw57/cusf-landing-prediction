# Modified from CMakeCInformation.cmake

# determine the compiler to use for CUDA programs
# NOTE, a generator may set CMAKE_CUDA_COMPILER before
# loading this file to force a compiler.
# use environment variable CUDAC first if defined by user, next use 
# the cmake variable CMAKE_GENERATOR_NVCC which can be defined by a generator
# as a default compiler

IF(NOT CMAKE_CUDA_COMPILER)
  # prefer the environment variable NVCC
  IF($ENV{CUDAC} MATCHES ".+")
    GET_FILENAME_COMPONENT(CMAKE_CUDA_COMPILER_INIT $ENV{CUDAC} PROGRAM PROGRAM_ARGS CMAKE_CUDA_FLAGS_ENV_INIT)
    IF(CMAKE_CUDA_FLAGS_ENV_INIT)
      SET(CMAKE_CUDA_COMPILER_ARG1 "${CMAKE_CUDA_FLAGS_ENV_INIT}" CACHE STRING "First argument to CUDA compiler")
    ENDIF(CMAKE_CUDA_FLAGS_ENV_INIT)
    IF(EXISTS ${CMAKE_CUDA_COMPILER_INIT})
    ELSE(EXISTS ${CMAKE_CUDA_COMPILER_INIT})
      MESSAGE(FATAL_ERROR "Could not find compiler set in environment variable C:\n$ENV{CUDAC}.") 
    ENDIF(EXISTS ${CMAKE_CUDA_COMPILER_INIT})
  ENDIF($ENV{CUDAC} MATCHES ".+")

  # next try prefer the compiler specified by the generator
  IF(CMAKE_GENERATOR_NVCC) 
    IF(NOT CMAKE_CUDA_COMPILER_INIT)
      SET(CMAKE_CUDA_COMPILER_INIT ${CMAKE_GENERATOR_NVCC})
    ENDIF(NOT CMAKE_CUDA_COMPILER_INIT)
  ENDIF(CMAKE_GENERATOR_NVCC)

  # finally list compilers to try
  IF(CMAKE_CUDA_COMPILER_INIT)
    SET(CMAKE_CUDA_COMPILER_LIST ${CMAKE_CUDA_COMPILER_INIT})
  ELSE(CMAKE_CUDA_COMPILER_INIT)
    SET(CMAKE_CUDA_COMPILER_LIST nvcc)  
  ENDIF(CMAKE_CUDA_COMPILER_INIT)

  # Find the compiler.
  FIND_PROGRAM(CMAKE_CUDA_COMPILER NAMES ${CMAKE_CUDA_COMPILER_LIST} DOC "CUDA compiler")
  IF(CMAKE_CUDA_COMPILER_INIT AND NOT CMAKE_CUDA_COMPILER)
    SET(CMAKE_CUDA_COMPILER "${CMAKE_CUDA_COMPILER_INIT}" CACHE FILEPATH "CUDA compiler" FORCE)
  ENDIF(CMAKE_CUDA_COMPILER_INIT AND NOT CMAKE_CUDA_COMPILER)
ENDIF(NOT CMAKE_CUDA_COMPILER)
MARK_AS_ADVANCED(CMAKE_CUDA_COMPILER)  
GET_FILENAME_COMPONENT(COMPILER_LOCATION "${CMAKE_CUDA_COMPILER}" PATH)

# test to see if the d compiler is nvcc
SET(CMAKE_CUDA_COMPILER_IS_NVCC_RUN 1)
IF("${CMAKE_CUDA_COMPILER}" MATCHES ".*nvcc.*" )
  SET(CMAKE_CUDA_COMPILER_IS_NVCC_RUN 1)
  FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
    "Determining if the CUDA compiler is NVCC succeeded with "
    "the following output:\n${CMAKE_CUDA_COMPILER}\n\n")
ENDIF("${CMAKE_CUDA_COMPILER}" MATCHES ".*nvcc.*" )

# configure variables set in this file for fast reload later on
IF(EXISTS ${CMAKE_SOURCE_DIR}/cmake/CMakeCUDACompiler.cmake.in)
	CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/cmake/CMakeCUDACompiler.cmake.in 
               "${CMAKE_PLATFORM_ROOT_BIN}/CMakeCUDACompiler.cmake" IMMEDIATE)
ELSE(EXISTS ${CMAKE_SOURCE_DIR}/cmake/CMakeCUDACompiler.cmake.in)
	CONFIGURE_FILE(${CMAKE_ROOT}/Modules/CMakeCUDACompiler.cmake.in 
               "${CMAKE_PLATFORM_ROOT_BIN}/CMakeCUDACompiler.cmake" IMMEDIATE)
ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/cmake/CMakeCUDACompiler.cmake.in)

SET(CMAKE_CUDA_COMPILER_ENV_VAR "CUDAC")
