cmake_minimum_required(VERSION 3.20)
project(Manannan VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(yaml-cpp REQUIRED)

add_library(manannan_api INTERFACE)
target_include_directories(manannan_api INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(manannan_api INTERFACE yaml-cpp)

add_library(manannan_logging STATIC
  src/loggers/logger_scoped.cpp
  src/loggers/logger_stdio.cpp
  src/util.cpp)
target_link_libraries(manannan_logging PUBLIC manannan_api)

add_executable(manannan
  src/main.cpp
  src/config.cpp
  src/duration.cpp
  src/plugin_loader.cpp
  src/service.cpp)
target_link_libraries(manannan PRIVATE manannan_logging ${CMAKE_DL_LIBS})

add_subdirectory(plugins/amazon_check_ip)
add_subdirectory(plugins/aws_route53)

install(TARGETS manannan RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(DIRECTORY include/manannan DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES config/manannan.yaml DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/manannan)
install(FILES packaging/manannan.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)

if(BUILD_TESTING)
  add_executable(manannan_tests
    tests/test_main.cpp
    src/config.cpp
    src/duration.cpp)
  target_link_libraries(manannan_tests PRIVATE manannan_logging)
  add_test(NAME manannan_tests COMMAND manannan_tests)
endif()

