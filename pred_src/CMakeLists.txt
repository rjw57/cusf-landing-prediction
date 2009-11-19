# CMake build file for the CUSF landing prediction software
#
# Original version: Rich Wareham <rjw57@cam.ac.uk>

# Use PkgConfig to find the glib libraries
find_package(PkgConfig)

# Look for CUDA
find_package(CUDARuntime)

pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

# Create the configuration header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h @ONLY)

set(PRED_LINK_LIBRARIES ${GLIB_LIBRARIES} -lm)
if(CUDA_RUNTIME_FOUND)
	include_directories(${CUDA_RUNTIME_INCLUDE_DIR})
	set(PRED_LINK_LIBRARIES ${PRED_LINK_LIBRARIES} ${CUDA_RUNTIME_LIBRARIES})
endif(CUDA_RUNTIME_FOUND)

add_executable(pred
	config.h
	util/gopt.c
	util/getdelim.c
	util/random.h
	util/gopt.h
	util/getline.h
	util/getline.c
	util/getdelim.h
	util/random.c
	altitude.h
	wind/wind_file_cache.c
	wind/wind_file_cache.h
	wind/wind_file.c
	wind/wind_file.h
	altitude.c
	pred.c
	run_model.c
	pred.h
	run_model.h
	ini/iniparser.c
	ini/iniparser.h
	ini/dictionary.h
	ini/dictionary.c
)

target_link_libraries(pred ${PRED_LINK_LIBRARIES})
