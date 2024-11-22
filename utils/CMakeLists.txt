# [open]aptx - CMakeLists.txt
# Copyright (c) 2017-2024 Arkadiusz Bokowy

if(ENABLE_APTX_DECODER_API)

	add_executable(aptxdec ${CMAKE_CURRENT_SOURCE_DIR}/aptxdec.c)
	target_link_libraries(aptxdec aptx)

	add_executable(aptxhddec ${CMAKE_CURRENT_SOURCE_DIR}/aptxdec.c)
	target_compile_definitions(aptxhddec PRIVATE -DAPTXHD=1)
	target_link_libraries(aptxhddec aptx)

	if(WITH_SNDFILE)
		target_link_libraries(aptxdec PkgConfig::SNDFile)
		target_link_libraries(aptxhddec PkgConfig::SNDFile)
	endif()

	install(TARGETS
		aptxdec aptxhddec
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif()

if(ENABLE_APTX_ENCODER_API)

	add_executable(aptxenc ${CMAKE_CURRENT_SOURCE_DIR}/aptxenc.c)
	target_link_libraries(aptxenc aptx)

	add_executable(aptxhdenc ${CMAKE_CURRENT_SOURCE_DIR}/aptxenc.c)
	target_compile_definitions(aptxhdenc PRIVATE -DAPTXHD=1)
	target_link_libraries(aptxhdenc aptx)

	if(WITH_SNDFILE)
		target_link_libraries(aptxenc PkgConfig::SNDFile)
		target_link_libraries(aptxhdenc PkgConfig::SNDFile)
	endif()

	install(TARGETS
		aptxenc aptxhdenc
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif()
