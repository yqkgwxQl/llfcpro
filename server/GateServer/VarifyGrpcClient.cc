#include "VarifyGrpcClient.h"
#include "ConfigMgr.h"

// 注意== VarifyConPool 原先为RPConPool
VarifyConPool::VarifyConPool(size_t poolSize, std::string host, std::string port):
    b_stop_(false),poolSize_(poolSize),host_(host),port_(port){
        for(std::size_t i = 0; i < poolSize_; i++){
            std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,grpc::InsecureChannelCredentials());
            connections_.push(VarifyService::NewStub(channel)); //这里用了移动构造
        }
}

VarifyConPool::~VarifyConPool(){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    Close();
    while(!connections_.empty()){
        connections_.pop();
    }
}

void VarifyConPool::Close(){
    b_stop_ = true;
    cond_.notify_all();
}

std::unique_ptr<VarifyService::Stub> VarifyConPool::getConnection(){
    std::unique_lock<std::mutex> lock(mutex_);
    //这里的lamba表达式只有为true时，才不会cond_.wait
    cond_.wait(lock,[this](){
        if(b_stop_)
            return true;
        return !connections_.empty();
    });
    if(b_stop_)
        return nullptr;
    auto getStub = std::move(connections_.front());
    connections_.pop();
    return getStub;
}

void VarifyConPool::returnConnection(std::unique_ptr<VarifyService::Stub> context){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    if(b_stop_)
        return;     //这里context会自动析构的
    connections_.push(std::move(context));
    cond_.notify_one();
}

VarifyGrpcClient::VarifyGrpcClient(){
    //grpc::CreateChannel(...)  用于创建到服务端的通信通道。
    //InsecureChannelCredentials()：表示不加密的明文传输，数据在网络中以明文形式发送
    //50051：端口号，这是 gRPC 的默认端口
    ConfigMgr& CfgMgr = ConfigMgr::GetInstance();
    std::string host = CfgMgr["VarifyServer"]["Host"];
    std::string port = CfgMgr["VarifyServer"]["Port"];
    // pool_ = std::unique_ptr<VarifyConPool>(new VarifyConPool(5,host,port));
    // pool_.reset(new VarifyConPool(5, host, port));
    pool_ = std::make_unique<VarifyConPool>(5,host,port);
    /*pool_.reset(new VarifyConPool(5, host, port)); 做了两件事：
    1.释放旧对象（如果有的话）：如果 pool_ 之前已经管理着一个 VarifyConPool 对象，reset() 会先调用该对象的析构函数，释放内存。
    2.接管新对象：用 new VarifyConPool(5, host, port) 创建的新对象，交给 pool_ 来管理。*/
}

//这个函数是同步的，只是多线程可以分别使用各自的代理来获取验证码，因此调用完此函数后，一定发送了验证码
GetVarifyRsp VarifyGrpcClient::GetVarifyCode(std::string email){
    ClientContext context;
    GetVarifyReq request;
    GetVarifyRsp reply;
    request.set_email(email);

    auto stub = pool_->getConnection();
    Status status = stub->GetVarifyCode(&context,request,&reply);
    if(status.ok()){
        pool_->returnConnection(std::move(stub));
        return reply;
    }else{
        pool_->returnConnection(std::move(stub));
        reply.set_error(ErrorCodes::RPCVarifyFailed);
        return reply;
    }
}
