# 1.基础环境与标准配置--------------------------------
cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)

project(STATUSSERVER LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制控制台子系统（否则 MinGW 会去找 WinMain）
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole")

# 2. 第三方库路径--------------------------------
set(Boost_ROOT "E:/YingYongRuanjian/mingw64")
set(BOOST_INCLUDEDIR "${Boost_ROOT}/x86_64-w64-mingw32/include")
set(BOOST_LIBRARYDIR "${Boost_ROOT}/x86_64-w64-mingw32/lib/lib")

set(JSONCPP_ROOT "E:/YingYongRuanjian/mingw64")
set(JSONCPP_INCLUDEDIR "${JSONCPP_ROOT}/x86_64-w64-mingw32/include")
set(JSONCPP_LIBRARYDIR "${JSONCPP_ROOT}/x86_64-w64-mingw32/lib")

# Protobuf（使用 gRPC 自带的，版本一致）
set(PROTOBUF_INCLUDEDIR "E:/zhongzhuan2/grpc/third_party/protobuf/src")
set(PROTOBUF_LIB "E:/zhongzhuan2/grpc/build_mingw/third_party/protobuf/libprotobuf.a")

# gRPC 构建目录
set(GRPC_BUILD "E:/zhongzhuan2/grpc/build_mingw")
set(GRPC_INCLUDEDIR "E:/zhongzhuan2/grpc/include")

# Abseil 头文件路径（protobuf 引用了 absl 头文件）
set(ABSL_INCLUDEDIR "E:/zhongzhuan2/grpc/third_party/abseil-cpp")

# gRPC 核心库（在 build_mingw 根目录）
set(GRPC_LIBS
    "${GRPC_BUILD}/libgrpc++.a"
    "${GRPC_BUILD}/libgrpc.a"
    "${GRPC_BUILD}/libgpr.a"
    "${GRPC_BUILD}/libupb.a"
    "${GRPC_BUILD}/libaddress_sorting.a"
)

# 第三方库（在 third_party 子目录下）
set(GRPC_DEPS_LIBS
    "${GRPC_BUILD}/third_party/re2/libre2.a"
    "${GRPC_BUILD}/third_party/cares/cares/lib/libcares.a"
    "${GRPC_BUILD}/third_party/boringssl-with-bazel/libssl.a"
    "${GRPC_BUILD}/third_party/boringssl-with-bazel/libcrypto.a"
    "${GRPC_BUILD}/third_party/zlib/libzlibstatic.a"
)

# Abseil 库（在 third_party/abseil-cpp/ 子目录下）
set(ABSL_BASE    "${GRPC_BUILD}/third_party/abseil-cpp/absl/base")
set(ABSL_CONTAINER "${GRPC_BUILD}/third_party/abseil-cpp/absl/container")
set(ABSL_DEBUG   "${GRPC_BUILD}/third_party/abseil-cpp/absl/debugging")
set(ABSL_FLAGS   "${GRPC_BUILD}/third_party/abseil-cpp/absl/flags")
set(ABSL_HASH    "${GRPC_BUILD}/third_party/abseil-cpp/absl/hash")
set(ABSL_NUMERIC "${GRPC_BUILD}/third_party/abseil-cpp/absl/numeric")
set(ABSL_RANDOM  "${GRPC_BUILD}/third_party/abseil-cpp/absl/random")
set(ABSL_STATUS  "${GRPC_BUILD}/third_party/abseil-cpp/absl/status")
set(ABSL_STRINGS "${GRPC_BUILD}/third_party/abseil-cpp/absl/strings")
set(ABSL_SYNC    "${GRPC_BUILD}/third_party/abseil-cpp/absl/synchronization")
set(ABSL_TIME    "${GRPC_BUILD}/third_party/abseil-cpp/absl/time")
set(ABSL_TYPES   "${GRPC_BUILD}/third_party/abseil-cpp/absl/types")

set(ABSL_LIBS
    "${ABSL_BASE}/libabsl_base.a"
    "${ABSL_BASE}/libabsl_raw_logging_internal.a"
    "${ABSL_BASE}/libabsl_throw_delegate.a"
    "${ABSL_BASE}/libabsl_spinlock_wait.a"
    "${ABSL_BASE}/libabsl_malloc_internal.a"
    "${ABSL_BASE}/libabsl_strerror.a"
    "${ABSL_BASE}/libabsl_log_severity.a"
    "${ABSL_BASE}/libabsl_exponential_biased.a"
    "${ABSL_BASE}/libabsl_periodic_sampler.a"
    "${ABSL_BASE}/libabsl_scoped_set_env.a"
    "${ABSL_STRINGS}/libabsl_strings.a"
    "${ABSL_STRINGS}/libabsl_strings_internal.a"
    "${ABSL_STRINGS}/libabsl_cord.a"
    "${ABSL_STRINGS}/libabsl_str_format_internal.a"
    "${ABSL_SYNC}/libabsl_synchronization.a"
    "${ABSL_SYNC}/libabsl_graphcycles_internal.a"
    "${ABSL_TIME}/libabsl_time.a"
    "${ABSL_TIME}/libabsl_civil_time.a"
    "${ABSL_TIME}/libabsl_time_zone.a"
    "${ABSL_NUMERIC}/libabsl_int128.a"
    "${ABSL_HASH}/libabsl_hash.a"
    "${ABSL_HASH}/libabsl_city.a"
    "${ABSL_CONTAINER}/libabsl_raw_hash_set.a"
    "${ABSL_CONTAINER}/libabsl_hashtablez_sampler.a"
    "${ABSL_DEBUG}/libabsl_stacktrace.a"
    "${ABSL_DEBUG}/libabsl_symbolize.a"
    "${ABSL_DEBUG}/libabsl_debugging_internal.a"
    "${ABSL_DEBUG}/libabsl_demangle_internal.a"
    "${ABSL_DEBUG}/libabsl_examine_stack.a"
    "${ABSL_DEBUG}/libabsl_leak_check.a"
    "${ABSL_DEBUG}/libabsl_leak_check_disable.a"
    "${ABSL_DEBUG}/libabsl_failure_signal_handler.a"
    "${ABSL_STATUS}/libabsl_status.a"
    "${ABSL_STATUS}/libabsl_statusor.a"
    "${ABSL_TYPES}/libabsl_bad_any_cast_impl.a"
    "${ABSL_TYPES}/libabsl_bad_optional_access.a"
    "${ABSL_TYPES}/libabsl_bad_variant_access.a"
    "${ABSL_RANDOM}/libabsl_random_distributions.a"
    "${ABSL_RANDOM}/libabsl_random_seed_gen_exception.a"
    "${ABSL_RANDOM}/libabsl_random_seed_sequences.a"
    "${ABSL_RANDOM}/libabsl_random_internal_platform.a"
    "${ABSL_RANDOM}/libabsl_random_internal_randen.a"
    "${ABSL_RANDOM}/libabsl_random_internal_randen_slow.a"
    "${ABSL_RANDOM}/libabsl_random_internal_randen_hwaes.a"
    "${ABSL_RANDOM}/libabsl_random_internal_randen_hwaes_impl.a"
    "${ABSL_RANDOM}/libabsl_random_internal_seed_material.a"
    "${ABSL_RANDOM}/libabsl_random_internal_pool_urbg.a"
    "${ABSL_RANDOM}/libabsl_random_internal_distribution_test_util.a"
    "${ABSL_FLAGS}/libabsl_flags.a"
    "${ABSL_FLAGS}/libabsl_flags_internal.a"
    "${ABSL_FLAGS}/libabsl_flags_config.a"
    "${ABSL_FLAGS}/libabsl_flags_marshalling.a"
    "${ABSL_FLAGS}/libabsl_flags_commandlineflag.a"
    "${ABSL_FLAGS}/libabsl_flags_commandlineflag_internal.a"
    "${ABSL_FLAGS}/libabsl_flags_program_name.a"
    "${ABSL_FLAGS}/libabsl_flags_reflection.a"
    "${ABSL_FLAGS}/libabsl_flags_usage.a"
    "${ABSL_FLAGS}/libabsl_flags_usage_internal.a"
    "${ABSL_FLAGS}/libabsl_flags_parse.a"
    "${ABSL_FLAGS}/libabsl_flags_private_handle_accessor.a"
)

# 3. 核心依赖查找--------------------------------
find_package(Boost QUIET COMPONENTS thread)
if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_VERSION}")
else()
    message(STATUS "Boost NOT found via find_package, using manual paths")
    set(BOOST_INCLUDE_PATH "${BOOST_INCLUDEDIR}")
    set(BOOST_THREAD_LIB "${BOOST_LIBRARYDIR}/libboost_thread-mgw15-mt-x64-1_91.a")
    set(BOOST_FILESYSTEM_LIB "${BOOST_LIBRARYDIR}/libboost_filesystem-mgw15-mt-x64-1_91.a")
