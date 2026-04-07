cmake_minimum_required ( VERSION 3.2 )
project ( GpO VERSION 0.0.1)
set( GPO_VERSION_ABI 1 )

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC ")

if( "${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}" )
 message( FATAL "no in source building allowed." )
endif( )

list( APPEND CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/CMake
)

set( GPO_DESCRIPTION "GpO Base Project" )
set( GPO_LICENSE LGPL )

find_package(OpenGL REQUIRED)

add_subdirectory(libs/glfw EXCLUDE_FROM_ALL)
add_subdirectory(libs/glm )
add_subdirectory(libs/glad )

include_directories(libs/glad/include)
include_directories(libs/glm)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/KHR)
include_directories(${PROJECT_SOURCE_DIR}/include/stb)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_01.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_01 ${GPO_SOURCE_FILES})
target_link_libraries(gpo_01 PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_01_entrega.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_01_entrega ${GPO_SOURCE_FILES})
target_link_libraries(gpo_01_entrega PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_02.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_02 ${GPO_SOURCE_FILES})
target_link_libraries(gpo_02 PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_02_entrega.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_02_entrega ${GPO_SOURCE_FILES})
target_link_libraries(gpo_02_entrega PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_03.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_03 ${GPO_SOURCE_FILES})
target_link_libraries(gpo_03 PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_03_entrega.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_03_entrega ${GPO_SOURCE_FILES})
target_link_libraries(gpo_03_entrega PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_04.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_04 ${GPO_SOURCE_FILES})
target_link_libraries(gpo_04 PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_04_entrega.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_04_entrega ${GPO_SOURCE_FILES})
target_link_libraries(gpo_04_entrega PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 src/GPO_proyecto_final.cpp
					 src/GPO_aux.cpp)
add_executable(gpo_proyecto_final ${GPO_SOURCE_FILES})
target_link_libraries(gpo_proyecto_final PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface)

set_target_properties(gpo_01
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(gpo_01_entrega
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_target_properties(gpo_02
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(gpo_02_entrega
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_target_properties(gpo_03
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(gpo_03_entrega
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_target_properties(gpo_04
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(gpo_04_entrega
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_target_properties(gpo_proyecto_final
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)

file(GLOB MY_CONTENT "data/*")
file(COPY ${MY_CONTENT} DESTINATION "${CMAKE_BINARY_DIR}/bin/data")
