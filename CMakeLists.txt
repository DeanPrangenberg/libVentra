cmake_minimum_required(VERSION 4.1)
project(libVentra LANGUAGES CXX)

add_library(libVentra INTERFACE)

target_sources(libVentra INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
    include/ventra/unordered_map/unordered_map.hpp
    include/ventra/unordered_map/unordered_map.tpp
    include/ventra/vector/concurrent_atomic_vector.hpp
    include/ventra/vector/concurrent_atomic_vector.tpp
    include/ventra/vector/concurrent_smart_vector.hpp
    include/ventra/vector/concurrent_smart_vector.tpp
    include/ventra/vector/vector.hpp
    include/ventra/vector/vector.tpp
)

target_include_directories(libVentra INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(libVentra INTERFACE cxx_std_23)

include(GNUInstallDirs)

install(TARGETS libVentra
    EXPORT libVentraTargets
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT libVentraTargets
    FILE libVentraTargets.cmake
    NAMESPACE Ventra::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libVentra
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/libVentraConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libVentraConfig.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/libVentraTargets.cmake\")\n"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/libVentraConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/libVentraConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libVentra
)

if(PROJECT_IS_TOP_LEVEL)
  # ==========================================
  # BUILD OPTIONS
  # ==========================================
  option(VENTRA_BUILD_TESTS "Build unit tests with GTest" ON)
  option(VENTRA_BUILD_BENCHMARKS "Build benchmarks with Google Benchmark" OFF)
  option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

  if(ENABLE_TSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g -O1")
  endif()

  include(FetchContent)

  # ==========================================
  # UNIT TESTS (GoogleTest)
  # ==========================================
  if(VENTRA_BUILD_TESTS)
    include(CTest)
    enable_testing()

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(libVentra_unordered_map_unordered_map_test tests/unordered_map/unordered_map_test.cpp)
    target_link_libraries(libVentra_unordered_map_unordered_map_test PRIVATE libVentra GTest::gtest_main)

    add_executable(libVentra_vector_concurrent_atomic_vector_test tests/vector/concurrent_atomic_vector_test.cpp)
    target_link_libraries(libVentra_vector_concurrent_atomic_vector_test PRIVATE libVentra GTest::gtest_main)

    add_executable(libVentra_vector_concurrent_smart_vector_test tests/vector/concurrent_smart_vector_test.cpp)
    target_link_libraries(libVentra_vector_concurrent_smart_vector_test PRIVATE libVentra GTest::gtest_main)

    add_executable(libVentra_vector_vector_test tests/vector/vector_test.cpp)
    target_link_libraries(libVentra_vector_vector_test PRIVATE libVentra GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(libVentra_unordered_map_unordered_map_test)
    gtest_discover_tests(libVentra_vector_concurrent_atomic_vector_test)
    gtest_discover_tests(libVentra_vector_concurrent_smart_vector_test)
    gtest_discover_tests(libVentra_vector_vector_test)
  endif()

  # ==========================================
  # BENCHMARKS (Google Benchmark)
  # ==========================================
  if(VENTRA_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    FetchContent_MakeAvailable(google_benchmark)

    find_package(Threads REQUIRED)

    add_executable(libVentra_vector_ventra_concurrent_test benchmarks/vector/ventra_concurrent_test.cpp)
    target_link_libraries(libVentra_vector_ventra_concurrent_test PRIVATE
        libVentra
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
    )

    add_executable(libVentra_vector_ventra_vector_std_vector benchmarks/vector/ventra_vector_std_vector.cpp)
    target_link_libraries(libVentra_vector_ventra_vector_std_vector PRIVATE
        libVentra
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
    )

    enable_testing()

            add_test(
                NAME libVentra_vector_ventra_concurrent_test
                COMMAND libVentra_vector_ventra_concurrent_test
                --benchmark_out=$<SHELL_PATH:${CMAKE_CURRENT_BINARY_DIR}/libVentra_vector_ventra_concurrent_test.json>
                --benchmark_out_format=json
                COMMAND_EXPAND_LISTS
            )
        

            add_test(
                NAME libVentra_vector_ventra_vector_std_vector
                COMMAND libVentra_vector_ventra_vector_std_vector
                --benchmark_out=$<SHELL_PATH:${CMAKE_CURRENT_BINARY_DIR}/libVentra_vector_ventra_vector_std_vector.json>
                --benchmark_out_format=json
                COMMAND_EXPAND_LISTS
            )
        

  endif()
endif()
