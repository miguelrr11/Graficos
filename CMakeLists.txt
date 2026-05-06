cmake_minimum_required ( VERSION 3.5 )
project ( GpO VERSION 0.0.1)
set( GPO_VERSION_ABI 1 )

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC ")



list( APPEND CMAKE_MODULE_PATH
 ${PROJECT_SOURCE_DIR}/CMake
)

set( GPO_DESCRIPTION "GpO Base Project" )
set( GPO_LICENSE LGPL )

find_package(OpenGL REQUIRED)

add_subdirectory(libs/glfw EXCLUDE_FROM_ALL)
add_subdirectory(libs/glm )
add_subdirectory(libs/glad )

# ── Assimp (OBJ importer only, no tools, no exporter, no tests) ──────────────
include(FetchContent)
set(ASSIMP_BUILD_TESTS                OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS         OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                    OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT                  ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER         ON  CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS                 OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(assimp)

include_directories(libs/glad/include)
include_directories(libs/glm)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/KHR)
include_directories(${PROJECT_SOURCE_DIR}/include/stb)

# SoLoud
include_directories(${PROJECT_SOURCE_DIR}/libs/soloud/include)
include_directories(${PROJECT_SOURCE_DIR}/libs/soloud/src/audiosource/wav)

file(GLOB SOLOUD_CORE_SRC     libs/soloud/src/core/*.cpp)
file(GLOB SOLOUD_BACKEND_SRC  libs/soloud/src/backend/miniaudio/*.cpp)
file(GLOB SOLOUD_WAV_SRC      libs/soloud/src/audiosource/wav/*.cpp
                               libs/soloud/src/audiosource/wav/*.c)

set(SOLOUD_SRC ${SOLOUD_CORE_SRC} ${SOLOUD_BACKEND_SRC} ${SOLOUD_WAV_SRC})

set(GPO_SOURCE_FILES include/stb/stb_image.h
					 include/GpO.h
					 include/obstacle.h
					 include/level.h
					 include/game.h
					 include/sphere.h
					 include/wallmesh.h
					 include/floormesh.h
					 include/generator.h
					 include/particles.h
					 include/model.h
					 src/wallmesh.cpp
					 src/sphere.cpp
					 src/level.cpp
					 src/game.cpp
					 src/GPO_proyecto_final.cpp
					 src/obstacle.cpp
					 src/GPO_aux.cpp
					 src/generator.cpp
					 src/floormesh.cpp
					 src/particles.cpp
					 src/model.cpp
					 ${SOLOUD_SRC}
					 )
add_executable(gpo_proyecto_final ${GPO_SOURCE_FILES})
target_compile_definitions(gpo_proyecto_final PRIVATE WITH_MINIAUDIO)
target_include_directories(gpo_proyecto_final PRIVATE
    ${PROJECT_SOURCE_DIR}/libs/soloud/src/backend/miniaudio
    ${assimp_SOURCE_DIR}/include
    ${assimp_BINARY_DIR}/include)

if(WIN32)
    target_link_libraries(gpo_proyecto_final PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface assimp)
elseif(APPLE)
    target_link_libraries(gpo_proyecto_final PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface assimp
        "-framework AudioUnit" "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
else()
    target_link_libraries(gpo_proyecto_final PRIVATE ${OPENGL_LIBRARIES} glfw glm glad_interface assimp
        pthread dl m)
endif()

set_target_properties(gpo_proyecto_final
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)
    file(GLOB MY_CONTENT "data/*")
    file(COPY ${MY_CONTENT} DESTINATION "${CMAKE_BINARY_DIR}/bin/data")
endif()
