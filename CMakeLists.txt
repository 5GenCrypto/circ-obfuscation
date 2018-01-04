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
  src/obf_run.c
  src/util.c
  )
set(lin_SOURCES
  ${mio_SOURCES}
  src/lin/encoding.c
  src/lin/level.c
  src/lin/obf_params.c
  src/lin/obfuscator.c
  src/lin/public_params.c
  src/lin/secret_params.c
  )
set(lz_SOURCES
  ${mio_SOURCES}
  src/lz/encoding.c
  src/lz/obf_params.c
  src/lz/obfuscator.c
  src/lz/public_params.c
  src/lz/secret_params.c
  )
set(mife_SOURCES
  ${mio_SOURCES}
  src/mife/encoding.c
  src/mife/mife.c
  src/mife/mife_params.c
  src/mife/mife_run.c
  src/mife/public_params.c
  src/mife/secret_params.c
  )
set(mobf_SOURCES
  ${mio_SOURCES}
  src/mobf/obf_params.c
  src/mobf/obfuscator.c
  )

add_library(lz SHARED ${lz_SOURCES})
# add_library(lin SHARED ${lin_SOURCES})
# add_library(mife SHARED ${mife_SOURCES})
# add_library(mobf SHARED ${mobf_SOURCES})
add_executable(mio src/mio.c)
# target_link_libraries(mio lz lin mife mobf)
target_link_libraries(mio lz)
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
