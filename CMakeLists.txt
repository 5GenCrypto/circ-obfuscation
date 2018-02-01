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
  src/mmap.c
  src/mife_run.c
  src/obf_run.c
  src/util.c
  )
set(obf_lz_SOURCES
  ${mio_SOURCES}
  src/obf-lz/encoding.c
  src/obf-lz/obf_params.c
  src/obf-lz/obfuscator.c
  src/obf-lz/public_params.c
  src/obf-lz/secret_params.c
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
set(mife_gc_SOURCES
  ${mio_SOURCES}
  src/mife-gc/mife.c
  src/mife-gc/mife_params.c
  )
set(obf_polylog_SOURCES
  ${mio_SOURCES}
  src/obf-polylog/encoding.c
  src/obf-polylog/extra.c
  src/obf-polylog/obf_params.c
  src/obf-polylog/obfuscator.c
  src/obf-polylog/public_params.c
  src/obf-polylog/secret_params.c
  src/obf-polylog/wire.c
  )

add_library(obf_lz SHARED ${obf_lz_SOURCES})
add_library(mife_cmr SHARED ${mife_cmr_SOURCES})
add_library(obf_cmr SHARED ${obf_cmr_SOURCES})
# add_library(mife_gc SHARED ${mife_gc_SOURCES})
add_library(obf_polylog SHARED ${obf_polylog_SOURCES})
add_executable(mio src/mio.c)
target_link_libraries(mio obf_lz)
target_link_libraries(mio mife_cmr)
target_link_libraries(mio obf_cmr)
target_link_libraries(mio obf_polylog)
find_library(libacirc acirc)
find_library(libmmap mmap)
find_library(libclt13 clt13)
find_library(libthreadpool threadpool)
find_library(libaesrand aesrand)
target_link_libraries(mio "${libacirc}" "${libmmap}" "${libclt13}" "${libthreadpool}" "${libaesrand}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -std=gnu11 -march=native")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg -ggdb -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

install(TARGETS mio DESTINATION bin)
