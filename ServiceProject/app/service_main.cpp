#include <TrainingService.hpp>

#include <exception>
#include <iostream>

int main() {
    try {
        training::service::TrainingService service;
        service.Run(); // 堵塞运行
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "training_service error: " << ex.what() << std::endl;
        return 1;
    }
}
