#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"
#include <thread>

class AsioIOServicePool: public Singleton<AsioIOServicePool>{
    friend Singleton<AsioIOServicePool>;
    using IOService = boost::asio::io_context;
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;
public:
    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool& ) = delete;
    boost::asio::io_context& operator=(const AsioIOServicePool&) = delete;
    // 使用 round-robin 的方式返回一个 io_service
    IOService& GetIOService();
    void Stop();
private:
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
    std::vector<IOService> _ioServices;
    std::vector<Work> _work_guards;
    std::vector<std::thread> _threads;
    std::size_t _nextIOService;
};