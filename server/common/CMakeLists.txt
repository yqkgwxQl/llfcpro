include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/LlfcDeps.cmake)

# ============================================================
#  server/common —— 三个后端服务共用的基础库
#
#  设计说明：
#    · 头文件保持「扁平」暴露（include/ 直接加到 include 路径），
#      所以各服务里 #include "Logger.h" / "const.h" 这种写法一行都不用改；
#    · 依赖（Boost / JsonCpp / hiredis / MySQL Connector）由本目标 PUBLIC 透出，
#      服务端只需要 target_link_libraries(... llfc_common)；
#    · 第三方路径变量由上层 CMakeLists 在 add_subdirectory 之前定义。
#
#  迁移状态（按 docs/改造记录.md BL-07 分批）：
#    第 1 批：Singleton / Logger / ConfigMgr / AsioIOServicePool        ✅
#    第 2 批：RedisConPool / RedisMgr                                   ✅
#    第 3 批：MysqlPool / MysqlDao / MysqlMgr / data.h / const.h        ✅
#  至此，原先在三个服务里各存一份的 12 个数据层文件，只剩这一份。
# ============================================================

# 第三方路径（与各服务 CMakeLists 里同一套；集中到一处便于日后统一，见 BL-09）
set(LLFC_HIREDIS_INCLUDEDIR "${LLFC_MINGW_ROOT}/x86_64-w64-mingw32/include/hiredis")
set(LLFC_MYSQLCPPCONN_INCLUDEDIR "${LLFC_MYSQLCPPCONN_ROOT}/include")
set(LLFC_MYSQLCPPCONN_LIB "${LLFC_MYSQLCPPCONN_ROOT}/lib64/debug/libmysqlcppconn.dll.a")

add_library(llfc_common STATIC
    src/Logger.cc
    src/ConfigMgr.cc
    src/AsioIOServicePool.cc
    src/RedisConPool.cc
    src/RedisMgr.cc
    src/MysqlPool.cc
    src/MysqlDao.cc
    src/MysqlMgr.cc
)

target_include_directories(llfc_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<$<BOOL:${Boost_FOUND}>:${Boost_INCLUDE_DIRS}>
    $<$<NOT:$<BOOL:${Boost_FOUND}>>:${BOOST_INCLUDEDIR}>
    ${JSONCPP_INCLUDEDIR}
    ${LLFC_HIREDIS_INCLUDEDIR}
    ${LLFC_MYSQLCPPCONN_INCLUDEDIR}
)

target_link_libraries(llfc_common PUBLIC
    $<$<BOOL:${Boost_FOUND}>:${Boost_LIBRARIES}>
    $<$<NOT:$<BOOL:${Boost_FOUND}>>:${BOOST_THREAD_LIB}>
    ${BOOST_FILESYSTEM_LIB}
    ${JSONCPP_LIBRARY}
    hiredis
    mysql
    ${LLFC_MYSQLCPPCONN_LIB}
)
