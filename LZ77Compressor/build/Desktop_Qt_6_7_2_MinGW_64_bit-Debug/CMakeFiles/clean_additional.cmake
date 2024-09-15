# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\LZ77Compressor_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\LZ77Compressor_autogen.dir\\ParseCache.txt"
  "LZ77Compressor_autogen"
  )
endif()
