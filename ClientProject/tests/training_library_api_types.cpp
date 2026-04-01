#include <public/DbusConstants.hpp>
#include <public/TrainingLibraryApi.hpp>

#include <type_traits>

using ExpectedSetTestInfoFn =
    bool (*)(TrainingLibraryHandle*, const training::public_api::TestInfo*);
using ExpectedGetTestInfoFn =
    bool (*)(TrainingLibraryHandle*, training::public_api::TestInfo*);
using ExpectedInfoListenerFn =
    void (*)(void*, const training::public_api::TestInfo*);
using ExpectedSendFilePathFn =
    bool (*)(TrainingLibraryHandle*, const char*, const char*);
using ExpectedSendFileBufferFn =
    bool (*)(TrainingLibraryHandle*, const unsigned char*, unsigned long long, const char*, const char*);
using ExpectedDownloadFileFn =
    bool (*)(TrainingLibraryHandle*, const char*, const char*);

static_assert(std::is_same_v<TrainingSetTestInfoFn, ExpectedSetTestInfoFn>,
              "TrainingSetTestInfoFn should use TestInfo directly");
static_assert(std::is_same_v<TrainingGetTestInfoFn, ExpectedGetTestInfoFn>,
              "TrainingGetTestInfoFn should use TestInfo directly");
static_assert(std::is_same_v<decltype(TrainingListenerCallbacks::on_test_info_changed), ExpectedInfoListenerFn>,
              "on_test_info_changed should use TestInfo directly");
static_assert(std::is_same_v<TrainingSendFilePathFn, ExpectedSendFilePathFn>,
              "TrainingSendFilePathFn should send file by path through dynamic library");
static_assert(std::is_same_v<TrainingSendFileBufferFn, ExpectedSendFileBufferFn>,
              "TrainingSendFileBufferFn should include remote path");
static_assert(std::is_same_v<TrainingDownloadFileFn, ExpectedDownloadFileFn>,
              "TrainingDownloadFileFn should download a remote file to a local path");

int main() {
    return 0;
}
