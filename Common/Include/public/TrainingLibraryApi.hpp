#pragma once

#include <stdbool.h>

extern "C" {

typedef struct TrainingLibraryHandle TrainingLibraryHandle;

typedef struct TrainingInfoView {
    bool bool_param;
    int int_param;
    double double_param;
    const char* string_param;
} TrainingInfoView;

typedef struct TrainingListenerCallbacks {
    void* user_data;
    void (*on_test_bool_changed)(void* user_data, bool param);
    void (*on_test_int_changed)(void* user_data, int param);
    void (*on_test_double_changed)(void* user_data, double param);
    void (*on_test_string_changed)(void* user_data, const char* param);
    void (*on_test_info_changed)(void* user_data, const TrainingInfoView* param);
} TrainingListenerCallbacks;

typedef TrainingLibraryHandle* (*TrainingCreateFn)(void);
typedef void (*TrainingDestroyFn)(TrainingLibraryHandle* handle);
typedef void (*TrainingSetListenerFn)(TrainingLibraryHandle* handle, const TrainingListenerCallbacks* callbacks);
typedef bool (*TrainingSetTestBoolFn)(TrainingLibraryHandle* handle, bool param);
typedef bool (*TrainingSetTestIntFn)(TrainingLibraryHandle* handle, int param);
typedef bool (*TrainingSetTestDoubleFn)(TrainingLibraryHandle* handle, double param);
typedef bool (*TrainingSetTestStringFn)(TrainingLibraryHandle* handle, const char* param);
typedef bool (*TrainingSetTestInfoFn)(TrainingLibraryHandle* handle, const TrainingInfoView* param);
typedef bool (*TrainingGetTestBoolFn)(TrainingLibraryHandle* handle, bool* result);
typedef bool (*TrainingGetTestIntFn)(TrainingLibraryHandle* handle, int* result);
typedef bool (*TrainingGetTestDoubleFn)(TrainingLibraryHandle* handle, double* result);
typedef bool (*TrainingGetTestStringFn)(TrainingLibraryHandle* handle, const char** result);
typedef bool (*TrainingGetTestInfoFn)(TrainingLibraryHandle* handle, TrainingInfoView* result);
typedef const char* (*TrainingGetLastErrorFn)(void);

TrainingLibraryHandle* Training_Create(void);
void Training_Destroy(TrainingLibraryHandle* handle);
void Training_SetListener(TrainingLibraryHandle* handle, const TrainingListenerCallbacks* callbacks);
bool Training_SetTestBool(TrainingLibraryHandle* handle, bool param);
bool Training_SetTestInt(TrainingLibraryHandle* handle, int param);
bool Training_SetTestDouble(TrainingLibraryHandle* handle, double param);
bool Training_SetTestString(TrainingLibraryHandle* handle, const char* param);
bool Training_SetTestInfo(TrainingLibraryHandle* handle, const TrainingInfoView* param);
bool Training_GetTestBool(TrainingLibraryHandle* handle, bool* result);
bool Training_GetTestInt(TrainingLibraryHandle* handle, int* result);
bool Training_GetTestDouble(TrainingLibraryHandle* handle, double* result);
bool Training_GetTestString(TrainingLibraryHandle* handle, const char** result);
bool Training_GetTestInfo(TrainingLibraryHandle* handle, TrainingInfoView* result);
const char* Training_GetLastError(void);

}
