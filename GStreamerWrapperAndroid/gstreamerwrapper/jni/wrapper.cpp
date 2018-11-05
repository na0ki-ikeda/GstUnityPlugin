// GStreamerWrapper.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

//#include "stdafx.h"
//#include "Unity/IUnityGraphics.h"

//#include <d3d11.h>
//#include "Unity/IUnityGraphics.h"
//#include "Unity/IUnityGraphicsD3D11.h"

#define UNITY_INTERFACE_API
#define UNITY_INTERFACE_EXPORT

extern "C" void debug_log(const char* msg);

#include <map>
#include <vector>
#include <thread>
#include <mutex>

#include <iostream>
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

        //thread
        std::mutex VideoMutex;
        std::mutex AudioMutex;
        std::thread* Thread;

        //video
        int Texture;
        int Width;
        int Height;
        unsigned char* VideoBuffer;

        //audio
        std::vector<float> AudioBuffer;
        int Channels;
        int MaxAudioBufferLength;

        CustomData()
        {
            ID = -1;
            Pipeline = nullptr;
            Loop = nullptr;
            Alive = false;
            State = GST_STATE_NULL;
            Thread = nullptr;

            Texture = 0;
            Width = Height = 0;
            VideoBuffer = nullptr;

            Channels = 0;
            MaxAudioBufferLength = 0;
        }
        virtual ~CustomData() {}
    };

    //static IUnityInterfaces* s_unity = nullptr;
    //static ID3D11Device* s_device = nullptr;
    static int s_idCounter = 1001;
    static std::map<int, CustomData*> s_Data;
    static std::mutex s_DataMutex;
    static bool s_Initialized = false;

    static char* s_errorString = nullptr;

    CustomData* GetCustomData(const int id)
    {
        if(s_Data.count(id) == 0) return nullptr;

        return s_Data.at(id);
    }

    gboolean BusMessageCallback(GstBus* bus, GstMessage* msg, gpointer p)
    {
        auto* data = reinterpret_cast<CustomData*>(p);

        std::string logStr = "";
        logStr += "Got message: ";
        logStr += GST_MESSAGE_TYPE_NAME(msg);
        if(data)
        {
            logStr += ", ID: ";
            logStr += std::to_string(data->ID);
        }
        debug_log(logStr.c_str());
        //std::cout << logStr << std::endl;

        switch(GST_MESSAGE_TYPE(msg))
        {
            case GST_MESSAGE_ERROR:
            {
                GError *err = nullptr;
                gchar *debug;

                gst_message_parse_error(msg, &err, &debug);
                debug_log("Error: ");
                if(err != nullptr) debug_log(err->message);

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

    GstFlowReturn NewVideoSampleCallback(GstAppSink *sink, gpointer p)
    {
        auto* data = reinterpret_cast<CustomData*>(p);

        //std::cout << "hello new video sample" << std::endl;
        //debug_log("hello new video sample");

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

            //std::cout << "format: " << videoInfo.finfo->format << std::endl;
            //std::cout << "width: " << videoInfo.width << std::endl;
            //std::cout << "height: " << videoInfo.height << std::endl;

            //std::cout << "videoInfo.stride[0]: " << videoInfo.stride[0] << std::endl;
            //std::cout << "videoInfo.stride[1]: " << videoInfo.stride[1] << std::endl;
            //std::cout << "videoInfo.stride[2]: " << videoInfo.stride[2] << std::endl;


            //format check
            if(videoInfo.finfo->format != GST_VIDEO_FORMAT_I420)//this assumes pixel size is 1 byte and height is 1.5 times
            {
                //error!
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //size check
            if(data->Width != videoInfo.width || data->Height != videoInfo.height * 3 / 2)
            {
                //error!
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //copy to buffer
            auto buffer = gst_sample_get_buffer(sample);
            {
                std::lock_guard<std::mutex> lock(data->VideoMutex);

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

    GstFlowReturn NewAudioSampleCallback(GstAppSink *sink, gpointer p)
    {
        auto* data = reinterpret_cast<CustomData*>(p);

        //std::cout << "hello new audio sample" << std::endl;
        //debug_log("hello new audio sample");


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

            //std::cout << "format: " << audioInfo.finfo->format << std::endl;
            //std::cout << "channels: " << audioInfo.channels << std::endl;
            //std::cout << "rate(frequency): " << audioInfo.rate << std::endl;

            //format check
            if(audioInfo.finfo->format != GST_AUDIO_FORMAT_F32LE)//this format is the default for autoaudiosink
            {
                //error!
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //size check
            if(data->Channels != audioInfo.channels)
            {
                //error!
                gst_sample_unref(sample);
                return GST_FLOW_OK;
            }

            //copy to buffer
            auto buffer = gst_sample_get_buffer(sample);
            {
                std::lock_guard<std::mutex> lock(data->AudioMutex);

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

    void RegisterError(const char* error1, const char* error2)
    {
        if(s_errorString) delete[] s_errorString;

        s_errorString = new char[256];

        snprintf(s_errorString, 256, "%s %s", error1, error2);
    }
}

extern "C"
{

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetAndroidNumber()
    {
        return 12345;
    }

    UNITY_INTERFACE_EXPORT char* UNITY_INTERFACE_API GetLastErrorString()
    {
        auto str = new char[256];

        snprintf(str, 256, "%s", s_errorString == nullptr ? "no error" : s_errorString);

        return str;
    }

    //called automatically by Unity
//    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
//    {
//        s_unity = unityInterfaces;
//    }

//    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload()
//    {
//        //nothing special
//    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Initialize()
    {
        if(s_Initialized) return true;

#if 0
        //check DirectX11 build. 
        //if D3D12 build or D3D9 build or OpenGL/Vulkan build, Initialize() will return false
        if(s_unity == nullptr) return false;
        auto instance = s_unity->Get<IUnityGraphicsD3D11>();
        if(instance == nullptr)
        {
            debug_log("couldn't get D3D11 instance\n");
            std::cout << "couldn't get D3D11 instance" << std::endl;
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

            RegisterError("gst_parse_launch failed:", err->message);
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
            std::cout << "error!!!!: data->Loop is nullptr" << std::endl;
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
            RegisterError("both appVideoSink and appAudioSink are not set", "");

            delete data;
            return -1;
        }
        if(appVideoSink && !existVideoSinkSignal)
        {
            //sink exists but signal not available
            debug_log("appVideoSink exists but emit-signals is false\n");
            RegisterError("appVideoSink exists but emit-signals is false", "");

            delete data;
            return -1;
        }
        if(appAudioSink && !existAudioSinkSignal)
        {
            //sink exists but signal not available
            debug_log("appAudioSink exists but emit-signals is false\n");
            RegisterError("appAidopSink exists but emit-signals is false", "");

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

        if(s_errorString) delete[] s_errorString;
        s_Initialized = false;

        return true;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API SetTexture(const int id, int width, int height, void* ptr)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        data->Width = width;
        data->Height = height;
        data->VideoBuffer = new unsigned char[width*height];
        data->Texture = (int)reinterpret_cast<long long>(ptr);//static_cast<ID3D11Texture2D*>(ptr);;

        std::string logStr = "";
        logStr += "Texture ID: ";
        logStr += std::to_string(data->Texture);

        debug_log(logStr.c_str());

        return true;
    }

#include <GLES/gl.h>
#include <GLES2/gl2.h>
void glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params);

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API UpdateTexture(const int id)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        if(data->VideoBuffer == nullptr) return false;

        {
            std::lock_guard<std::mutex> lock(data->VideoMutex);

//            ID3D11DeviceContext* context;
//            s_device->GetImmediateContext(&context);
//            context->UpdateSubresource(data->Texture, 0, nullptr, data->VideoBuffer, data->Width, 0);//note: texture pitch is assumed as non exponent of 2 size texture

            GLenum err = glGetError();

            std::string logStr = "";
            logStr += "start: ";
            logStr += std::to_string(err);

            glBindTexture(GL_TEXTURE_2D, data->Texture);
            err = glGetError();
            logStr += "\nglBindTexture: ";
            logStr += std::to_string(err);


            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            err = glGetError();
            logStr += "\nglTexParameterf: ";
            logStr += std::to_string(err);


            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            err = glGetError();
            logStr += "\nglTexParameterf: ";
            logStr += std::to_string(err);


            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            err = glGetError();
            logStr += "\nglPixelStorei: ";
            logStr += std::to_string(err);

            int texWidth, texHeight;

            //#define GL_TEXTURE_WIDTH                  0x1000
            //#define GL_TEXTURE_HEIGHT                 0x1001
            glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, 0x1000, &texWidth);
            glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, 0x1001, &texHeight);
            logStr += "\nWidth: ";
            logStr += std::to_string(texWidth);
            logStr += "\nHeight: ";
            logStr += std::to_string(texHeight);

            logStr += "\nWidthUnity: ";
            logStr += std::to_string(data->Width);
            logStr += "\nHeightUnity: ";
            logStr += std::to_string(data->Height);

            int texFormat;
            //#define GL_TEXTURE_INTERNAL_FORMAT        0x1003
            glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, 0x1003, &texFormat);
            logStr += "\nFormat: ";
            logStr += std::to_string(texFormat);

            //RGBA8_EXT                        0x8058
                    //R8                      0x8229
//#define GL_RGBA                           0x1908

            glTexSubImage2D
                    (
                            GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            data->Width,
                            data->Height/4,
                            0x1908,//#define GL_RED                            0x1903 #define GL_ALPHA                          0x1906
                            0x1401,//#define GL_UNSIGNED_BYTE                  0x1401
                            data->VideoBuffer
                    );

            err = glGetError();
            logStr += "\nglTexSubImage2D: ";
            logStr += std::to_string(err);

            glBindTexture(GL_TEXTURE_2D, 0);

            err = glGetError();
            logStr += "\nglBindTexture: ";
            logStr += std::to_string(err);

            debug_log(logStr.c_str());


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

    typedef void (* UnityRenderingEvent)(int eventId);

    UNITY_INTERFACE_EXPORT UnityRenderingEvent UNITY_INTERFACE_API GetRenderEventFunc()
    {
        return OnRenderEvent;
    }


    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API SetAudioInfo(const int id, const int channels, const int maxBufferLength)
    {
        if(!s_Initialized) return false;

        auto data = GetCustomData(id);
        if(data == nullptr) return false;

        {
            std::lock_guard<std::mutex> lock(data->AudioMutex);

            data->Channels = channels;
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

}
