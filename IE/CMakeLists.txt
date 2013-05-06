cmake_minimum_required (VERSION 2.8)
project (AnchoFramework)

add_definitions(-DUNICODE -D_UNICODE)
option(DISABLE_REGISTRATION "Disable registration" OFF)

#-----------------------------------------
if(NOT BOOST_ROOT)
  set(BOOST_ROOT "BOOST_ROOT-NOT_FOUND" CACHE PATH "Where you installed boost?")
endif(NOT BOOST_ROOT)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON) 
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS filesystem thread)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  add_definitions(-DBOOST_THREAD_VERSION=3 -DBOOST_RESULT_OF_USE_TR1 -DBOOST_NO_RVALUE_REFERENCES -DBOOST_NO_CXX11_RVALUE_REFERENCES)
endif(Boost_FOUND)

option(ANCHO_WTL_SEARCH "Whether cmake should look for the WTL" OFF)
if(${ANCHO_WTL_SEARCH})
  find_path(WTL_INCLUDE_DIR "atlapp.h")
  include_directories(${WTL_INCLUDE_DIR})
endif(${ANCHO_WTL_SEARCH})
#-----------------------------------------


add_subdirectory(libbhohelper)
include_directories(${libbhohelper_INC_DIR})
link_directories(${libbhohelper_ARCHIVE_DIR})

add_subdirectory(Magpie)
add_dependencies(Magpie libbhohelper)
include_directories(${MagpieProject_INC_DIRS})

add_subdirectory(libprothandlers)
include_directories(${libprothandlers_INC_DIR})

add_subdirectory(passthruapp)
include_directories(${passthruapp_INC_DIR})

#Ancho components
add_subdirectory(anchocommons)
include_directories(${anchocommons_INC_DIR})

add_subdirectory(AnchoShared)
add_dependencies(AnchoShared Magpie anchocommons)
include_directories(${AnchoShared_INC_DIR})
include_directories(${AnchoShared_GENERATED_FILES_DIR})

add_subdirectory(AnchoBgSrv)
add_dependencies(AnchoBgSrv anchocommons Magpie libbhohelper libprothandlers AnchoShared)# passthruapp)
include_directories(${AnchoBgSrv_INC_DIR})

add_subdirectory(ancho)
add_dependencies(ancho anchocommons Magpie libbhohelper libprothandlers AnchoShared AnchoBgSrv) #also dependent on AnchoBgSrv - but this will often cause needless rebuild

set_target_properties(Magpie PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${${PROJECT_NAME}_BINARY_DIR}/")
set_target_properties(AnchoShared PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${${PROJECT_NAME}_BINARY_DIR}/")
set_target_properties(ancho PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${${PROJECT_NAME}_BINARY_DIR}/")
set_target_properties(AnchoBgSrv PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${${PROJECT_NAME}_BINARY_DIR}/")


#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
