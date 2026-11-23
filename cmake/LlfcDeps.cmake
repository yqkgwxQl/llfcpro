# ============================================================
#  cmake/LlfcDeps.cmake —— 全仓库唯一的“第三方依赖路径调度点”
#
#  服务端（ChatServer / GateServer / StatusServer）与客户端（llfcchat）
#  都从这里拿路径；本文件只【定义变量】，不调用 find_package，
#  谁用谁去找（服务端不找 Qt，客户端不找 gRPC）。
#
#  换机器时只需要设环境变量（未设置则回落本机默认值）：
#
#    LLFC_MINGW_ROOT         MinGW + Boost + JsonCpp + hiredis 根目录
#                            默认 E:/YingYongRuanjian/mingw64
#    LLFC_GRPC_ROOT          gRPC 源码根目录（含 include/、third_party/）
#                            默认 E:/zhongzhuan2/grpc
#    LLFC_GRPC_BUILD         gRPC 构建目录
#                            默认 ${LLFC_GRPC_ROOT}/build_mingw
#    LLFC_MYSQLCPPCONN_ROOT  MySQL Connector/C++ 安装根目录
#                            默认 E:/zhongzhuan2/mysql_connector/install_mingw
#    LLFC_QT_ROOT            Qt 安装前缀（客户端用）
#                            默认 F:/YingYongRuanJian3/Qt/qita/6.11.0/mingw_64
#
#  用法（在 project() 之后）：
#      include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/LlfcDeps.cmake)
#
#  没有环境变量、且默认路径也不存在时，变量为空；由使用方自行报错。
# ============================================================

function(_llfc_env_or VAR_NAME ENV_NAME DEFAULT_VALUE)
    if(DEFINED ENV{${ENV_NAME}} AND NOT "$ENV{${ENV_NAME}}" STREQUAL "")
        set(${VAR_NAME} "$ENV{${ENV_NAME}}" PARENT_SCOPE)
        set(${VAR_NAME}_SRC "环境变量 ${ENV_NAME}" PARENT_SCOPE)
    elseif(NOT "${DEFAULT_VALUE}" STREQUAL "")
        set(${VAR_NAME} "${DEFAULT_VALUE}" PARENT_SCOPE)
        set(${VAR_NAME}_SRC "默认" PARENT_SCOPE)
    else()
        set(${VAR_NAME} "" PARENT_SCOPE)
        set(${VAR_NAME}_SRC "未设置" PARENT_SCOPE)
    endif()
endfunction()

_llfc_env_or(LLFC_MINGW_ROOT        LLFC_MINGW_ROOT        "E:/YingYongRuanjian/mingw64")
_llfc_env_or(LLFC_GRPC_ROOT         LLFC_GRPC_ROOT         "E:/zhongzhuan2/grpc")
_llfc_env_or(LLFC_GRPC_BUILD        LLFC_GRPC_BUILD        "${LLFC_GRPC_ROOT}/build_mingw")
_llfc_env_or(LLFC_MYSQLCPPCONN_ROOT LLFC_MYSQLCPPCONN_ROOT "E:/zhongzhuan2/mysql_connector/install_mingw")

# Qt 只有客户端需要：默认值必须“确实存在”才采用，否则留空
_llfc_env_or(LLFC_QT_ROOT LLFC_QT_ROOT "")
if(NOT LLFC_QT_ROOT)
    set(_llfc_qt_default "F:/YingYongRuanJian3/Qt/qita/6.11.0/mingw_64")
    if(EXISTS "${_llfc_qt_default}")
        set(LLFC_QT_ROOT "${_llfc_qt_default}")
        set(LLFC_QT_ROOT_SRC "默认")
    endif()
endif()

# 统一成 /：Windows 反斜杠在字符串/生成器表达式里容易出问题。
# 注意：这里是文件作用域（include 与调用方共享作用域），不能写 PARENT_SCOPE，
#       否则在顶层 CMakeLists 里会触发 "no parent scope" 的 dev warning。
foreach(_llfc_var LLFC_MINGW_ROOT LLFC_GRPC_ROOT LLFC_GRPC_BUILD LLFC_MYSQLCPPCONN_ROOT LLFC_QT_ROOT)
    if(${_llfc_var})
        string(REPLACE "\\" "/" _llfc_val "${${_llfc_var}}")
        set(${_llfc_var} "${_llfc_val}")
    endif()
endforeach()

# 启动时打印一行清单：一眼看出“这台机器用的是默认值还是环境变量”
foreach(_llfc_var LLFC_MINGW_ROOT LLFC_GRPC_ROOT LLFC_GRPC_BUILD LLFC_MYSQLCPPCONN_ROOT LLFC_QT_ROOT)
    if(${_llfc_var})
        message(STATUS "[llfc] ${_llfc_var} = ${${_llfc_var}}    (${${_llfc_var}_SRC})")
    else()
        message(STATUS "[llfc] ${_llfc_var} = <未设置>")
    endif()
endforeach()
