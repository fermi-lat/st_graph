cmake_minimum_required(VERSION 3.11)
include(GNUInstallDirs)
include(FetchContent)

project(st_graph
  VERSION 0.0.0
  LANGUAGES CXX
  )

###############################################################################
#
# External dependencies managed through fetchcontent
#
###############################################################################

fetchcontent_declare(st_stream
  GIT_REPOSITORY https://github.com/fermi-lat/st_stream.git
  GIT_TAG standalone
)

fetchcontent_getproperties(st_stream)
if(NOT st_stream_POPULATED)
  fetchcontent_populate(st_stream)
  add_subdirectory(${st_stream_SOURCE_DIR} ${st_stream_BINARY_DIR})
endif()

fetchcontent_declare(hoops
  GIT_REPOSITORY https://github.com/fermi-lat/hoops.git
  GIT_TAG standalone
)

fetchcontent_getproperties(hoops)
if(NOT hoops_POPULATED)
  fetchcontent_populate(hoops)
  add_subdirectory(${hoops_SOURCE_DIR} ${hoops_BINARY_DIR})
endif()

find_package(Python2 COMPONENTS Development)

###############################################################################


###############################################################################
#
# ST graph targets
#
###############################################################################

add_library(st_graph STATIC
  src/Axis.cxx
  src/Engine.cxx
  src/IPlot.cxx
  src/MPLEngine.cxx
  src/MPLFrame.cxx
  src/MPLPlot.cxx
  src/MPLPlotFrame.cxx
  src/MPLTabFolder.cxx
  src/StGui.cxx
)
add_library(ST::graph ALIAS st_graph)
target_include_directories(st_graph
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(st_graph
  PRIVATE
    ST::hoops
    ST::stream
    Python2::Python
)


set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/st_graph)

install(DIRECTORY st_graph DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS st_graph
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

install(TARGETS st_graph
  EXPORT st_graphTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

install(EXPORT st_graphTargets
  NAMESPACE    ST::
  DESTINATION  ${INSTALL_CONFIGDIR}
  )

#Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/st_graphConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/st_graphConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/st_graphConfig.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/st_graphConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/st_graphConfigVersion.cmake
  DESTINATION ${INSTALL_CONFIGDIR}
)

export(EXPORT st_graphTargets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/st_graphTargets.cmake
  NAMESPACE ST::
)

#Register package in user's package registry
export(PACKAGE st_graph)
