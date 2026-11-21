# ============================================================
#  server/proto —— 所有服务共用的协议定义（唯一真源）
#
#  真源：message.proto
#  生成物：generated/（message.pb.* / message.grpc.pb.*，入库，
#         这样 clone 下来不需要装 protoc 也能编译）
#
#  改完 message.proto 后重新生成：
#      cmake --build server\ChatServer\build --target regen_proto
#      （三个服务的 build 目录都注册了 regen_proto，跑哪个都行）
#
#  注意：protoc 必须用 grpc 自带的 3.13.0；
#       用 E:/zhongzhuan2/protoc/bin/protoc.exe（35.0）生成的 .pb.h
#       无法用 3.13 的头文件编译。
# ============================================================

set(PROTOC_EXE      "E:/zhongzhuan2/grpc/build_mingw/third_party/protobuf/protoc.exe")
set(GRPC_CPP_PLUGIN "E:/zhongzhuan2/grpc/build_mingw/grpc_cpp_plugin.exe")

# 注意：--cpp_out / --grpc_out 用相对路径，否则 Windows 盘符里的冒号
#       会被 protoc 当成生成器选项分隔符（报 Unknown generator option: "E"）。
add_custom_target(regen_proto
    COMMAND "${PROTOC_EXE}"
            -I .
            --cpp_out=generated
            --grpc_out=generated
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            message.proto
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerating server/proto/generated/* with protoc 3.13.0"
    VERBATIM
)
