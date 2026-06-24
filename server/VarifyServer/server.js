
const grpc = require('@grpc/grpc-js')
const message_proto = require('./proto')
const const_module = require('./const')
const { v4: uuidv4 } = require('uuid');
const emailModule = require('./email');
const redis_module = require('./redis')

async function GetVarifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try{
        let query_res = await redis_module.GetRedis(const_module.code_prefix+call.request.email);
        console.log("query_res is ", query_res)
        // if(query_res == null){

        // }
        let uniqueId = query_res;
        if(query_res ==null){
            uniqueId = uuidv4();
            if (uniqueId.length > 4) {
                uniqueId = uniqueId.substring(0, 4);
            } 
            let bres = await redis_module.SetRedisExpire(const_module.code_prefix+call.request.email, uniqueId,600)
            if(!bres){
                callback(null, { email:  call.request.email,
                    error:const_module.Errors.RedisErr
                });
                return;
            }
        }

        console.log("uniqueId is ", uniqueId)
        let text_str =  '您的验证码为'+ uniqueId +'请三分钟内完成注册'
        //发送邮件
        let mailOptions = {
            from: 'yqkgwxql@163.com',
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };
    
        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Success
        }); 
        
 
    }catch(error){
        console.log("catch error is ", error)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }
     
}
// async function GetVarifyCode(call, callback) {
//     console.log("email is ", call.request.email)
//     try{
//         uniqueId = uuidv4();
//         console.log("uniqueId is ", uniqueId)
//         let text_str =  '您的验证码为'+ uniqueId +'请三分钟内完成注册'
//         //发送邮件
//         let mailOptions = {
//             from: 'yqkgwxql@163.com',
//             to: call.request.email,
//             subject: '验证码',
//             text: text_str,
//         };
    
//         let send_res = await emailModule.SendMail(mailOptions);
//         console.log("send res is ", send_res)

//         // ★ 将验证码存入 Redis，key=邮箱，value=验证码，3分钟过期
//         await redis_module.setEx(call.request.email, 180, uniqueId);
//         console.log("验证码已存入 Redis:", call.request.email, "->", uniqueId);

//         callback(null, { email:  call.request.email,
//             error:const_module.Errors.Success
//         }); 
        
 
//     }catch(error){
//         console.log("catch error is ", error)

//         callback(null, { email:  call.request.email,
//             error:const_module.Errors.Exception
//         }); 
//     }
     
// }

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {

        console.log('grpc server started')        
    })
}

main()

/*const grpc = require('@grpc/grpc-js')           // gRPC 库
const message_proto = require('./proto')         // 协议定义
const const_module = require('./const')          // 常量
const { v4: uuidv4 } = require('uuid');          // 只导入 uuid 库的 v4 函数
const emailModule = require('./email');          // 邮件模块
const redis_module = require('./redis')          // Redis 模块

// 核心函数：处理验证码请求
async function GetVarifyCode(call, callback) {
    console.log("email is ", call.request.email)  // 打印请求中的邮箱
    try{
        uniqueId = uuidv4();                      // 生成唯一验证码（UUID）
        console.log("uniqueId is ", uniqueId)
        let text_str = '您的验证码为'+ uniqueId +'请三分钟内完成注册'
        
        // 构造邮件参数
        let mailOptions = {
            from: 'yqkgwxql@163.com',             // 发件人
            to: call.request.email,               // 收件人（从请求中获取）
            subject: '验证码',                      // 邮件主题
            text: text_str,                        // 邮件正文
        };
    
        let send_res = await emailModule.SendMail(mailOptions);  // ★ 发送邮件
        console.log("send res is ", send_res)

        // 成功 → 回调返回 Success
        callback(null, { 
            email: call.request.email,
            error: const_module.Errors.Success
        }); 
        
    } catch(error) {
        console.log("catch error is ", error)
        // 失败 → 回调返回 Exception
        callback(null, { 
            email: call.request.email,
            error: const_module.Errors.Exception
        }); 
    }
}

// 主函数：启动 gRPC 服务
function main() {
    var server = new grpc.Server()
    // 注册服务：把 GetVarifyCode 函数绑定到 proto 定义的 VarifyService 上
    server.addService(message_proto.VarifyService.service, { 
        GetVarifyCode: GetVarifyCode 
    })
    // 监听 0.0.0.0:50051（所有网卡的 50051 端口）
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()  // 执行主函数*/