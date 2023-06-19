cmake_minimum_required(VERSION 3.14)
project(meshimport)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${meshimport_SOURCE_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${meshimport_SOURCE_DIR}")

include_directories(${meshimport_SOURCE_DIR})

add_definitions(-DNX64)
add_definitions(-DLINUX)
add_definitions(-DLINUX_GENERIC)

file(GLOB meshimport_app_MeshConvert___cpp "${meshimport_SOURCE_DIR}/app/MeshConvert/*.cpp")
file(GLOB meshimport_include_MeshImport___cpp "${meshimport_SOURCE_DIR}/include/MeshImport/*.cpp")
file(GLOB meshimport_include_MeshImport___h "${meshimport_SOURCE_DIR}/include/MeshImport/*.h")
file(GLOB meshimport_include_common_binding___cpp "${meshimport_SOURCE_DIR}/include/common/binding/*.cpp")
file(GLOB meshimport_include_common_snippets___cpp "${meshimport_SOURCE_DIR}/include/common/snippets/*.cpp")
file(GLOB meshimport_src_MeshImport___cpp "${meshimport_SOURCE_DIR}/src/MeshImport/*.cpp")
file(GLOB meshimport_src_MeshImportARM___cpp "${meshimport_SOURCE_DIR}/src/MeshImportARM/*.cpp")
file(GLOB meshimport_src_MeshImportEzm___cpp "${meshimport_SOURCE_DIR}/src/MeshImportEzm/*.cpp")
file(GLOB meshimport_src_MeshImportFBX___cpp "${meshimport_SOURCE_DIR}/src/MeshImportFBX/*.cpp")
file(GLOB meshimport_src_MeshImportObj___cpp "${meshimport_SOURCE_DIR}/src/MeshImportObj/*.cpp")
file(GLOB meshimport_src_MeshImportOgre___cpp "${meshimport_SOURCE_DIR}/src/MeshImportOgre/*.cpp")
file(GLOB meshimport_src_MeshImportPSK___cpp "${meshimport_SOURCE_DIR}/src/MeshImportPSK/*.cpp")
file(GLOB meshimport_src_MeshImportTest___cpp "${meshimport_SOURCE_DIR}/src/MeshImportTest/*.cpp")

add_library(MeshImport SHARED
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_include_MeshImport___cpp}
  ${meshimport_src_MeshImport___cpp}
  ${meshimport_src_MeshImport___h}
  )
target_include_directories(MeshImport PUBLIC ${meshimport_SOURCE_DIR}/include/common/snippets)
target_include_directories(MeshImport PUBLIC ${meshimport_SOURCE_DIR}/include/MeshImport)

if(FALSE) # Requires APEX SDK; figure out later.
add_library(MeshImportARM MODULE
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_src_MeshImportARM___cpp}
  )
target_link_libraries(MeshImportARM MeshImport)
endif()

add_library(MeshImportEzm MODULE
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_src_MeshImportEzm___cpp}
  )
target_link_libraries(MeshImportEzm MeshImport)

add_library(MeshImportObj MODULE
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_src_MeshImportObj___cpp}
  )
target_link_libraries(MeshImportObj MeshImport)

add_library(MeshImportOgre MODULE
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_src_MeshImportOgre___cpp}
  )
target_link_libraries(MeshImportOgre MeshImport)

add_library(MeshImportPSK MODULE
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_src_MeshImportPSK___cpp}
  )
target_link_libraries(MeshImportPSK MeshImport)

add_executable(MeshImportTest
  ${meshimport_include_common_snippets___cpp}
  ${meshimport_src_MeshImportTest___cpp}
  )
target_link_libraries(MeshImportTest MeshImport)
