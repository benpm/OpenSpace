##########################################################################################
#                                                                                        #
# OpenSpace                                                                              #
#                                                                                        #
# Copyright (c) 2014-2026                                                                #
#                                                                                        #
# Permission is hereby granted, free of charge, to any person obtaining a copy of this   #
# software and associated documentation files (the "Software"), to deal in the Software  #
# without restriction, including without limitation the rights to use, copy, modify,     #
# merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     #
# permit persons to whom the Software is furnished to do so, subject to the following    #
# conditions:                                                                            #
#                                                                                        #
# The above copyright notice and this permission notice shall be included in all copies  #
# or substantial portions of the Software.                                               #
#                                                                                        #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,    #
# INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A          #
# PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT     #
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF   #
# CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE   #
# OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                          #
##########################################################################################

cmake_minimum_required(VERSION 4.0)

project(OpenSpace LANGUAGES C CXX)

# CMake 4.0 will no longer allow minimum required version below 3.5 and some of our
# dependencies have not been updated to reflect this. To make things work, we blanked
# require to 3.10 here to remove those issues. Once CMake configures and generates
# successfully without this next line, it can be removed
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

set(OPENSPACE_RELEASE_BUILD OFF)
set(OPENSPACE_VERSION_MAJOR)
set(OPENSPACE_VERSION_MINOR)
set(OPENSPACE_VERSION_PATCH)

# Optionally disable vcpkg to prevent dependency conflicts
set(DISABLE_VCPKG OFF CACHE BOOL "Disable the use of vcpkg libs for Windows builds")
if (WIN32 AND DISABLE_VCPKG)
  # This works for all subprojects due to MSBuild's hierarchical search
  configure_file(
    "${CMAKE_SOURCE_DIR}/support/cmake/Directory.Build.Props.template"
    "${CMAKE_BINARY_DIR}/Directory.Build.props"
    COPYONLY
  )
  message(STATUS "Vcpkg disabled through CMake option DISABLE_VCPKG")
endif ()

include(${PROJECT_SOURCE_DIR}/support/cmake/module_common.cmake)
include(${PROJECT_SOURCE_DIR}/ext/ghoul/support/cmake/message_macros.cmake)

begin_header("Configuring OpenSpace project")

# Bail out if the user tries to generate a 32 bit project
if (NOT ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
  message(FATAL_ERROR "OpenSpace can only be generated for 64 bit architectures.")
endif ()

##########################################################################################
# Cleanup project                                                                        #
##########################################################################################
if (NOT EXISTS "${PROJECT_SOURCE_DIR}/ext/ghoul/CMakeLists.txt")
  message(FATAL_ERROR "Git submodules are missing. Please run "
    "git submodule update --init --recursive to download the missing dependencies."
  )
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER CMake)

mark_as_advanced(CMAKE_BACKWARDS_COMPATIBILITY CMAKE_BUILD_TYPE CMAKE_DEBUG_POSTFIX
  CMAKE_INSTALL_PREFIX CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
  CMAKE_OSX_SYSROOT CMAKE_RELEASE_POSTFIX
)

# Set build output directories
list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/support/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if (MSVC)
  # Force all builds to be multi-threaded and increase number of sections in obj files
  add_compile_options(/MP /bigobj)
endif ()

##########################################################################################
# Main                                                                                   #
##########################################################################################

# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE OPENSPACE_GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE OPENSPACE_GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)

option(OPENSPACE_HAVE_TESTS "Activate the OpenSpace unit tests" ON)

if (MSVC)
  option(OPENSPACE_BREAK_ON_FLOATING_POINT_EXCEPTION "Raise exceptions when encountering Inf's or Nan's in floating point numbers" OFF)
endif ()

if (UNIX AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  endif ()
  add_compile_definitions(D_GNU_SOURCE)
endif ()

add_subdirectory(ext)
add_subdirectory(src)

add_subdirectory(support/coding/codegen)
# It is important that the __codegen.h do not actually exist so
# that this target is never considered as finished
add_custom_target(
  run_codegen
  ALL DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/__codegen.h"
)
add_dependencies(run_codegen codegen-tool)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/__codegen.h"
  COMMAND codegen-tool ARGS "modules" "src"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  VERBATIM
)
set_target_properties(codegen-lib PROPERTIES FOLDER "support")
set_target_properties(codegen-tool PROPERTIES FOLDER "support")
set_target_properties(run_codegen PROPERTIES FOLDER "support")


begin_header("Configuring Modules")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/modules")
end_header("End: Configuring Modules")


begin_header("Configuring Applications")
add_subdirectory(apps)
end_header("End: Configuring Applications")


if (MSVC AND TARGET OpenSpace)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT OpenSpace)
endif ()


