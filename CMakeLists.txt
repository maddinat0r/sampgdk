project(sampgdk)

cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0022 NEW)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

option(SAMPGDK_STATIC "Build static library instead" OFF)
option(SAMPGDK_BUILD_PLUGINS "Build example plugins" OFF)
option(SAMPGDK_BUILD_AMALGAMATION "Build amalgamation" OFF)

set(SAMPGDK_VERSION_MAJOR 4)
set(SAMPGDK_VERSION_MINOR 1)
set(SAMPGDK_VERSION_PATCH 0)

set(SAMPGDK_VERSION ${SAMPGDK_VERSION_MAJOR})
set(SAMPGDK_VERSION ${SAMPGDK_VERSION}.${SAMPGDK_VERSION_MINOR})
set(SAMPGDK_VERSION ${SAMPGDK_VERSION}.${SAMPGDK_VERSION_PATCH})
set(SAMPGDK_VERSION ${SAMPGDK_VERSION}${SAMPGDK_VERSION_SUFFIX})

math(EXPR SAMPGDK_VERSION_ID "${SAMPGDK_VERSION_MAJOR} << 24 |
                              ${SAMPGDK_VERSION_MINOR} << 16 |
                              ${SAMPGDK_VERSION_PATCH} << 8")

if(NOT (MSVC_IDE AND MSVC_VERSION LESS 1600))
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

find_package(SDK REQUIRED)
find_package(PythonInterp 2.7 REQUIRED)

include_directories(
  ${SDK_INCLUDE_DIR}
  ${SDK_INCLUDE_DIR}/amx
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_BINARY_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
)

macro(sampgdk_add_subproject name)
  add_subdirectory(${name})
  file(RELATIVE_PATH folder ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  set_property(TARGET ${name} PROPERTY FOLDER ${folder})
endmacro()

macro(sampgdk_get_property name var)
  get_property(${var} DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY ${name})
endmacro()

macro(sampgdk_set_property name value)
  set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY ${name} ${value})
endmacro()

macro(sampgdk_append_property name value)
  set_property(DIRECTORY ${PROJECT_SOURCE_DIR} APPEND PROPERTY ${name} ${value})
endmacro()

macro(sampgdk_add_sources)
  foreach(file ${ARGN})
    if(NOT IS_ABSOLUTE ${file})
      sampgdk_append_property(SAMPGDK_SOURCES
                              ${CMAKE_CURRENT_SOURCE_DIR}/${file})
    else()
      sampgdk_append_property(SAMPGDK_SOURCES ${file})
    endif()
  endforeach()
endmacro()

macro(sampgdk_get_sources var)
  sampgdk_get_property(SAMPGDK_SOURCES ${var})
endmacro()

macro(sampgdk_add_headers)
  foreach(file ${ARGN})
    if(NOT IS_ABSOLUTE ${file})
      sampgdk_append_property(SAMPGDK_HEADERS
                              ${CMAKE_CURRENT_SOURCE_DIR}/${file})
    else()
      sampgdk_append_property(SAMPGDK_HEADERS ${file})
    endif()
  endforeach()
endmacro()

macro(sampgdk_get_headers var)
  sampgdk_get_property(SAMPGDK_HEADERS ${var})
endmacro()

macro(sampgdk_add_libraries)
  foreach(library ${ARGN})
    sampgdk_append_property(SAMPGDK_LIBRARIES ${library})
  endforeach()
endmacro()

macro(sampgdk_get_libraries var)
  sampgdk_get_property(SAMPGDK_LIBRARIES ${var})
endmacro()

macro(sampgdk_make_output_dir file)
  if(NOT IS_ABSOLUTE file)
    get_filename_component(_file ${file} ABSOLUTE)
  else()
    set(_file ${file})
  endif()

  get_filename_component(_path ${_file} PATH)
  if(_path MATCHES ${PROJECT_BINARY_DIR})
    file(RELATIVE_PATH _path ${CMAKE_CURRENT_BINARY_DIR} ${_path})
  else()
    file(RELATIVE_PATH _path ${CMAKE_CURRENT_SOURCE_DIR} ${_path})
  endif()

  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_path})
endmacro()

macro(sampgdk_add_api_gen module_name idl_file api_file)
  sampgdk_make_output_dir(${api_file})
  add_custom_command(
    OUTPUT ${api_file}
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_BINARY_DIR}/scripts/python.py
      ${PROJECT_SOURCE_DIR}/scripts/generate_code.py
      --module=${module_name}
      --idl=${idl_file}
      --api=${api_file}
    DEPENDS ${idl_file} ${PROJECT_SOURCE_DIR}/scripts/generate_code.py)
  sampgdk_add_sources(${api_file})
endmacro()

