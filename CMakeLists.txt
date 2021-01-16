# [open]aptx - CMakeLists.txt
# Copyright (c) 2017-2021 Arkadiusz Bokowy

cmake_minimum_required(VERSION 3.2)
project(openaptx
	VERSION 1.3.0
	DESCRIPTION "Reverse-engineered apt-X audio codec"
	HOMEPAGE_URL "https://github.com/Arkq/openaptx"
	LANGUAGES C)

option(ENABLE_DOC "Auto-generate manual files using Doxygen." OFF)
option(ENABLE_APTX422 "Build reverse-engineered library for apt-X encoding." OFF)
option(ENABLE_APTXHD100 "Build reverse-engineered library for apt-X HD encoding." OFF)
option(WITH_FFMPEG "Use FFmpeg as a backend for apt-X / apt-X HD libraries." OFF)
option(WITH_SNDFILE "Use sndfile for reading/writign audio files." OFF)

if(WITH_FFMPEG)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(FFLibAVCodec REQUIRED IMPORTED_TARGET libavcodec>=58.18.100)

	message(WARNING
		"Building apt-X / apt-X HD with FFmpeg: "
		"Compiling openaptx with FFmpeg as an encoding backend "
		"requires apt-X Adaptive API support in the client code. "
		"Otherwise, client will not be able to free resources "
		"allocated with the aptxbtenc_init() function.")

endif()

if(WITH_SNDFILE)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SNDFile REQUIRED IMPORTED_TARGET sndfile)
	add_definitions(-DWITH_SNDFILE=1)
endif()

include(GNUInstallDirs)

add_definitions(-DPACKAGE_NAME="${PROJECT_NAME}")
add_definitions(-DPACKAGE_VERSION="${PROJECT_VERSION}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(ENABLE_DOC)
	add_subdirectory(doc)
endif()
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(utils)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/openaptx.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/openaptx.pc
	@ONLY)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/openaptxhd.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/openaptxhd.pc
	@ONLY)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/openaptx.pc
	${CMAKE_CURRENT_BINARY_DIR}/openaptxhd.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
