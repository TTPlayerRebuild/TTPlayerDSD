cmake_minimum_required(VERSION 3.24)
project(TtpDsd LANGUAGES C CXX RC)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "ttp_dsd requires MSVC and -A Win32")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/legacy_windows.cmake)
include(cmake/size_release.cmake)
include(cmake/codec.cmake)
include(cmake/version.cmake)
add_library(ttp_dsd SHARED src/plugin.cpp src/reader.cpp src/dsd_metadata.cpp src/ttp_dsd.def
  "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc")
add_dependencies(ttp_dsd ttp_dsd_version)
target_include_directories(ttp_dsd PUBLIC include PRIVATE src)
target_compile_definitions(ttp_dsd PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0501 _CRT_SECURE_NO_WARNINGS)
target_compile_options(ttp_dsd PRIVATE /W4 /permissive- /EHsc /GR- /utf-8 /fp:precise /arch:SSE2)
target_link_libraries(ttp_dsd PRIVATE codec ole32 uuid shlwapi)
target_link_options(ttp_dsd PRIVATE /ENTRY:DllMainCRTStartupForYY_Thunks
  /alternatename:_YY_ThunksOriginalDllMainCRTStartup@12=__DllMainCRTStartup@12)
set_target_properties(ttp_dsd PROPERTIES PREFIX "" OUTPUT_NAME "ttp_dsd")
add_custom_command(TARGET ttp_dsd POST_BUILD
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_legacy_imports.py"
    "$<TARGET_FILE:ttp_dsd>"
    --exports "${ttplayer_yy_thunks_SOURCE_DIR}/Config/x86/5.1.2600.txt"
    --exports "${ttplayer_yy_thunks_SOURCE_DIR}/Config/x86/6.1.7600.txt"
    --report "$<TARGET_FILE_DIR:ttp_dsd>/legacy-imports.json" VERBATIM)
install(TARGETS ttp_dsd RUNTIME DESTINATION AddIn COMPONENT Runtime)