macro(sampgdk_add_source_gen module_name idl_file source_file)
  sampgdk_make_output_dir(${source_file})
  add_custom_command(
    OUTPUT ${source_file}
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_BINARY_DIR}/scripts/python.py
      ${PROJECT_SOURCE_DIR}/scripts/generate_code.py
      --module=${module_name}
      --idl=${idl_file}
      --source=${source_file}
    DEPENDS ${idl_file} ${PROJECT_SOURCE_DIR}/scripts/generate_code.py)
  sampgdk_add_sources(${source_file})
endmacro()

macro(sampgdk_add_header_gen module_name idl_file header_file)
  sampgdk_make_output_dir(${header_file})
  add_custom_command(
    OUTPUT ${header_file}
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_BINARY_DIR}/scripts/python.py
      ${PROJECT_SOURCE_DIR}/scripts/generate_code.py
      --module=${module_name}
      --idl=${idl_file}
      --header=${header_file}
    DEPENDS ${idl_file} ${PROJECT_SOURCE_DIR}/scripts/generate_code.py)
  sampgdk_add_headers(${header_file})
endmacro()

macro(sampgdk_add_init_gen target list_file source_file)
  sampgdk_make_output_dir(${source_file})
  add_custom_target(
    ${target}
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_BINARY_DIR}/scripts/python.py
      ${PROJECT_SOURCE_DIR}/scripts/generate_init.py
      --list=${list_file}
      --source=${source_file}
    DEPENDS ${list_file} ${PROJECT_SOURCE_DIR}/scripts/generate_init.py)
  sampgdk_add_sources(${source_file})
endmacro()

macro(sampgdk_add_exports_gen format output_file)
  add_custom_command(
    OUTPUT ${output_file}
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_BINARY_DIR}/scripts/python.py
      ${PROJECT_SOURCE_DIR}/scripts/generate_exports.py
      --format=${format}
      --output=${output_file}
      ${ARGN}
    DEPENDS ${ARGN} ${PROJECT_SOURCE_DIR}/scripts/generate_exports.py)
endmacro()

add_subdirectory(cmake)
add_subdirectory(doc)
add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(scripts)
add_subdirectory(src)
if(SAMPGDK_BUILD_PLUGINS)
  add_subdirectory(plugins)
endif()

sampgdk_get_sources(SAMPGDK_SOURCES)

foreach(file IN LISTS SAMPGDK_SOURCES)
  get_filename_component(extension ${file} EXT)

  if(extension STREQUAL ".idl")
    file(RELATIVE_PATH path ${PROJECT_SOURCE_DIR}/src ${file})

    get_filename_component(dir ${path} PATH)
    get_filename_component(name ${file} NAME_WE)

    sampgdk_add_api_gen(${name} ${file}
      ${PROJECT_BINARY_DIR}/src/${dir}/${name}.api)
    sampgdk_add_source_gen(${name} ${file}
    ${PROJECT_BINARY_DIR}/src/${dir}/${name}.c)
    sampgdk_add_header_gen(${name} ${file}
      ${PROJECT_BINARY_DIR}/include/sampgdk/${dir}/${name}.h)

    list(APPEND SAMPGDK_IDLS ${file})
  endif()
endforeach()

sampgdk_get_sources(SAMPGDK_SOURCES)

foreach(file IN LISTS SAMPGDK_SOURCES)
  get_filename_component(extension ${file} EXT)
  if(extension STREQUAL ".api")
    list(APPEND SAMPGDK_APIS ${file})
  endif()
endforeach()

if(SAMPGDK_DEF_FILE)
  sampgdk_add_exports_gen("def" ${SAMPGDK_DEF_FILE} ${SAMPGDK_APIS})
endif()

if(SAMPGDK_SYM_FILE)
  sampgdk_add_exports_gen("list" ${SAMPGDK_SYM_FILE} ${SAMPGDK_APIS})
endif()

sampgdk_get_sources(SAMPGDK_SOURCES)
sampgdk_get_headers(SAMPGDK_HEADERS)

list(REMOVE_ITEM SAMPGDK_SOURCES ${SAMPGDK_IDLS})

if(SAMPGDK_STATIC)
  add_library(sampgdk STATIC ${SAMPGDK_HEADERS} ${SAMPGDK_SOURCES})
else()
  add_library(sampgdk SHARED ${SAMPGDK_HEADERS} ${SAMPGDK_SOURCES})
endif()

get_target_property(SAMPGDK_OUTPUT_NAME sampgdk OUTPUT_NAME)

if(NOT SAMPGDK_OUTPUT_NAME)
  set(SAMPGDK_OUTPUT_NAME sampgdk)
endif()

if(WIN32)
  set(SAMPGDK_OUTPUT_NAME ${SAMPGDK_OUTPUT_NAME}${SAMPGDK_VERSION_MAJOR})
  if(SAMPGDK_STATIC)
    set(SAMPGDK_OUTPUT_NAME ${SAMPGDK_OUTPUT_NAME}s)
  endif()
