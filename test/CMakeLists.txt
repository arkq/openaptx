# [open]aptx - CMakeLists.txt
# Copyright (c) 2017-2021 Arkadiusz Bokowy

if(ENABLE_APTX422)

	add_library(qualcomm_libaptx SHARED IMPORTED)
	set_target_properties(qualcomm_libaptx PROPERTIES
		IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/3rdparty/libbt-aptX-ARM-4.2.2.so)

	add_executable(heval422 EXCLUDE_FROM_ALL
		${PROJECT_SOURCE_DIR}/src/aptx422/encode.c
		${PROJECT_SOURCE_DIR}/src/aptx422/params.c
		${PROJECT_SOURCE_DIR}/src/aptx422/processor.c
		${PROJECT_SOURCE_DIR}/src/aptx422/qmf.c
		${PROJECT_SOURCE_DIR}/src/aptx422/quantizer.c
		${PROJECT_SOURCE_DIR}/src/aptx422/search.c
		${CMAKE_CURRENT_SOURCE_DIR}/inspect-422.c
		${CMAKE_CURRENT_SOURCE_DIR}/inspect-utils.c
		${CMAKE_CURRENT_SOURCE_DIR}/heval-422.c)
	target_link_libraries(heval422 qualcomm_libaptx)

endif()

if(ENABLE_APTXHD100)

	add_library(qualcomm_libaptxHD SHARED IMPORTED)
	set_target_properties(qualcomm_libaptxHD PROPERTIES
		IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/3rdparty/libaptXHD_encoder.so)

	add_executable(hevalhd100 EXCLUDE_FROM_ALL
		${PROJECT_SOURCE_DIR}/src/aptxhd100/encode.c
		${PROJECT_SOURCE_DIR}/src/aptxhd100/params.c
		${PROJECT_SOURCE_DIR}/src/aptxhd100/processor.c
		${PROJECT_SOURCE_DIR}/src/aptxhd100/qmf.c
		${PROJECT_SOURCE_DIR}/src/aptxhd100/quantizer.c
		${PROJECT_SOURCE_DIR}/src/aptxhd100/search.c
		${CMAKE_CURRENT_SOURCE_DIR}/inspect-hd100.c
		${CMAKE_CURRENT_SOURCE_DIR}/inspect-utils.c
		${CMAKE_CURRENT_SOURCE_DIR}/heval-hd100.c)
	target_link_libraries(heval422 qualcomm_libaptxHD)

endif()
