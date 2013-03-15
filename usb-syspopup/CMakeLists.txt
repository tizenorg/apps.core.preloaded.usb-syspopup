CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(SRCS usb-syspopup.c)

SET(VENDOR "tizen")
SET(PACKAGE "usb-syspopup")
SET(PKGNAME "org.${VENDOR}.${PACKAGE}")
SET(PREFIX "${CMAKE_INSTALL_PREFIX}/apps/${PKGNAME}")
SET(BINDIR "${PREFIX}/bin")
SET(RESDIR "${PREFIX}/res")
SET(LOCALEDIR "${RESDIR}/locale")
SET(MANIFESTDIR "${CMAKE_INSTALL_PREFIX}/share/packages")

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
				appcore-efl
				bundle
				dlog
				ecore
				elementary
				evas
				syspopup
				devman
				appsvc
				pmapi
				vconf)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -O2 -g -Wall")

ADD_DEFINITIONS("-DPACKAGE=\"${PACKAGE}\"")
ADD_DEFINITIONS("-DLOCALEDIR=\"${LOCALEDIR}\"")

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} syspopup  ${pkgs_LDFLAGS})

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BINDIR})

# install manifest file
INSTALL(FILES ${CMAKE_SOURCE_DIR}/${PACKAGE}/${PKGNAME}.xml DESTINATION ${MANIFESTDIR})

ADD_SUBDIRECTORY(po)
