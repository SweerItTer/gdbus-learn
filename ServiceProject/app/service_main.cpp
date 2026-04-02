#include <TrainingService.hpp>

#include <exception>
#include <iostream>

// ---------------------------------------------------------------------------
// 服务端入口
// ---------------------------------------------------------------------------
int main() {
    try {
        training::service::TrainingService service;
        service.Run(); // 前台阻塞运行，Ctrl+C 退出
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "training_service error: " << ex.what() << std::endl;
        return 1;
    }
}