##########################################################################################
# clang/LLVM (clang-cl) build fixups for third-party submodule targets.                  #
# Kept here, in the main repo, so the submodule working trees stay pristine. Each is      #
# guarded on the Clang compiler id so the MSVC build is unaffected. See the global        #
# CMAKE_C_FLAGS / CMAKE_RC_FLAGS in CMakePresets.json for the companion C/RC fixes.        #
##########################################################################################
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # assimp's generated revision.h hardcodes VER_COPYRIGHT_STR "\xA9 2006-2023". llvm-rc
  # decodes the \xA9 escape to byte 169 and rejects it ("Non-ASCII 8-bit codepoint ...
  # can't occur in a non-Unicode string"); MSVC's rc.exe tolerates it. Rewrite the
  # generated header (a build-dir artifact, not the submodule) to ASCII "(C)". This runs
  # every configure, after assimp's configure_file has (re)generated the header.
  if (TARGET assimp)
    # configure_file writes revision.h from assimp's root binary dir (the target itself
    # is defined in the code/ subdir, so its BINARY_DIR is the wrong place to look).
    set(_assimp_rev "${CMAKE_BINARY_DIR}/ext/ghoul/ext/assimp/include/assimp/revision.h")
    if (EXISTS "${_assimp_rev}")
      file(READ "${_assimp_rev}" _rev_contents)
      # Bracket argument: no escape processing, so the literal \xA9 is matched as-is
      # (a quoted "\\xA9" would trip CMake's "invalid character escape '\x'").
      string(REPLACE [[\xA9 2006]] "(C) 2006" _rev_contents "${_rev_contents}")
      file(WRITE "${_assimp_rev}" "${_rev_contents}")
    endif ()
  endif ()

  # tiny-process-library/CMakeLists.txt passes "/D_CRT_SECURE_NO_WARNINGS" to
  # target_compile_definitions(), which auto-prepends -D and yields the invalid
  # "-D/D_CRT_SECURE_NO_WARNINGS" (clang: "macro name must be an identifier"). Overwrite
  # the property with the bare macro name.
  if (TARGET tiny-process-library)
    set_property(TARGET tiny-process-library
      PROPERTY COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS
    )
  endif ()

  # spice is legacy f2c-generated K&R C that trips clang diagnostics which default to
  # errors (clang 16+); spice's own /W0 doesn't downgrade those. Applied per-target
  # rather than via global CMAKE_C_FLAGS so we don't clobber CMake's clang-cl defaults
  # (/DWIN32 etc.) that other C deps such as cdf rely on.
  # It also forces SpiceZfc.h into a PCH whose prototypes conflict with the f2c sources'
  # own local prototypes under clang (an unsuppressible "conflicting types" error), so
  # disable the forced PCH; the affected sources include what they need themselves.
  if (TARGET spice)
    target_compile_options(spice PRIVATE
      -Wno-implicit-int
      -Wno-implicit-function-declaration
      -Wno-int-conversion
      -Wno-deprecated-non-prototype
      -Wno-error=incompatible-function-pointer-types
    )
    set_property(TARGET spice PROPERTY DISABLE_PRECOMPILE_HEADERS ON)
  endif ()

  # sgct/src/image.cpp includes stb inside an anonymous namespace. stb_image.h pulls
  # <stddef.h>, whose clang piece declares `namespace std { typedef ... nullptr_t; }`
  # *inside* that anonymous namespace, so every subsequent `std::` is ambiguous. Flags
  # can't fix a source-structure defect, so compile a patched copy (stb moved to global
  # scope with internal linkage via the _STATIC macros) generated into the build tree,
  # leaving the submodule source untouched.
  if (TARGET sgct)
    set(_sgct_img_orig "${CMAKE_SOURCE_DIR}/apps/OpenSpace/ext/sgct/src/image.cpp")
    set(_sgct_img_patched "${CMAKE_BINARY_DIR}/sgct_clang_fix/image.cpp")
    if (EXISTS "${_sgct_img_orig}")
      file(READ "${_sgct_img_orig}" _img)
      string(REPLACE "\r\n" "\n" _img "${_img}")
      set(_img_before "${_img}")
      string(REPLACE
[[namespace {
#define STBI_NO_SIMD
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
} // namespace]]
[[// [OpenSpace clang fix] stb hoisted out of the anonymous namespace (see the root
// CMakeLists.txt); _STATIC gives internal linkage, which the namespace used to provide.
#define STBI_NO_SIMD
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>]]
        _img "${_img}")
      if (_img STREQUAL _img_before)
        message(FATAL_ERROR
          "sgct image.cpp clang fix: expected stb anonymous-namespace block not found; "
          "the submodule source changed. Update the patch in the root CMakeLists.txt.")
      endif ()
      file(WRITE "${_sgct_img_patched}" "${_img}")

      # Swap the original image.cpp out of sgct's SOURCES for the patched copy.
      get_target_property(_sgct_srcs sgct SOURCES)
      set(_sgct_srcs_new "")
      foreach (_s IN LISTS _sgct_srcs)
        if (NOT _s MATCHES "(^|/)image\\.cpp$")
          list(APPEND _sgct_srcs_new "${_s}")
        endif ()
      endforeach ()
      list(APPEND _sgct_srcs_new "${_sgct_img_patched}")
      set_property(TARGET sgct PROPERTY SOURCES "${_sgct_srcs_new}")
    endif ()
  endif ()
endif ()


if (OPENSPACE_HAVE_TESTS)
  add_subdirectory(support/coding/codegen/tests)
  set_target_properties(run_test_codegen PROPERTIES FOLDER "Unit Tests/support")
  set_target_properties(codegentest PROPERTIES FOLDER "Unit Tests")

  begin_header("Generating OpenSpace unit test")
  add_subdirectory(tests)
  end_header()
endif ()


# Web Browser and Web gui
# Why not put these in the module's path? Because they do not have access to the
# target as of July 2017, which is needed.
if (OPENSPACE_MODULE_WEBBROWSER AND CEF_ROOT)
  if (WIN32)
    set(RESOURCE_FILE openspace.rc)
  endif ()

  # Add the CEF binary distribution's cmake/ directory to the module path and
  # find CEF to initialize it properly.
  list(APPEND CMAKE_MODULE_PATH "${WEBBROWSER_MODULE_PATH}/cmake")
  include(webbrowser_helpers)
else ()
  message(WARNING "Web configured to be included, but no CEF_ROOT was found, please try configuring CMake again")
endif ()

##########################################################################################
# Misc settings                                                                          #
##########################################################################################

# Manage the CPack packaging
include(${PROJECT_SOURCE_DIR}/support/cmake/packaging.cmake)

end_header("End: Configuring OpenSpace project")
