#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>
#include <limits>
#include <string>

// ---------------------------------------------------------------------------
// 菜单辅助
// ---------------------------------------------------------------------------
namespace {

void PrintMenu() {
    // 菜单既保留原有 Set/Get 调试能力，也暴露上传和下载，方便联调时直接做手工冒烟。
    std::cout << "\n=== Training Client Menu ===\n"
              << "1. SetTestBool\n"
              << "2. SetTestInt\n"
              << "3. SetTestDouble\n"
              << "4. SetTestString\n"
              << "5. SetTestInfo\n"
              << "6. GetTestBool\n"
              << "7. GetTestInt\n"
              << "8. GetTestDouble\n"
              << "9. GetTestString\n"
              << "10. GetTestInfo\n"
              << "11. SendFilePath\n"
              << "12. DownloadFile\n"
              << "0. Exit\n"
              << "Select: ";
}

void ClearInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool ReadBool(const char* prompt) {
    int value = 0;
    std::cout << prompt << " (0/1): ";
    std::cin >> value;
    return value != 0; // 非零均为true
}

std::string ReadLine(const char* prompt) {
    std::string value;
    std::cout << prompt;
    std::getline(std::cin >> std::ws, value);
    return value;
}

void PrintInfo(const training::public_api::TestInfo& info) {
    std::cout << "{ bool=" << std::boolalpha << info.bool_param // 将布尔数据按照字符串输出“true false”
              << ", int=" << info.int_param
              << ", double=" << info.double_param
              << ", string=\"" << info.string_param << "\" }"
              << std::noboolalpha << std::endl; // 关闭输出模式
}

} // namespace

// ---------------------------------------------------------------------------
// 客户端入口
// ---------------------------------------------------------------------------
int main() {
    try {
        training::client::TrainingClient client;
        std::cout << "training_client started. Waiting for menu commands." << std::endl;

        while (true) {
            PrintMenu();

            // 先读取菜单项，再分发到具体接口。
            int choice = -1;
            if (!(std::cin >> choice)) {
                ClearInput();
                std::cout << "Invalid input." << std::endl;
                continue;
            }

            switch (choice) {
            case 0:
                std::cout << "Exiting training_client." << std::endl;
                return 0;
            case 1: {
                // 手工验证 bool 写入。
                const bool value = ReadBool("bool value");
                std::cout << "SetTestBool -> " << std::boolalpha << client.SetTestBool(value) << std::noboolalpha
                          << std::endl;
                break;
            }
            case 2: {
                // 手工验证 int 写入。
                int value = 0;
                std::cout << "int value: ";
                std::cin >> value;
                std::cout << "SetTestInt -> " << std::boolalpha << client.SetTestInt(value) << std::noboolalpha
                          << std::endl;
                break;
            }
            case 3: {
                // 手工验证 double 写入。
                double value = 0.0;
                std::cout << "double value: ";
                std::cin >> value;
                std::cout << "SetTestDouble -> " << std::boolalpha << client.SetTestDouble(value) << std::noboolalpha
                          << std::endl;
                break;
            }
            case 4: {
                // 手工验证 string 写入。
                const auto value = ReadLine("string value: ");
                std::cout << "SetTestString -> " << std::boolalpha << client.SetTestString(value) << std::noboolalpha
                          << std::endl;
                break;
            }
            case 5: {
                // 组合结构体一次性写入。
                training::public_api::TestInfo info{};
                info.bool_param = ReadBool("bool value");
                std::cout << "int value: ";
                std::cin >> info.int_param;
                std::cout << "double value: ";
                std::cin >> info.double_param;
                info.string_param = ReadLine("string value: ");
                std::cout << "SetTestInfo -> " << std::boolalpha << client.SetTestInfo(info) << std::noboolalpha
                          << std::endl;
                break;
            }
            case 6:
                std::cout << "GetTestBool -> " << std::boolalpha << client.GetTestBool() << std::noboolalpha
                          << std::endl;
                break;
            case 7:
                // 读取最新 int。
                std::cout << "GetTestInt -> " << client.GetTestInt() << std::endl;
                break;
            case 8:
                // 读取最新 double。
                std::cout << "GetTestDouble -> " << client.GetTestDouble() << std::endl;
                break;
            case 9:
                // 读取最新 string。
                std::cout << "GetTestString -> " << client.GeTestString() << std::endl;
                break;
            case 10:
                // 读取完整结构体。
                std::cout << "GetTestInfo -> ";
                PrintInfo(client.GetTestInfo());
                break;
            case 11: {
                // 这里允许 remote 为空，便于快速验证“默认上传到服务端 file/ 根目录”的行为。
                const auto value = ReadLine("local file path: ");
                const auto remote = ReadLine("remote relative path (empty uses basename): ");
                std::cout << "SendFilePath -> " << std::boolalpha << client.SendFileByPath(value, remote) << std::noboolalpha
                          << std::endl;
                break;
            }
            case 12: {
                // 下载接口强制输入本地路径，避免示例程序悄悄在未知位置创建文件。
                const auto remote = ReadLine("remote relative path: ");
                const auto local = ReadLine("local target path: ");
                std::cout << "DownloadFile -> " << std::boolalpha << client.DownloadFile(remote, local) << std::noboolalpha
                          << std::endl;
                break;
            }
            default:
                // 保持循环，等待下一次输入。
                std::cout << "Unknown option." << std::endl;
                break;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "training_client error: " << ex.what() << std::endl;
        return 1;
    }
}
