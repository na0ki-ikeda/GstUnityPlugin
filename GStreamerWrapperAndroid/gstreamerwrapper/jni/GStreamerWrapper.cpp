#include "stdafx.h"

#ifdef _WINDOWS
#define UNITY_INTERFACE_API __stdcall
#define UNITY_INTERFACE_EXPORT __declspec(dllexport)
#else
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#define UNITY_INTERFACE_API
#define UNITY_INTERFACE_EXPORT
typedef void(*UnityRenderingEvent)(int eventId);
#endif

extern "C" void debug_log(const char* msg);
extern "C" bool is_debug_log();

#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
//#include <iostream>
#include <string>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#pragma comment(lib, "gstapp-1.0.lib")
#pragma comment(lib, "gstvideo-1.0.lib")
#pragma comment(lib, "gstaudio-1.0.lib")
#pragma comment(lib, "gstapp-1.0.lib")
#pragma comment(lib, "gstbase-1.0.lib")
#pragma comment(lib, "gstreamer-1.0.lib")
#pragma comment(lib, "gobject-2.0.lib")
#pragma comment(lib, "glib-2.0.lib")

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>  

namespace
{
    struct CustomData
    {
        int ID;

        //gst
        GstElement* Pipeline;
        GMainLoop* Loop;
        bool Alive;
        GstState State;
        bool ErrorDetected;

        //thread
        std::mutex VideoMutex;
        std::mutex AudioMutex;
        std::thread* Thread;

        //video
#ifdef _WINDOWS
        ID3D11Texture2D* Texture;
#else
        unsigned int Texture;
#endif
        int Width;
        int Height;
        unsigned char* VideoBuffer;
        bool VideoSampleAvailable;

        //audio
        std::vector<float> AudioBuffer;
        int Channels;
        int SampleRate;
        int MaxAudioBufferLength;
        bool AudioSampleAvailable;

        CustomData()
        {
            ID = -1;
            Pipeline = nullptr;
            Loop = nullptr;
            Alive = false;
            State = GST_STATE_NULL;
            ErrorDetected = false;

            Thread = nullptr;
#ifdef _WINDOWS
            Texture = nullptr;
#else
            Texture = 0;
#endif
            Width = Height = 0;
            VideoBuffer = nullptr;
            VideoSampleAvailable = false;

            Channels = SampleRate = 0;
            MaxAudioBufferLength = 0;
            AudioSampleAvailable = false;
        }
        virtual ~CustomData() {}
    };
#ifdef _WINDOWS
    static IUnityInterfaces* s_unity = nullptr;
    static ID3D11Device* s_device = nullptr;
#endif
    static int s_idCounter = 1001;
    static std::map<int, CustomData*> s_Data;
    static std::mutex s_DataMutex;
    static bool s_Initialized = false;

    CustomData* GetCustomData(const int id)
    {
        if(s_Data.count(id) == 0) return nullptr;

        return s_Data.at(id);
    }

    gboolean BusMessageCallback(GstBus* bus, GstMessage* msg, gpointer p)
    {
        auto* data = reinterpret_cast<CustomData*>(p);

        if(is_debug_log())
        {
            std::string logStr = "";
            logStr += "Got message: ";
            logStr += GST_MESSAGE_TYPE_NAME(msg);
            if(data)
            {
                logStr += ", ID: ";
                logStr += std::to_string(data->ID);
            }
            debug_log(logStr.c_str());
        }

        switch(GST_MESSAGE_TYPE(msg))
        {
            case GST_MESSAGE_ERROR:
            {
                GError *err = nullptr;
                gchar *debug;

                gst_message_parse_error(msg, &err, &debug);

                debug_log("Error detected!");
                if(err != nullptr) debug_log(err->message);
                data->ErrorDetected = true;

                if(err != nullptr) g_error_free(err);
                g_free(debug);

                gst_element_set_state(data->Pipeline, GST_STATE_READY);
                g_main_loop_quit(data->Loop);
                break;
            }
            case GST_MESSAGE_EOS:
                /* end-of-stream */
                gst_element_set_state(data->Pipeline, GST_STATE_READY);
                g_main_loop_quit(data->Loop);
                break;
            case GST_MESSAGE_BUFFERING:
            {
                gint percent = 0;

                /* If the stream is live, we do not care about buffering. */
                if(data->Alive) break;

                gst_message_parse_buffering(msg, &percent);

                /* Wait until buffering is complete before start/resume playing */
                if(percent < 100)
                    gst_element_set_state(data->Pipeline, GST_STATE_PAUSED);
                else
                    gst_element_set_state(data->Pipeline, GST_STATE_PLAYING);
                break;
            }
            case GST_MESSAGE_CLOCK_LOST:
                /* Get a new clock */
                gst_element_set_state(data->Pipeline, GST_STATE_PAUSED);
                gst_element_set_state(data->Pipeline, GST_STATE_PLAYING);
                break;
            case GST_MESSAGE_STATE_CHANGED:
            {
                GstState oldState, newState;
                gst_message_parse_state_changed(msg, &oldState, &newState, NULL);
                data->State = newState;
                break;
            }
            default:
                /* Unhandled message */
                break;
        }

        return TRUE;
    }

