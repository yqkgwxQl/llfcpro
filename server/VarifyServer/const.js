let code_prefix = "code_";  // Redis 中存储验证码时用的键前缀

const Errors = {
    Success : 0,
    RedisErr : 1,
    Exception : 2,
};


module.exports = {code_prefix,Errors}