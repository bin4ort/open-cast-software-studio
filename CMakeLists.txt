cmake_minimum_required(VERSION 3.20)
project(ocs_studio
        VERSION 0.2.0
        DESCRIPTION "Open Cast Software Studio: voice transformation, gender conversion, virtual mic"
        HOMEPAGE_URL "https://github.com/bin4ort/open-cast-software-studio"
        LANGUAGES CXX)

option(OCS_GUI   "Build the Qt6 native GUI" ON)
option(OCS_TESTS "Build and register unit tests" ON)
option(OCS_NEURAL_VC "Experimental onnxruntime-based neural voice conversion" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies (system packages, no vendored/FetchContent code)
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)
pkg_check_modules(SNDFILE   REQUIRED IMPORTED_TARGET sndfile)
pkg_check_modules(SAMPLERATE REQUIRED IMPORTED_TARGET samplerate)

# ---------------------------------------------------------------------------
# Core library (CLI + GUI both link this)
# ---------------------------------------------------------------------------
add_library(ocs_core STATIC
  src/cli.cpp
  src/util/json.cpp
  src/audio/device.cpp
  src/audio/fileio.cpp
  src/audio/ringbuf.cpp
  src/audio/virtualmic.cpp
  src/dsp/fft.cpp
  src/dsp/stft.cpp
  src/dsp/phase_vocoder.cpp
  src/dsp/formant.cpp
  src/dsp/gender.cpp
  src/dsp/denoise.cpp
  src/dsp/vad.cpp
  src/dsp/pipeline.cpp
  src/transcribe/whisper.cpp
  src/anonymize/redactor.cpp
  src/ai/rewriter.cpp
  src/ai/llm.cpp
  src/ai/vc.cpp
  src/ai/regen.cpp
  src/tts.cpp
  src/processing/job.cpp
)

target_include_directories(ocs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ocs_core PUBLIC
  OCS_VERSION="${PROJECT_VERSION}"
  OCS_PROJECT_NAME="${PROJECT_NAME}"
)
target_compile_options(ocs_core PRIVATE -Wall -Wextra)

target_link_libraries(ocs_core PUBLIC
  PkgConfig::PORTAUDIO
  PkgConfig::SNDFILE
  PkgConfig::SAMPLERATE
)

if(NOT CMAKE_CROSSCOMPILING)
  find_program(FFMPEG_EXECUTABLE ffmpeg)
  if(FFMPEG_EXECUTABLE)
    target_compile_definitions(ocs_core PRIVATE OCS_HAVE_FFMPEG=1)
  endif()
endif()

if(OCS_NEURAL_VC)
  pkg_check_modules(ONNXRUNTIME QUIET IMPORTED_TARGET onnxruntime)
  if(ONNXRUNTIME_FOUND)
    target_link_libraries(ocs_core PUBLIC PkgConfig::ONNXRUNTIME)
    target_compile_definitions(ocs_core PRIVATE OCS_NEURAL_VC=1)
    message(STATUS "Neural VC: onnxruntime found (experimental)")
  else()
    message(WARNING "OCS_NEURAL_VC=ON but onnxruntime was not found - neural VC disabled")
  endif()
endif()

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(ocs_studio src/main.cpp)
target_link_libraries(ocs_studio PRIVATE ocs_core)
set_target_properties(ocs_studio PROPERTIES
  OUTPUT_NAME ocs_studio
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

# ---------------------------------------------------------------------------
# Qt6 GUI
# ---------------------------------------------------------------------------
if(OCS_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets)
  if(Qt6_FOUND)
    add_library(ocs_gui STATIC
      src/ui/mainwindow.cpp
      src/ui/waveform.cpp
      src/ui/livepanel.cpp
      src/ui/voicepanel.cpp
    )
    target_include_directories(ocs_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(ocs_gui PUBLIC ocs_core Qt6::Widgets)
    set_target_properties(ocs_gui PROPERTIES AUTOMOC ON)

    add_executable(ocs_studio_gui src/ui/gui_main.cpp)
    target_link_libraries(ocs_studio_gui PRIVATE ocs_gui)
    set_target_properties(ocs_studio_gui PROPERTIES OUTPUT_NAME ocs_studio_gui)
  else()
    message(WARNING "Qt6 Widgets not found - building CLI only. Install qt6-base-dev/qt6-base to enable the GUI.")
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(OCS_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    # test_main.cpp #includes the individual *_test.cpp files (single TU).
    add_executable(ocs_tests
      tests/test_main.cpp
    )
    target_include_directories(ocs_tests PRIVATE tests)
    target_link_libraries(ocs_tests PRIVATE ocs_core)
    target_compile_options(ocs_tests PRIVATE -Wall -Wextra)
    add_test(NAME ocs_tests COMMAND ocs_tests)
  endif()
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS ocs_studio
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET ocs_studio_gui)
  install(TARGETS ocs_studio_gui
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
