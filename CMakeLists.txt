cmake_minimum_required(VERSION 3.20)
project(voice_anonymizer
        VERSION 0.1.0
        DESCRIPTION "Voice anonymization toolkit: pitch/formant shifting, transcript redaction"
        HOMEPAGE_URL "https://example.invalid/voice_anonymizer"
        LANGUAGES CXX)

option(VA_GUI   "Build the Qt6 native GUI" ON)
option(VA_TESTS "Build and register unit tests" ON)

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
add_library(va_core STATIC
  src/cli.cpp
  src/util/json.cpp
  src/audio/device.cpp
  src/audio/fileio.cpp
  src/audio/ringbuf.cpp
  src/dsp/fft.cpp
  src/dsp/stft.cpp
  src/dsp/phase_vocoder.cpp
  src/dsp/formant.cpp
  src/dsp/denoise.cpp
  src/dsp/vad.cpp
  src/dsp/pipeline.cpp
  src/transcribe/whisper.cpp
  src/anonymize/redactor.cpp
  src/processing/job.cpp
)

target_include_directories(va_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(va_core PUBLIC
  VA_VERSION="${PROJECT_VERSION}"
  VA_PROJECT_NAME="${PROJECT_NAME}"
)
target_compile_options(va_core PRIVATE -Wall -Wextra)

target_link_libraries(va_core PUBLIC
  PkgConfig::PORTAUDIO
  PkgConfig::SNDFILE
  PkgConfig::SAMPLERATE
)

if(NOT CMAKE_CROSSCOMPILING)
  find_program(FFMPEG_EXECUTABLE ffmpeg)
  if(FFMPEG_EXECUTABLE)
    target_compile_definitions(va_core PRIVATE VA_HAVE_FFMPEG=1)
  endif()
endif()

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(voice_anon src/main.cpp)
target_link_libraries(voice_anon PRIVATE va_core)
set_target_properties(voice_anon PROPERTIES
  OUTPUT_NAME voice_anon
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

# ---------------------------------------------------------------------------
# Qt6 GUI
# ---------------------------------------------------------------------------
if(VA_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets)
  if(Qt6_FOUND)
    add_library(va_gui STATIC
      src/ui/mainwindow.cpp
      src/ui/waveform.cpp
      src/ui/livepanel.cpp
    )
    target_include_directories(va_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(va_gui PUBLIC va_core Qt6::Widgets)
    set_target_properties(va_gui PROPERTIES AUTOMOC ON)

    add_executable(voice_anon_gui src/ui/gui_main.cpp)
    target_link_libraries(voice_anon_gui PRIVATE va_gui)
    set_target_properties(voice_anon_gui PROPERTIES OUTPUT_NAME voice_anon_gui)
  else()
    message(WARNING "Qt6 Widgets not found - building CLI only. Install qt6-base-dev/qt6-base to enable the GUI.")
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(VA_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    # test_main.cpp #includes the individual *_test.cpp files (single TU).
    add_executable(va_tests
      tests/test_main.cpp
    )
    target_include_directories(va_tests PRIVATE tests)
    target_link_libraries(va_tests PRIVATE va_core)
    target_compile_options(va_tests PRIVATE -Wall -Wextra)
    add_test(NAME va_tests COMMAND va_tests)
  endif()
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS voice_anon
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET voice_anon_gui)
  install(TARGETS voice_anon_gui
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
