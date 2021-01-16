# [open]aptx - CMakeLists.txt
# Copyright (c) 2017-2021 Arkadiusz Bokowy

include(FindDoxygen)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
	${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	@ONLY)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/man/man3/openaptx.h.3
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/man
	DEPENDS ${PROJECT_SOURCE_DIR}/include/openaptx.h
	COMMAND ${DOXYGEN_EXECUTABLE})

add_custom_target(doxygen ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/man/man3/openaptx.h.3)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/man/man3/openaptx.h.3
	DESTINATION ${CMAKE_INSTALL_MANDIR}/man3)
