
#include "stdafx.h"

#if _WINDOWS
#define UNITY_INTERFACE_API __stdcall
#define UNITY_INTERFACE_EXPORT __declspec(dllexport)
#else
#define UNITY_INTERFACE_API
#define UNITY_INTERFACE_EXPORT
#endif

extern "C"
{
    using debug_log_func_type = void(*)(const char*);

    namespace
    {
        debug_log_func_type debug_log_func = nullptr;
    }

    void debug_log(const char* msg)
    {
        if (debug_log_func != nullptr) debug_log_func(msg);
    }

    bool is_debug_log()
    {
        return debug_log_func != nullptr;
    }

    UNITY_INTERFACE_EXPORT void set_debug_log_func(debug_log_func_type func)
    {
        debug_log_func = func;
    }

    UNITY_INTERFACE_EXPORT void debug_log_test()
    {
        debug_log("hogehoge");
    }
}
