cmake_minimum_required(VERSION 3.23)

SET(CMAKE_C_STANDARD 99)

project(h264bitstream
	VERSION 0.2.0
	HOMEPAGE_URL https://github.com/aizvorski/h264bitstream
	LANGUAGES C
)

include(GNUInstallDirs)

# Adding interface target for compiler flags
add_library(compile_options INTERFACE)
target_compile_options(compile_options INTERFACE -Wall -pedantic $<IF:$<CONFIG:Debug>,-O0,-O2>)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
	target_compile_options(compile_options INTERFACE -Wextra -Wshadow -Wwrite-strings -Wno-unused -g)
endif()

# Library sources
set(SOURCES
	h264_nal.c
	h264_sei.c
	h264_stream.c
)

set(HEADERS
	bs.h
	h264_avcc.h
	h264_sei.h
	h264_stream.h
)

# These do not participate in build -- for now...
set(ADDITIONALS
	h264_avcc.c
	h264_slice_data.c
	h264_slice_data.h
)

add_library(h264bitstream ${SOURCES} ${HEADERS})
target_sources(h264bitstream
	PUBLIC FILE_SET headers TYPE HEADERS FILES ${HEADERS}
)
target_link_libraries(h264bitstream PRIVATE compile_options)

add_executable(h264_analyze h264_analyze.c)
target_link_libraries(h264_analyze PRIVATE compile_options h264bitstream)

add_executable(svc_split svc_split.c)
target_link_libraries(svc_split PRIVATE compile_options h264bitstream)

install(TARGETS h264bitstream h264_analyze svc_split
	FILE_SET headers
)
