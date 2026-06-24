const nodemailer = require('nodemailer');
const config_module = require("./config")

function SendMail(mailOptions_){
    let transport = nodemailer.createTransport({
        host: 'smtp.163.com',
        port: 465,
        secure: true,
        auth: {
            user: config_module.email_user,
            pass: config_module.email_pass
        }
    });

    return new Promise(function(resolve, reject){
        transport.sendMail(mailOptions_, function(error, info){
            transport.close();
            if (error) {
                console.log(error);
                reject(error);
            } else {
                console.log('邮件已成功发送：' + info.response);
                resolve(info.response);
            }
        });
    });
}

module.exports.SendMail = SendMail