endif()

set_target_properties(sampgdk PROPERTIES
  VERSION       ${SAMPGDK_VERSION}
  SOVERSION     ${SAMPGDK_VERSION_MAJOR}
  DEFINE_SYMBOL IN_SAMPGDK
  OUTPUT_NAME   ${SAMPGDK_OUTPUT_NAME}
  DEBUG_POSTFIX d
)

if(SAMPGDK_STATIC)
  target_compile_definitions(sampgdk PUBLIC SAMPGDK_STATIC)
endif()

if(WIN32)
  target_compile_definitions(sampgdk PRIVATE WIN32_LEAN_AND_MEAN)
elseif(UNIX)
  target_compile_definitions(sampgdk PRIVATE _GNU_SOURCE)
endif()

if(MSVC)
  target_compile_definitions(sampgdk PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX)
  target_compile_options(sampgdk PRIVATE -fvisibility=hidden)
  if(NOT SAMPGDK_STATIC)
    target_link_libraries(sampgdk PRIVATE
                  -Wl,--no-undefined,--retain-symbols-file=${SAMPGDK_SYM_FILE})
  endif()
endif()

if(UNIX)
  sampgdk_add_libraries(${CMAKE_DL_LIBS} rt)
endif()

sampgdk_get_libraries(SAMPGDK_LIBRARIES)

if(SAMPGDK_STATIC)
  target_link_libraries(sampgdk INTERFACE ${SAMPGDK_LIBRARIES})
else()
  target_link_libraries(sampgdk PRIVATE ${SAMPGDK_LIBRARIES})
endif()

if(SAMPGDK_BUILD_AMALGAMATION)
  foreach(file IN LISTS SAMPGDK_SOURCES)
    if (file MATCHES ".*\\.c" OR file MATCHES ".*\\.h")
      list(APPEND SAMPGDK_AMALGAMATE_FLAGS --source "${file}")
    endif()
  endforeach()
  foreach(file IN LISTS SAMPGDK_HEADERS)
    if (file MATCHES ".*\\.h")
      list(APPEND SAMPGDK_AMALGAMATE_FLAGS --header "${file}")
    endif()
  endforeach()

  add_custom_target(sampgdk_amalgam ALL
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/scripts/amalgamate.py
      --include-dir ${PROJECT_SOURCE_DIR}/include
      --include-dir ${PROJECT_BINARY_DIR}/include
      --include-dir ${PROJECT_SOURCE_DIR}/src
      --output-source ${PROJECT_BINARY_DIR}/src/sampgdk.c
      --output-header ${PROJECT_BINARY_DIR}/src/sampgdk.h
      --preamble ${PROJECT_SOURCE_DIR}/src/sampgdk.c
      ${SAMPGDK_AMALGAMATE_FLAGS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generating amalgamation")
  add_dependencies(sampgdk_amalgam sampgdk)

  add_custom_target(sampgdk_amalgam_package ALL
    COMMAND ${PYTHON_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/scripts/zip_files.py
      ${PROJECT_BINARY_DIR}/sampgdk-${SAMPGDK_VERSION}-amalgamation.zip
      ${PROJECT_BINARY_DIR}/src/sampgdk.c
      ${PROJECT_BINARY_DIR}/src/sampgdk.h
      ${PROJECT_SOURCE_DIR}/doc/how-to-use-amalgamation.txt
    COMMENT "Packaging amalgamation files")
  add_dependencies(sampgdk_amalgam_package sampgdk_amalgam)
endif()

add_dependencies(sampgdk sampgdk_init)

foreach(header IN LISTS SAMPGDK_HEADERS)
  get_filename_component(path ${header} PATH)
  get_filename_component(abs_path ${path} ABSOLUTE)

  if(header MATCHES ${PROJECT_BINARY_DIR})
    file(RELATIVE_PATH dir ${PROJECT_BINARY_DIR} ${abs_path})
  else()
    file(RELATIVE_PATH dir ${PROJECT_SOURCE_DIR} ${abs_path})
  endif()

  install(FILES ${header} DESTINATION ${dir} COMPONENT dev)
endforeach()

install(TARGETS sampgdk RUNTIME DESTINATION bin COMPONENT bin
                        ARCHIVE DESTINATION lib COMPONENT dev
                        LIBRARY DESTINATION lib COMPONENT bin)

if(WIN32)
  set(CPACK_PACKAGE_FILE_NAME sampgdk-${SAMPGDK_VERSION}-win32)
  set(CPACK_GENERATOR ZIP)
elseif(UNIX)
  set(CPACK_PACKAGE_FILE_NAME sampgdk-${SAMPGDK_VERSION}-linux)
  set(CPACK_GENERATOR TGZ)
endif()

if(SAMPGDK_STATIC)
  set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}-static)
endif()

include(CPack)
