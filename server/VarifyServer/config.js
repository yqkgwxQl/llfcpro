
const fs = require('fs');  // Node.js 内置的文件系统模块

let config = JSON.parse(fs.readFileSync('config.json', 'utf8'));
// 1. fs.readFileSync('config.json', 'utf8') — 同步读取 config.json 文件内容（返回字符串）
// 2. JSON.parse(...) — 把 JSON 字符串解析成 JavaScript 对象

let email_user = config.email.user;   // 从对象中取出邮箱用户名
let email_pass = config.email.pass;   // 取出邮箱授权码
let mysql_host = config.mysql.host;
let mysql_port = config.mysql.port;
let redis_host = config.redis.host;
let redis_port = config.redis.port;
let redis_passwd = config.redis.passwd;
let code_prefix = "code_";

module.exports = {email_pass, email_user, mysql_host, mysql_port,redis_host, redis_port, redis_passwd, code_prefix};  // 导出这些变量

