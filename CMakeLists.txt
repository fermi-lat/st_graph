add_library(
  st_graph STATIC
  src/Axis.cxx
  src/EmbedPython.cpp
  src/Engine.cxx
  src/IPlot.cxx
  src/MPLEngine.cxx
  src/MPLFrame.cxx
  src/MPLPlot.cxx
  src/MPLPlotFrame.cxx
  src/MPLTabFolder.cxx
  src/StGui.cxx
)

target_include_directories(
  st_graph PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:>
)

target_link_libraries(
  st_graph
  PRIVATE embed_python hoops
  PUBLIC st_stream
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_compile_definitions(st_graph PRIVATE TRAP_FPE)
endif()

add_executable(test_st_graph src/test/test_st_graph.cxx)
target_link_libraries(test_st_graph PRIVATE st_graph hoops)

###############################################################
# Installation
###############################################################
install(DIRECTORY st_graph DESTINATION ${FERMI_INSTALL_INCLUDEDIR})
install(DIRECTORY pfiles/ DESTINATION ${FERMI_INSTALL_PFILESDIR})

install(
  TARGETS st_graph test_st_graph
  EXPORT fermiTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
