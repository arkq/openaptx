# [open]aptx - CMakeLists.txt
# Copyright (c) 2017-2024 Arkadiusz Bokowy

add_library(aptx SHARED)
set_target_properties(aptx PROPERTIES
	PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/../include/openaptx.h)

if(ENABLE_APTX422)
	add_library(aptx-4.2.2 SHARED
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/encode.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/params.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/processor.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/qmf.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/quantizer.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/search.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx422/main.c)
	target_compile_features(aptx-4.2.2 PRIVATE c_std_11)
	install(TARGETS aptx-4.2.2
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(ENABLE_APTXHD100)
	add_library(aptxHD-1.0.0 SHARED
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/encode.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/params.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/processor.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/qmf.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/quantizer.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/search.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptxhd100/main.c)
	target_compile_features(aptxHD-1.0.0 PRIVATE c_std_11)
	install(TARGETS aptxHD-1.0.0
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(WITH_FFMPEG)

	target_sources(aptx
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/aptx-ffmpeg.c)
	target_link_libraries(aptx PRIVATE PkgConfig::FFLibAVCodec)

else()

	add_executable(bin2array
		${CMAKE_CURRENT_SOURCE_DIR}/sample/bin2array.c)

	set(SAMPLE_SONAR_APTX "${CMAKE_CURRENT_SOURCE_DIR}/sample/sonar.aptx")
	set(SAMPLE_SONAR_APTX_HD "${CMAKE_CURRENT_SOURCE_DIR}/sample/sonar.aptxhd")

	add_custom_command(
		DEPENDS bin2array ${SAMPLE_SONAR_APTX}
		COMMAND bin2array sample_sonar_aptx ${SAMPLE_SONAR_APTX} > sample-sonar.c
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sample-sonar.c)

	add_custom_command(
		DEPENDS bin2array ${SAMPLE_SONAR_APTX_HD}
		COMMAND bin2array sample_sonar_aptx_hd ${SAMPLE_SONAR_APTX_HD} > sample-sonar-hd.c
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sample-sonar-hd.c)

	target_sources(aptx PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/sample-sonar.c
		${CMAKE_CURRENT_BINARY_DIR}/sample-sonar-hd.c
		${CMAKE_CURRENT_SOURCE_DIR}/aptx-stub.c)

	if(ENABLE_APTX422)
		# use reverse-engineered library as an encoding backend
		target_link_libraries(aptx PRIVATE aptx-4.2.2)
	endif()

	if(ENABLE_APTXHD100)
		# use reverse-engineered library as an encoding backend
		target_link_libraries(aptx PRIVATE aptxHD-1.0.0)
	endif()

endif()

install(TARGETS aptx
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
