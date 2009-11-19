# - try to find Sesame library and include files
#  CUDA_RUNTIME_INCLUDE_DIR, where to find cuda.h, etc.
#  CUDA_RUNTIME_LIBRARIES, the libraries to link against
#  CUDA_RUNTIME_FOUND, If false, do not try to use CUDA_RUNTIME.
# Also defined, but not for general use are:
#  CUDA_RUNTIME_cudart_LIBRARY = the full path to the cudart library.

FIND_PATH( CUDA_RUNTIME_INCLUDE_DIR cuda_runtime_api.h
	/usr/include
	/usr/include/cuda
	/usr/local/include
	/usr/local/include/cuda
	/usr/local/cuda/include
	/opt/cuda/include
)

FIND_LIBRARY( CUDA_RUNTIME_cudart_LIBRARY cudart
	/usr/lib
	/usr/lib/cuda
	/usr/local/lib
	/usr/local/lib/cuda
	/usr/local/cuda/lib
	/opt/cuda/lib
)

SET( CUDA_RUNTIME_FOUND "NO" )
IF(CUDA_RUNTIME_INCLUDE_DIR)
	IF(CUDA_RUNTIME_cudart_LIBRARY)
		SET( CUDA_RUNTIME_LIBRARIES "${CUDA_RUNTIME_cudart_LIBRARY}")
		SET( CUDA_RUNTIME_FOUND "YES" )
	ENDIF(CUDA_RUNTIME_cudart_LIBRARY)
ENDIF(CUDA_RUNTIME_INCLUDE_DIR)

MARK_AS_ADVANCED(
	CUDA_RUNTIME_cudart_LIBRARY
)

# vim:sw=4:ts=4:autoindent

