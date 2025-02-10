# CMake generated Testfile for 
# Source directory: C:/dev/libzippp-win32-ready_to_compile
# Build directory: C:/dev/libzippp-win32-ready_to_compile/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(libzippp_tests "C:/dev/libzippp-win32-ready_to_compile/build/Debug/libzippp_static_test.exe")
  set_tests_properties(libzippp_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;63;add_test;C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(libzippp_tests "C:/dev/libzippp-win32-ready_to_compile/build/Release/libzippp_static_test.exe")
  set_tests_properties(libzippp_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;63;add_test;C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(libzippp_tests "C:/dev/libzippp-win32-ready_to_compile/build/MinSizeRel/libzippp_static_test.exe")
  set_tests_properties(libzippp_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;63;add_test;C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(libzippp_tests "C:/dev/libzippp-win32-ready_to_compile/build/RelWithDebInfo/libzippp_static_test.exe")
  set_tests_properties(libzippp_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;63;add_test;C:/dev/libzippp-win32-ready_to_compile/CMakeLists.txt;0;")
else()
  add_test(libzippp_tests NOT_AVAILABLE)
endif()