    const char* GetVideoFormatName(const GstVideoFormat format)
    {
        static const char* names[] =
        {
            "GST_VIDEO_FORMAT_UNKNOWN",
            "GST_VIDEO_FORMAT_ENCODED",
            "GST_VIDEO_FORMAT_I420",
            "GST_VIDEO_FORMAT_YV12",
            "GST_VIDEO_FORMAT_YUY2",
            "GST_VIDEO_FORMAT_UYVY",
            "GST_VIDEO_FORMAT_AYUV",
            "GST_VIDEO_FORMAT_RGBx",
            "GST_VIDEO_FORMAT_BGRx",
            "GST_VIDEO_FORMAT_xRGB",
            "GST_VIDEO_FORMAT_xBGR",
            "GST_VIDEO_FORMAT_RGBA",
            "GST_VIDEO_FORMAT_BGRA",
            "GST_VIDEO_FORMAT_ARGB",
            "GST_VIDEO_FORMAT_ABGR",
            "GST_VIDEO_FORMAT_RGB",
            "GST_VIDEO_FORMAT_BGR",
            "GST_VIDEO_FORMAT_Y41B",
            "GST_VIDEO_FORMAT_Y42B",
            "GST_VIDEO_FORMAT_YVYU",
            "GST_VIDEO_FORMAT_Y444",
            "GST_VIDEO_FORMAT_v210",
            "GST_VIDEO_FORMAT_v216",
            "GST_VIDEO_FORMAT_NV12",
            "GST_VIDEO_FORMAT_NV21",
            "GST_VIDEO_FORMAT_GRAY8",
            "GST_VIDEO_FORMAT_GRAY16_BE",
            "GST_VIDEO_FORMAT_GRAY16_LE",
            "GST_VIDEO_FORMAT_v308",
            "GST_VIDEO_FORMAT_RGB16",
            "GST_VIDEO_FORMAT_BGR16",
            "GST_VIDEO_FORMAT_RGB15",
            "GST_VIDEO_FORMAT_BGR15",
            "GST_VIDEO_FORMAT_UYVP",
            "GST_VIDEO_FORMAT_A420",
            "GST_VIDEO_FORMAT_RGB8P",
            "GST_VIDEO_FORMAT_YUV9",
            "GST_VIDEO_FORMAT_YVU9",
            "GST_VIDEO_FORMAT_IYU1",
            "GST_VIDEO_FORMAT_ARGB64",
            "GST_VIDEO_FORMAT_AYUV64",
            "GST_VIDEO_FORMAT_r210",
            "GST_VIDEO_FORMAT_I420_10BE",
            "GST_VIDEO_FORMAT_I420_10LE",
            "GST_VIDEO_FORMAT_I422_10BE",
            "GST_VIDEO_FORMAT_I422_10LE",
            "GST_VIDEO_FORMAT_Y444_10BE",
            "GST_VIDEO_FORMAT_Y444_10LE",
            "GST_VIDEO_FORMAT_GBR",
            "GST_VIDEO_FORMAT_GBR_10BE",
            "GST_VIDEO_FORMAT_GBR_10LE",
            "GST_VIDEO_FORMAT_NV16",
            "GST_VIDEO_FORMAT_NV24",
            "GST_VIDEO_FORMAT_NV12_64Z32",
            "GST_VIDEO_FORMAT_A420_10BE",
            "GST_VIDEO_FORMAT_A420_10LE",
            "GST_VIDEO_FORMAT_A422_10BE",
            "GST_VIDEO_FORMAT_A422_10LE",
            "GST_VIDEO_FORMAT_A444_10BE",
            "GST_VIDEO_FORMAT_A444_10LE",
            "GST_VIDEO_FORMAT_NV61",
            "GST_VIDEO_FORMAT_P010_10BE",
            "GST_VIDEO_FORMAT_P010_10LE",
            "GST_VIDEO_FORMAT_IYU2",
            "GST_VIDEO_FORMAT_VYUY",
            "GST_VIDEO_FORMAT_GBRA",
            "GST_VIDEO_FORMAT_GBRA_10BE",
            "GST_VIDEO_FORMAT_GBRA_10LE",
            "GST_VIDEO_FORMAT_GBR_12BE",
            "GST_VIDEO_FORMAT_GBR_12LE",
            "GST_VIDEO_FORMAT_GBRA_12BE",
            "GST_VIDEO_FORMAT_GBRA_12LE",
            "GST_VIDEO_FORMAT_I420_12BE",
            "GST_VIDEO_FORMAT_I420_12LE",
            "GST_VIDEO_FORMAT_I422_12BE",
            "GST_VIDEO_FORMAT_I422_12LE",
            "GST_VIDEO_FORMAT_Y444_12BE",
            "GST_VIDEO_FORMAT_Y444_12LE",
            "GST_VIDEO_FORMAT_GRAY10_LE32",
            "GST_VIDEO_FORMAT_NV12_10LE32",
            "GST_VIDEO_FORMAT_NV16_10LE32"
        };

        return names[format];
    }