endif()

find_library(JSONCPP_LIBRARY NAMES jsoncpp libjsoncpp PATHS "${JSONCPP_LIBRARYDIR}" NO_DEFAULT_PATH)
if(NOT JSONCPP_LIBRARY)
    set(JSONCPP_LIBRARY "${JSONCPP_LIBRARYDIR}/libjsoncpp.a")
endif()

# 自动将 config.ini 复制到构建目录
configure_file(${CMAKE_SOURCE_DIR}/config.ini ${CMAKE_BINARY_DIR}/config.ini COPYONLY)

# 4. 构建目标-----------------------------------
add_executable(${PROJECT_NAME}
    StatusServer.cc
    StatusServiceImpl.cc
    ConfigMgr.cc
    AsioIOServicePool.cc
    RedisMgr.cc
    RedisConPool.cc
    MysqlPool.cc
    MysqlDao.cc
    MysqlMgr.cc
    message.pb.cc
    message.grpc.pb.cc
)

# 5. 头文件包含路径-------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    $<$<BOOL:${Boost_FOUND}>:${Boost_INCLUDE_DIRS}>
    $<$<NOT:$<BOOL:${Boost_FOUND}>>:${BOOST_INCLUDEDIR}>
    ${JSONCPP_INCLUDEDIR}
    ${PROTOBUF_INCLUDEDIR}
    ${GRPC_INCLUDEDIR}
    ${ABSL_INCLUDEDIR}
    E:/zhongzhuan2/mysql_connector/install_mingw/include
    E:/YingYongRuanjian/mingw64/x86_64-w64-mingw32/include/hiredis
)

# 6. 链接库-------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    $<$<BOOL:${Boost_FOUND}>:${Boost_LIBRARIES}>
    $<$<NOT:$<BOOL:${Boost_FOUND}>>:${BOOST_THREAD_LIB}>
    ${BOOST_FILESYSTEM_LIB}
    ${JSONCPP_LIBRARY}
    ${PROTOBUF_LIB}
    ${GRPC_LIBS}
    ${GRPC_DEPS_LIBS}
    -Wl,--start-group
    ${ABSL_LIBS}
    -Wl,--end-group
    hiredis
    mysql
    E:/zhongzhuan2/mysql_connector/install_mingw/lib64/debug/libmysqlcppconn.dll.a
    ws2_32
    wsock32
    mswsock
    dbghelp
)

# 7. 复制运行时 DLL 到构建目录--------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "E:/YingYongRuanjian/mingw64/x86_64-w64-mingw32/bin/libhiredis.dll"
        "${CMAKE_BINARY_DIR}/libhiredis.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "E:/zhongzhuan2/mysql_connector/install_mingw/lib64/debug/libmysqlcppconn-10.dll"
        "${CMAKE_BINARY_DIR}/libmysqlcppconn-10.dll"
)

# 编译器选项
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -pthread
)
