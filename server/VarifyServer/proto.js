

const path = require('path')           // Node.js 内置模块，处理文件路径
const grpc = require('@grpc/grpc-js')  // gRPC 库
const protoLoader = require('@grpc/proto-loader')  // 加载 .proto 文件的库

const PROTO_PATH = path.join(__dirname, 'message.proto')  // 拼出 proto 文件的完整路径
const packageDefinition = protoLoader.loadSync(PROTO_PATH, { keepCase: true, longs: String, enums: String, defaults: true, oneofs: true })  // 同步加载 proto 文件
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition)  // 解析成 JS 对象
const message_proto = protoDescriptor.message  // 拿到 message 包里的定义

module.exports = message_proto  // 导出，让其他文件可以用