    GstFlowReturn NewVideoSampleCallback(GstAppSink *sink, gpointer p)
    {
        auto* data = reinterpret_cast<CustomData*>(p);
        auto sample = gst_app_sink_pull_sample(sink);

        if(sample == nullptr)
        {
            return GST_FLOW_EOS;
        }
        else
        {
            auto caps = gst_sample_get_caps(sample);
            GstVideoInfo videoInfo;
            gst_video_info_init(&videoInfo);

            if(!caps || !gst_video_info_from_caps(&videoInfo, caps))
            {
                //error!
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //format check
            if(videoInfo.finfo->format != GST_VIDEO_FORMAT_I420)//this assumes pixel size is 1 byte and height is 1.5 times
            {
                //error!
                if(is_debug_log())
                {
                    std::string logStr = "Video Format Error detected!\n";
                    logStr += "video format: ";
                    logStr += GetVideoFormatName(videoInfo.finfo->format);
                    logStr += "\n";
                    debug_log(logStr.c_str());
                }
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //size check
            if(data->Width != videoInfo.width || data->Height != videoInfo.height * 3 / 2)
            {
                //error!
                if(is_debug_log())
                {
                    std::string logStr = "Video Size(width/height) Error detected!";
                    logStr += "video width: " + std::to_string(videoInfo.width) + "\n";
                    logStr += "video height: " + std::to_string(videoInfo.height) + "\n";
                    logStr += "video stride[0]: " + std::to_string(videoInfo.stride[0]) + "\n";
                    logStr += "video stride[1]: " + std::to_string(videoInfo.stride[1]) + "\n";
                    logStr += "video stride[2]: " + std::to_string(videoInfo.stride[2]) + "\n";
                    debug_log(logStr.c_str());
                }
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //copy to buffer
            auto buffer = gst_sample_get_buffer(sample);
            {
                std::lock_guard<std::mutex> lock(data->VideoMutex);
                data->VideoSampleAvailable = true;

                GstMapInfo mapInfo;
                if(gst_buffer_map(buffer, &mapInfo, GST_MAP_READ))
                {
                    //copy y data
                    auto src = mapInfo.data;
                    auto dst = data->VideoBuffer;
                    for(auto y = 0; y < videoInfo.height; y++)
                    {
                        memcpy(dst, src, videoInfo.width);
                        dst += videoInfo.width;
                        src += videoInfo.stride[0];
                    }

                    //copy u and v
                    for(auto y = 0; y < videoInfo.height / 2; y++)
                    {
                        //v
                        memcpy(dst, src, videoInfo.width / 2);
                        dst += videoInfo.width / 2;
                        src += videoInfo.stride[1];

                        //u
                        memcpy(dst, src, videoInfo.width / 2);
                        dst += videoInfo.width / 2;
                        src += videoInfo.stride[2];
                    }

                    gst_buffer_unmap(buffer, &mapInfo);
                }
            }
            //gst_buffer_unref(buffer);//this occurs an error

            gst_sample_unref(sample);

            return GST_FLOW_OK;
        }
    }

    const char* GetAudioFormatName(const GstAudioFormat format)
    {
        static const char* names[] =
        {
            "GST_AUDIO_FORMAT_UNKNOWN",
            "GST_AUDIO_FORMAT_ENCODED",
            "GST_AUDIO_FORMAT_S8",
            "GST_AUDIO_FORMAT_U8",
            "GST_AUDIO_FORMAT_S16LE",
            "GST_AUDIO_FORMAT_S16BE",
            "GST_AUDIO_FORMAT_U16LE",
            "GST_AUDIO_FORMAT_U16BE",
            "GST_AUDIO_FORMAT_S24_32LE",
            "GST_AUDIO_FORMAT_S24_32BE",
            "GST_AUDIO_FORMAT_U24_32LE",
            "GST_AUDIO_FORMAT_U24_32BE",
            "GST_AUDIO_FORMAT_S32LE",
            "GST_AUDIO_FORMAT_S32BE",
            "GST_AUDIO_FORMAT_U32LE",
            "GST_AUDIO_FORMAT_U32BE",
            "GST_AUDIO_FORMAT_S24LE",
            "GST_AUDIO_FORMAT_S24BE",
            "GST_AUDIO_FORMAT_U24LE",
            "GST_AUDIO_FORMAT_U24BE",
            "GST_AUDIO_FORMAT_S20LE",
            "GST_AUDIO_FORMAT_S20BE",
            "GST_AUDIO_FORMAT_U20LE",
            "GST_AUDIO_FORMAT_U20BE",
            "GST_AUDIO_FORMAT_S18LE",
            "GST_AUDIO_FORMAT_S18BE",
            "GST_AUDIO_FORMAT_U18LE",
            "GST_AUDIO_FORMAT_U18BE",
            "GST_AUDIO_FORMAT_F32LE",
            "GST_AUDIO_FORMAT_F32BE",
            "GST_AUDIO_FORMAT_F64LE",
            "GST_AUDIO_FORMAT_F64BE"
        };

        return names[format];
    }

    GstFlowReturn NewAudioSampleCallback(GstAppSink *sink, gpointer p)
    {
        auto* data = reinterpret_cast<CustomData*>(p);
        auto sample = gst_app_sink_pull_sample(sink);

        if(sample == nullptr)
        {
            return GST_FLOW_EOS;
        }
        else
        {
            auto caps = gst_sample_get_caps(sample);
            GstAudioInfo audioInfo;
            gst_audio_info_init(&audioInfo);

            if(!caps || !gst_audio_info_from_caps(&audioInfo, caps))
            {
                //error!
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }


            //format check
            if(audioInfo.finfo->format != GST_AUDIO_FORMAT_F32LE)//this format is the default for autoaudiosink
            {
                //error!
                if(is_debug_log())
                {
                    std::string logStr = "Audio Format Error detected!\n";
                    logStr += "audio format: ";
                    logStr += GetAudioFormatName(audioInfo.finfo->format);
                    logStr += "\n";
                    debug_log(logStr.c_str());
                }

                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //size check
            if(data->Channels != audioInfo.channels || data->SampleRate != audioInfo.rate)
            {
                //error!
                if(is_debug_log())
                {
                    std::string logStr = "Video channel/samplerate Error detected!\n";
                    logStr += "audio channel  : " + std::to_string(audioInfo.channels) + "\n";
                    logStr += "audio rate     : " + std::to_string(audioInfo.rate) + "\n";
                    debug_log(logStr.c_str());
                }
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //copy to buffer
            auto buffer = gst_sample_get_buffer(sample);
            {
                std::lock_guard<std::mutex> lock(data->AudioMutex);
                data->AudioSampleAvailable = true;

                GstMapInfo mapInfo;
                if(gst_buffer_map(buffer, &mapInfo, GST_MAP_READ))
                {
                    auto length = mapInfo.size / (sizeof(float));//for GST_AUDIO_FORMAT_F32LE

                    auto src = reinterpret_cast<float*>(mapInfo.data);
                    data->AudioBuffer.insert(data->AudioBuffer.end(), src, src + length);

                    //std::cout << "added length: " << length << std::endl;

                    //check length
                    int overLength = (int)data->AudioBuffer.size() - data->MaxAudioBufferLength;
                    if(overLength > 0)
                    {
                        //erase over buffer
                        data->AudioBuffer.erase(data->AudioBuffer.begin(), std::next(data->AudioBuffer.begin(), overLength));
                    }

                    gst_buffer_unmap(buffer, &mapInfo);
                }
            }
            //gst_buffer_unref(buffer);//this occurs an error

            gst_sample_unref(sample);

            return GST_FLOW_OK;
        }
    }
}

extern "C"
{
#ifdef _WINDOWS
    //called automatically by Unity
    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
    {
        s_unity = unityInterfaces;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload()
    {
        //nothing special
    }
#endif

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Initialize()
    {
        if(s_Initialized) return true;

#ifdef _WINDOWS
        //check DirectX11 build. 
        //if D3D12 build or D3D9 build or OpenGL/Vulkan build, Initialize() will return false
        if(s_unity == nullptr) return false;
        auto instance = s_unity->Get<IUnityGraphicsD3D11>();
        if(instance == nullptr)
        {
            debug_log("couldn't get D3D11 instance\n");
            return false;
        }
        s_device = instance->GetDevice();
        if(s_device == nullptr) return false;
#endif

        //initialize param
        auto argc = 1;
        auto argv = { "GStreamerWrapper" };
        //gst_init(&argc, (char***)&argv);

        //check initialize
        GError* err = nullptr;
        if(!gst_init_check(&argc, (char***)&argv, &err))
        {
            debug_log("gst_init_check failed\n");
            return false;
        }
        if(err != nullptr) g_error_free(err);

        //OK
        s_Initialized = true;

        return true;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Play(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        auto ret = gst_element_set_state(data->Pipeline, GST_STATE_PLAYING);
        if(ret == GST_STATE_CHANGE_FAILURE)
        {
            debug_log("Unable to set the pipeline to the playing state.\n");
            return false;
        }
        else if(ret == GST_STATE_CHANGE_NO_PREROLL)
        {
            data->Alive = true;
        }
        return true;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Pause(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        auto ret = gst_element_set_state(data->Pipeline, GST_STATE_PAUSED);
        if(ret == GST_STATE_CHANGE_FAILURE)
        {
            debug_log("Unable to set the pipeline to the pause state.\n");
            return false;
        }
        return true;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API AddPipeline(const char* description)
    {
        if(!s_Initialized) return -1;

        auto data = new CustomData();

        GError* err = nullptr;
        data->Pipeline = gst_parse_launch(description, &err);
        if(data->Pipeline == nullptr)
        {
            std::string errStr = "";
            errStr += "gst_parse_launch failed: ";
            errStr += description;
            errStr += ": ";
            errStr += err->message;

            debug_log(errStr.c_str());
        }
        if(err != nullptr) g_error_free(err);

        if(data->Pipeline == nullptr)
        {
            delete data;
            return -1;
        }

        //create glib loop
        data->Loop = g_main_loop_new(NULL, FALSE);

        if(data->Loop == nullptr)
        {
            delete data;
            return -1;
        }

        //set bus callback
        auto bus = gst_element_get_bus(data->Pipeline);
        {
            gst_bus_add_signal_watch(bus);
            g_signal_connect(bus, "message", G_CALLBACK(BusMessageCallback), data);
        }
        gst_object_unref(bus);

        //find app sink
        auto appVideoSink = gst_bin_get_by_name(GST_BIN(data->Pipeline), "appVideoSink");
        auto appAudioSink = gst_bin_get_by_name(GST_BIN(data->Pipeline), "appAudioSink");
        bool existVideoSinkSignal = false;
        bool existAudioSinkSignal = false;
        {
            //check Signal
            if(appVideoSink)
            {
                gchar* emitSignals = nullptr;
                g_object_get(G_OBJECT(appVideoSink), "emit-signals", &emitSignals, NULL);

                if(emitSignals)
                {
                    existVideoSinkSignal = true;
                }
            }
            if(appAudioSink)
            {
                gchar* emitSignals = nullptr;
                g_object_get(G_OBJECT(appAudioSink), "emit-signals", &emitSignals, NULL);

                if(emitSignals)
                {
                    existAudioSinkSignal = true;
                }
            }

            //set appsink callback
            if(existVideoSinkSignal)
            {
                g_signal_connect(appVideoSink, "new-sample", G_CALLBACK(NewVideoSampleCallback), data);
            }
            if(existAudioSinkSignal)
            {
                g_signal_connect(appAudioSink, "new-sample", G_CALLBACK(NewAudioSampleCallback), data);
            }
        }
        if(appVideoSink) gst_object_unref(appVideoSink);
        if(appAudioSink) gst_object_unref(appAudioSink);

        if(appVideoSink == nullptr && appAudioSink == nullptr)
        {
            debug_log("both appVideoSink and appAudioSink are not set\n");

            delete data;
            return -1;
        }
        if(appVideoSink && !existVideoSinkSignal)
        {
            //sink exists but signal not available
            debug_log("appVideoSink exists but emit-signals is false\n");

            delete data;
            return -1;
        }
        if(appAudioSink && !existAudioSinkSignal)
        {
            //sink exists but signal not available
            debug_log("appAudioSink exists but emit-signals is false\n");

            delete data;
            return -1;
        }

        //set state to ready
        gst_element_set_state(data->Pipeline, GST_STATE_READY);

        //start thread
        bool launched = false;
        data->Thread = new std::thread([&]
        {
            launched = true;
            g_main_loop_run(data->Loop);
        });

        //wait for the thread launched
        while(!launched)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        //register data
        data->ID = s_idCounter++;
        s_Data[data->ID] = data;

        return data->ID;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API RemovePipeline(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        //stop
        g_main_loop_quit(data->Loop);
        data->Thread->join();

        //unref <= unref order is important
        g_main_loop_unref(data->Loop);
        gst_element_set_state(data->Pipeline, GST_STATE_NULL);
        gst_object_unref(data->Pipeline);

        //delete
        delete data->Thread;
        delete[] data->VideoBuffer;
        data->AudioBuffer.clear();
        delete data;

        //delete from map
        {
            std::lock_guard<std::mutex> lock(s_DataMutex);
            s_Data.erase(id);
        }
        return true;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Finalize()
    {
        if(!s_Initialized) return true;

        std::vector<CustomData*> vec;

        //stop and destroy
        for(auto pair : s_Data)
        {
            vec.push_back(pair.second);
        }
        for(auto data : vec)
        {
            RemovePipeline(data->ID);
        }

        {
            std::lock_guard<std::mutex> lock(s_DataMutex);
            s_Data.clear();
        }

        //shutdown gstreamer
        //gst_deinit();//This occurs AddPipeline() errors after the second time of Initialize(), I'm guessing this function wasn't tested

        s_Initialized = false;

        return true;
    }

#ifdef _WINDOWS
    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API SetTexture(const int id, int width, int height, void* ptr)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        data->Width = width;
        data->Height = height;
        data->VideoBuffer = new unsigned char[width*height];
        data->Texture = static_cast<ID3D11Texture2D*>(ptr);

        memset(data->VideoBuffer, 0, width * height);

        return true;
    }
#else
    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API SetVideoInfo(const int id, int width, int height)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        //set parameters
        data->Width = width;
        data->Height = height;
        data->VideoBuffer = new unsigned char[width*height];//GL_ALPHA
        memset(data->VideoBuffer, 0, width*height);

        return true;
    }

    UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetTexturePtr(const int id)
    {
        if(!s_Initialized) return nullptr;

        auto data = GetCustomData(id);
        if(data == nullptr) return nullptr;

        return reinterpret_cast<void*>(data->Texture);
    }
#endif

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API UpdateTexture(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        if(data->VideoBuffer == nullptr) return false;

        {
            std::lock_guard<std::mutex> lock(data->VideoMutex);
#ifdef _WINDOWS
            ID3D11DeviceContext* context;
            s_device->GetImmediateContext(&context);
            context->UpdateSubresource(data->Texture, 0, nullptr, data->VideoBuffer, data->Width, 0);//note: texture pitch is assumed as non exponent of 2 size texture
#else
            if(data->Texture == 0)
            {
                //check buffer
                if(data->VideoBuffer != nullptr)
                {
                    //generate texture
                    glGenTextures(1, &data->Texture);

                    //set GL_ALPHA format
                    glBindTexture(GL_TEXTURE_2D, data->Texture);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, data->Width, data->Height, 0, GL_ALPHA,
                        GL_UNSIGNED_BYTE, data->VideoBuffer);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }
            else {
                glBindTexture(GL_TEXTURE_2D, data->Texture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D
                (
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    data->Width,
                    data->Height,
                    GL_ALPHA,
                    GL_UNSIGNED_BYTE,
                    data->VideoBuffer
                );
                glBindTexture(GL_TEXTURE_2D, 0);
            }
#endif
        }
        return true;
    }

    //called from render thread of Unity
    void UNITY_INTERFACE_API OnRenderEvent(int eventId)
    {
        std::lock_guard<std::mutex> lock(s_DataMutex);

        for(auto pair : s_Data)
        {
            UpdateTexture(pair.first);
        }
    }

    UNITY_INTERFACE_EXPORT UnityRenderingEvent UNITY_INTERFACE_API GetRenderEventFunc()
    {
        return OnRenderEvent;
    }


    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API SetAudioInfo(const int id, const int channels, const int sampleRate, const int maxBufferLength)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        {
            std::lock_guard<std::mutex> lock(data->AudioMutex);

            data->Channels = channels;
            data->SampleRate = sampleRate;
            data->MaxAudioBufferLength = maxBufferLength;
        }

        return true;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetAudioBufferLength(const int id)
    {
        if(!s_Initialized) return -1;

        auto data = GetCustomData(id);
        if(data == nullptr) return -1;

        int length = 0;

        {
            std::lock_guard<std::mutex> lock(data->AudioMutex);
            length = (int)data->AudioBuffer.size();
        }

        return length;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API GetAudioBuffer(const int id, float* buffer, const int bufferLength)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        {
            std::lock_guard<std::mutex> lock(data->AudioMutex);
            auto before = data->AudioBuffer.size();

            if(data->AudioBuffer.size() < bufferLength) return false;

            //copy
            memcpy(buffer, data->AudioBuffer.data(), bufferLength * sizeof(float));

            //erase
            data->AudioBuffer.erase(data->AudioBuffer.begin(), std::next(data->AudioBuffer.begin(), bufferLength));

            //std::cout << "length changed: " << before << " => " << data->AudioBuffer.size() << ", " << bufferLength << std::endl;
        }

        return true;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API ShowAllGstElements()
    {  
        auto elements = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ANY, GST_RANK_NONE);
           
        for(auto l = elements; l; l = l->next)
        {
            auto f = reinterpret_cast<GstElementFactory*>(l->data);
            //g_print("factory: %s\n", GST_OBJECT_NAME(f));

            std::string errStr = "";
            errStr += "Element: ";
            errStr += GST_OBJECT_NAME(f);

            debug_log(errStr.c_str());
        }

        gst_plugin_feature_list_free(elements);

        return is_debug_log();
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsErrorDetected(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        return data->ErrorDetected;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API CheckVideoAndSetFalse(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        bool result = false;       
        {
            std::lock_guard<std::mutex> lock(data->VideoMutex);
            result = data->VideoSampleAvailable;
            data->VideoSampleAvailable = false;
        }

        return result;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API CheckAudioAndSetFalse(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        bool result = false;
        {
            std::lock_guard<std::mutex> lock(data->AudioMutex);
            result = data->AudioSampleAvailable;
            data->AudioSampleAvailable = false;
        }

        return result;
    }

}
