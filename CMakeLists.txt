cmake_minimum_required(VERSION 3.15)
project(KickCAT VERSION 0.0.0 LANGUAGES C CXX)

# AnChSa, 2026-10-30: native MSVC master-only build.
option(KICKCAT_MSVC_MINIMAL "Build native MSVC master core with externally supplied sockets" OFF)
option(ENABLE_WINDOWS_XDP "Enable the native MSVC Windows XDP socket backend" OFF)
if (ENABLE_WINDOWS_XDP)
  if (NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ENABLE_WINDOWS_XDP requires native MSVC x64 on Windows")
  endif()
  set(KICKCAT_MSVC_MINIMAL ON)
endif()
if(KICKCAT_MSVC_MINIMAL AND NOT (WIN32 AND MSVC))
  message(FATAL_ERROR "KICKCAT_MSVC_MINIMAL requires native Windows MSVC")
endif()


# Custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR})
include(Config)
include(debug)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(KICKCAT_INSTALL_DEFAULT ON)
else()
  set(KICKCAT_INSTALL_DEFAULT OFF)
endif()

# Host-only components (tests, simulation, GUI tools, ESI parser, master examples) need desktop
# deps and default ON for Linux/Windows only; embedded targets get the library + their slave example.
if ((UNIX OR WIN32) AND NOT KICKCAT_MSVC_MINIMAL)
  set(KICKCAT_HOST ON)
else()
  set(KICKCAT_HOST OFF)
endif()

option(BUILD_MASTER_EXAMPLES   "Build master examples" ${KICKCAT_HOST})
set(KICKCAT_SLAVE_EXAMPLES_DEFAULT ON)
if(KICKCAT_MSVC_MINIMAL)
  set(KICKCAT_SLAVE_EXAMPLES_DEFAULT OFF)
endif()
option(BUILD_SLAVE_EXAMPLES    "Build slave examples" ${KICKCAT_SLAVE_EXAMPLES_DEFAULT})
option(BUILD_UNIT_TESTS        "Build unit tests" OFF)
option(BUILD_SIMULATION        "Build simulation" ${KICKCAT_HOST})
option(BUILD_TOOLS             "Build tools" ${KICKCAT_HOST})
option(ENABLE_ESI_PARSER       "Enable ESI XML parser" ${KICKCAT_HOST})
option(ENABLE_AF_XDP           "Enable AF_XDP socket for improved performance (requires libxdp, libbpf, clang)" OFF)
option(ENABLE_CODE_COVERAGE    "Enable code coverage (requires gcovr)" OFF)
option(ENABLE_ASAN             "Build with AddressSanitizer" OFF)
option(ENABLE_UBSAN            "Build with UndefinedBehaviorSanitizer" OFF)
option(BUILD_EEPROM_EDITOR     "Build EEPROM editor GUI (requires imgui, glfw)" OFF)
option(BUILD_KICKUI            "Build KickUI, the KickCAT GUI (requires imgui, glfw)" OFF)
option(KICKCAT_INSTALL         "Install KickCAT targets, headers, CMake config and pkg-config files" ${KICKCAT_INSTALL_DEFAULT})

if(KICKCAT_MSVC_MINIMAL)
  foreach(feature BUILD_MASTER_EXAMPLES BUILD_SLAVE_EXAMPLES BUILD_UNIT_TESTS BUILD_SIMULATION
      BUILD_TOOLS ENABLE_ESI_PARSER ENABLE_AF_XDP BUILD_EEPROM_EDITOR BUILD_KICKUI
      ENABLE_CODE_COVERAGE ENABLE_ASAN ENABLE_UBSAN SKBUILD)
    if(${feature})
      message(FATAL_ERROR "${feature} is outside the native MSVC minimal build; use a separate build directory")
    endif()
  endforeach()
  option(BUILD_MSVC_CORE_CHECKS "Build hardware-free native master checks" ON)
endif()

include(Install)

if (ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

if (ENABLE_UBSAN)
  # 'alignment' is excluded on purpose: the EtherCAT frame layer reads/writes packed buffers at
  # unaligned offsets (supported on the x86/Cortex-M targets). The other UB checks stay on and abort.
  # float-cast-overflow is not in gcc's -fsanitize=undefined, and its no-recover needs naming too.
  add_compile_options(-fsanitize=undefined,float-cast-overflow -fno-sanitize=alignment
                      -fno-sanitize-recover=undefined,float-cast-overflow)
  add_link_options(-fsanitize=undefined,float-cast-overflow)
endif()


if (BUILD_UNIT_TESTS)
  find_package(GTest QUIET CONFIG)
  if (NOT GTest_FOUND)
    message(FATAL_ERROR "GTest not found: Unit tests will can NOT be built")
  else()
    if (ENABLE_CODE_COVERAGE)
      include(CodeCoverage)
      append_coverage_compiler_flags()

      # Derive versioned gcov (e.g. gcov-15) from the compiler so that gcovr
      # uses the same format as the .gcda/.gcno files produced by the build.
      get_filename_component(_cxx_name ${CMAKE_CXX_COMPILER} NAME)
      string(REGEX REPLACE "^g\\+\\+" "gcov" _gcov_name "${_cxx_name}")
      find_program(_gcov_exe NAMES ${_gcov_name} gcov)

      set(GCOVR_ADDITIONAL_ARGS
          --exclude-unreachable-branches
          --exclude-throw-branches
          --gcov-executable ${_gcov_exe})
    endif()
    enable_testing()
    add_subdirectory(unit)
  endif()
endif()

add_subdirectory(lib)

if(KICKCAT_MSVC_MINIMAL AND BUILD_MSVC_CORE_CHECKS)
  enable_testing()
  add_subdirectory(test/msvc)
endif()

if (ENABLE_WINDOWS_XDP AND BUILD_MSVC_CORE_CHECKS)
  add_subdirectory(test/windows_xdp)
endif()

if (BUILD_MASTER_EXAMPLES OR BUILD_SIMULATION OR BUILD_TOOLS)
  find_package(argparse CONFIG REQUIRED)
endif()

if (BUILD_MASTER_EXAMPLES)
  add_subdirectory(examples/master)
  add_subdirectory(test/integration)
endif()

if (BUILD_SLAVE_EXAMPLES)
  add_subdirectory(examples/slave)
endif()

if (BUILD_SIMULATION)
  find_package(nlohmann_json REQUIRED)
  add_subdirectory(simulation)
endif()

if (BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if (BUILD_EEPROM_EDITOR OR BUILD_KICKUI)
  add_subdirectory(tools/common)
endif()

if (BUILD_EEPROM_EDITOR)
  add_subdirectory(tools/eeprom_editor)
endif()

if (BUILD_KICKUI)
  add_subdirectory(tools/kickui)
endif()

add_subdirectory(py_bindings)

if (BUILD_EEPROM_EDITOR OR BUILD_KICKUI)
add_subdirectory(vendor)
endif()
