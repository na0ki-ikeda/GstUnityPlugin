// GStreamerWrapperTest.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include "pch.h"


#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC 
#include <stdlib.h> 
#include <crtdbg.h>  

#define malloc(s) _malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define calloc(c, s) _calloc_dbg(c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define realloc(p, s) _realloc_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define _recalloc(p, c, s) _recalloc_dbg(p, c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define _expand(p, s) _expand_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define new ::new(_NORMAL_BLOCK, __FILE__, __LINE__) 

#endif

#include <iostream>
#include <list>

#include <gst/gst.h>

#pragma comment(lib, "gstapp-1.0.lib")
#pragma comment(lib, "gstbase-1.0.lib")
#pragma comment(lib, "gstreamer-1.0.lib")
#pragma comment(lib, "gobject-2.0.lib")
#pragma comment(lib, "glib-2.0.lib")

#pragma comment(lib, "GStreamerWrapper.lib")

static GMainLoop *loop;

typedef struct _CustomData {
    gboolean is_live;
    GstElement *pipeline;
    GMainLoop *loop;
} CustomData;

gboolean my_bus_callback(GstBus* bus, GstMessage* msg, void* p)
{
    auto* data = reinterpret_cast<CustomData*>(p);

    g_print("Got %s message\n", GST_MESSAGE_TYPE_NAME(msg));

    switch(GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_ERROR:
        {
            GError *err = nullptr;
            gchar *debug;

            gst_message_parse_error(msg, &err, &debug);
            g_print("Error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);

            gst_element_set_state(data->pipeline, GST_STATE_READY);
            g_main_loop_quit(data->loop);
            break;
        }
        case GST_MESSAGE_EOS:
            /* end-of-stream */
            gst_element_set_state(data->pipeline, GST_STATE_READY);
            g_main_loop_quit(data->loop);
            break;
        case GST_MESSAGE_BUFFERING: {
            gint percent = 0;

            /* If the stream is live, we do not care about buffering. */
            if(data->is_live) break;

            gst_message_parse_buffering(msg, &percent);
            g_print("Buffering (%3d%%)\r", percent);
            /* Wait until buffering is complete before start/resume playing */
            if(percent < 100)
                gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
            else
                gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
            break;
        }
        case GST_MESSAGE_CLOCK_LOST:
            /* Get a new clock */
            gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
            gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
            break;
        default:
            /* Unhandled message */
            break;
    }

    return TRUE;
}

int func(int argc, char *argv[])
{
    GMainLoop *mainloop;
    GstElement *pipeline;
    GError *error = nullptr;
    GstBus *bus;
    GstStateChangeReturn ret;
    CustomData data;

    gst_init(&argc, &argv);

    mainloop = g_main_loop_new(NULL, FALSE);
    pipeline = gst_parse_launch("videotestsrc ! autovideosink", &error);
    bus = gst_element_get_bus(pipeline);


    data.loop = mainloop;
    data.is_live = false;
    data.pipeline = pipeline;
    gst_bus_add_watch(bus, my_bus_callback, &data);

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if(ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    else if(ret == GST_STATE_CHANGE_NO_PREROLL)
    {
        data.is_live = TRUE;
    }

    g_main_loop_run(mainloop);

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    gst_deinit();//全てのpipelineがGST_STATE_NULLになっていないとここでブロックされる

    return 0;
}

extern "C"
{
    bool Initialize();//This requires "IUnityGraphicsD3D11 is disabled"
    int AddPipeline(const char* description);
    bool SetTexture(const int id, int width, int height, void* ptr);
    bool SetAudioInfo(const int id, const int channels, const int sampleRate, const int maxBufferLength);
    bool Play(const int id);

    int GetAudioBufferLength(const int id);
    bool GetAudioBuffer(const int id, float* buffer, const int bufferLength);
   
    bool Finalize();

    bool IsErrorDetected(const int id);
    bool CheckVideoAndSetFalse(const int id);
    bool CheckAudioAndSetFalse(const int id);

    bool ShowAllGstElements();

    using debug_log_func_type = void(*)(const char*);
    void set_debug_log_func(debug_log_func_type func);

    void log_func(const char* str)
    {
        std::cout << str << std::endl;
    }
}

#include <thread>

void func2()
{
    Initialize();

    //auto id = AddPipeline(
    //    "videotestsrc is-live=true num-buffers=300 ! video/x-raw,width=854,height=480 ! appsink name=appVideoSink emit-signals=true !"
    //    "audiotestsrc is-live=true num-buffers=300 ! audio/x-raw,channels=2,format=F32LE ! appsink name=appAudioSink emit-signals=true"
    //);

    //auto id = AddPipeline(
    //    "videotestsrc is-live=true ! video/x-raw,width=854,height=480 ! appsink name=appVideoSink emit-signals=true !"
    //    "audiotestsrc is-live=true ! audio/x-raw,channels=2,format=F32LE ! appsink name=appAudioSink emit-signals=true"
    //);


    auto id = AddPipeline(
        "uridecodebin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm ! videoconvert ! appsink name=appVideoSink emit-signals=true !"
        "uridecodebin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm ! audioconvert ! appsink name=appAudioSink emit-signals=true"
    );

    //I420
    SetTexture(id, 854, 480 * 3 / 2, nullptr);

    //F32LE
    //SetAudioInfo(id, 2, 44100, 44100);
    SetAudioInfo(id, 2, 48000, 48000);

    //ログ有効
    set_debug_log_func(log_func);


    Play(id);

    auto start = std::chrono::system_clock::now();

    int totalBuffer = 0;

    std::thread t([&]
    {

        //for(auto i = 0; i < 30; i++)
        for(;;)
        {
            auto check = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
            if(check > 20000) break;

            auto length = GetAudioBufferLength(id);
            if(length > 0)
            {
                auto temp = new float[length];

                auto sub1 = std::chrono::system_clock::now();
                auto result = GetAudioBuffer(id, temp, length);
                auto sub2 = std::chrono::system_clock::now();

                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
                //auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(sub2 - sub1).count();

                //std::cout << "buffer: " << result << ": " << length << "          " << elapsed << " us" << std::endl;
                totalBuffer += length;


                //std::cout << "data ";

                //for(auto i = 0; i < length; i++)
                //{
                //    std::cout << temp[i] << " ";
                //}
                //std::cout << std::endl;

                delete[] temp;
            }

            auto errorDetected = IsErrorDetected(id);
            if(errorDetected)
            {
                std::cout << "error detected!!" << std::endl;
            }


            std::cout << "video sample!: " << CheckVideoAndSetFalse(id) << std::endl;
            std::cout << "audio sample!: " << CheckAudioAndSetFalse(id) << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(17));
        }
    });


    t.join();

    Finalize();

    std::cout << "finished!: " << totalBuffer << std::endl;

}

#include <vector>


void func3()
{

    std::chrono::system_clock::time_point  start, end; // 型は auto で可
    start = std::chrono::system_clock::now(); // 計測開始時間
    {
        // 処理
        std::list<float> myList;

        auto p = new float[10000];

        myList.insert(myList.end(), p, p + 10000);

        delete[] p;

        myList.erase(myList.begin(), std::next(myList.begin(), 5000));

        myList.clear();
    }
    end = std::chrono::system_clock::now();  // 計測終了時間
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(); //処理に要した時間をミリ秒に変換



    std::cout << elapsed << " ms" << std::endl;
}

void func4()
{
    //ログ有効
    set_debug_log_func(log_func);

    std::cout << "Initialized: " << Initialize() << std::endl;
    ShowAllGstElements();
}


int main()
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif

    std::cout << "launch the following command in anothor window" << std::endl;
    std::cout << "gst-launch-1.0.exe -v videotestsrc ! autovideosink" << std::endl;

    //char* argv[] = { (char*)"GStreamerWrapperTest" };
    //func (1, (char**)&argv);

    func2();
    //func2();
    //func2();
    //func2();

    //func3();

    //func4();

    return 0;
}

