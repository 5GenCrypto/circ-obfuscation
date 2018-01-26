cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(mio VERSION 1.0.0 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

include(GNUInstallDirs)

set(mio_SOURCES
  src/circ_params.c
  src/index_set.c
  src/input_chunker.c
  src/mmap.c
  src/mife_run.c
  src/obf_run.c
  src/util.c
  )
set(lz_SOURCES
  ${mio_SOURCES}
  src/lz/encoding.c
  src/lz/obf_params.c
  src/lz/obfuscator.c
  src/lz/public_params.c
  src/lz/secret_params.c
  )
set(mife_cmr_SOURCES
  ${mio_SOURCES}
  src/mife-cmr/encoding.c
  src/mife-cmr/mife.c
  src/mife-cmr/mife_params.c
  src/mife-cmr/public_params.c
  src/mife-cmr/secret_params.c
  )
set(obf_cmr_SOURCES
  ${mio_SOURCES}
  src/obf-cmr/obf_params.c
  src/obf-cmr/obfuscator.c
  )
set(polylog_SOURCES
  ${mio_SOURCES}
  src/polylog/encoding.c
  src/polylog/extra.c
  src/polylog/obf_params.c
  src/polylog/obfuscator.c
  src/polylog/public_params.c
  src/polylog/secret_params.c
  src/polylog/wire.c
  )

add_library(lz SHARED ${lz_SOURCES})
add_library(mife_cmr SHARED ${mife_cmr_SOURCES})
add_library(obf_cmr SHARED ${obf_cmr_SOURCES})
add_library(polylog SHARED ${polylog_SOURCES})
add_executable(mio src/mio.c)
target_link_libraries(mio lz)
target_link_libraries(mio mife_cmr obf_cmr)
target_link_libraries(mio polylog)
find_library(libacirc acirc)
find_library(libmmap mmap)
find_library(libclt13 clt13)
find_library(libthreadpool threadpool)
find_library(libaesrand aesrand)
target_link_libraries(mio "${libacirc}" "${libmmap}" "${libclt13}" "${libthreadpool}" "${libaesrand}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-discarded-qualifiers -Werror -std=gnu11 -march=native")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg -ggdb -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

install(TARGETS mio DESTINATION bin)
