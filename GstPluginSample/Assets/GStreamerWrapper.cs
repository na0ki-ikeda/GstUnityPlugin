using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;

public class GStreamerWrapper : MonoBehaviour
{
#if UNITY_EDITOR || UNITY_STANDALONE_WIN
    [DllImport("GStreamerWrapper")]
    static extern int GetNumber();

    [DllImport("GStreamerWrapper")]
    [return: MarshalAs(UnmanagedType.U1)]
    static extern bool Initialize();

    [DllImport("GStreamerWrapper")]
    static extern int AddPipeline(string description);

    [DllImport("GStreamerWrapper")]
    [return: MarshalAs(UnmanagedType.U1)]
    static extern bool SetTexture(int id, int width, int height, IntPtr ptr);

    [DllImport("GStreamerWrapper")]
    [return: MarshalAs(UnmanagedType.U1)]
    static extern bool Play(int id);

    [DllImport("GStreamerWrapper")]
    [return: MarshalAs(UnmanagedType.U1)]
    static extern bool Finalize();

    [DllImport("GStreamerWrapper")]
    public static extern IntPtr GetRenderEventFunc();

    [DllImport("GStreamerWrapper")]
    [return: MarshalAs(UnmanagedType.U1)]
    static extern bool SetAudioInfo(int id, int channels, int maxBufferLength);

    [DllImport("GStreamerWrapper")]
    static extern int GetAudioBufferLength(int id);

    [DllImport("GStreamerWrapper")]
    [return: MarshalAs(UnmanagedType.U1)]
    static extern bool GetAudioBuffer(int id, IntPtr buffer, int bufferLength);

#endif

    [SerializeField]
    int pipelineId_;

    [SerializeField]
    Texture2D texture_;

    [SerializeField]
    AudioClip clip_;

    [SerializeField]
    int clipPosition_ = 0;

    [SerializeField]
    float audioDeadLineTime_ = 0;

    // Use this for initialization
    void Start()
    {
        DebugLog.Initialize();

        var width = 854;
        var height = 480;

        var actualWidth = width;
        var actualHeight = height * 3 / 2;//for I420 YUV

        //テクスチャの作成
        texture_ = new Texture2D(actualWidth, actualHeight, TextureFormat.Alpha8, false);

        //設定
        GetComponent<Renderer>().material.mainTexture = texture_;
        GetComponent<Renderer>().material.SetFloat("_TextureHeight", actualHeight);

        //クリップの作成
        clip_ = AudioClip.Create("clip", 44100, 2, 44100, false);
        GetComponent<AudioSource>().clip = clip_;

        //Gst初期化
        var initialized = Initialize();

        //パイプライン作成
        pipelineId_ = AddPipeline(
        "videotestsrc is-live=true num-buffers=300 ! video/x-raw,width=854,height=480 ! appsink name=appVideoSink emit-signals=true !" +
        "audiotestsrc is-live=true num-buffers=300 ! audio/x-raw,channels=2,format=F32LE ! appsink name=appAudioSink emit-signals=true"

        //"uridecodebin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm ! queue ! videoconvert ! appsink name=appVideoSink emit-signals=true !" +
        //"uridecodebin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm ! queue ! audioconvert ! appsink name=appAudioSink emit-signals=true"

        );

        //テクスチャのセット
        SetTexture(pipelineId_, actualWidth, actualHeight, texture_.GetNativeTexturePtr());

        //オーディオ情報
        SetAudioInfo(pipelineId_, 2, 44100);

        //再生
        Play(pipelineId_);

        //StartCoroutine(OnRender());
    }

    void UpdateAudio()
    {
        //check deadline
        if (GetComponent<AudioSource>().isPlaying && Time.time > audioDeadLineTime_)
        {
            GetComponent<AudioSource>().Stop();
            clipPosition_ = 0;

            Debug.Log("deadline detected!                        " + Time.time);
        }

        //check length 
        var length = GetAudioBufferLength(pipelineId_);
        if (length <= 0) return;

        //copy to buffer
        var buffer = new float[length];
        IntPtr ptr = Marshal.AllocHGlobal(length * sizeof(float));
        {
            GetAudioBuffer(pipelineId_, ptr, length);
            Marshal.Copy(ptr, buffer, 0, length);
        }
        Marshal.FreeHGlobal(ptr);

        //[-1, 1] => [0, 1]
        for (var i = 0; i < buffer.Length; i++)
        {
            buffer[i] = (buffer[i] * 1.0f + 1) / 2;
        }

        //Debug.Log("new buffer length: " + length + "                           " + Time.time);


        var newSamples = length / clip_.channels;
        if (clipPosition_ + newSamples <= clip_.samples)
        {
            //copy directly
            clip_.SetData(buffer, clipPosition_);
            clipPosition_ = (clipPosition_ + newSamples) % clip_.samples;
        }
        else
        {
            //separate into head(remained) and tail(starting from 0) buffer
            var headLength = (clip_.samples - clipPosition_) * clip_.channels;
            var tailLength = (clipPosition_ + newSamples - clip_.samples) * clip_.channels;

            var head = new float[headLength];
            var tail = new float[tailLength];
            Array.Copy(buffer, 0, head, 0, headLength);
            Array.Copy(buffer, headLength, tail, 0, tailLength);
            clip_.SetData(head, clipPosition_);
            clip_.SetData(tail, 0);

            clipPosition_ = clipPosition_ + newSamples - clip_.samples;
        }


        //play if not playing
        if (!GetComponent<AudioSource>().isPlaying)
        {
            // 10% buffer required
            if (clipPosition_ / (float)clip_.samples > 0.1f)
            {
                //record deadline
                audioDeadLineTime_ = Time.time + clipPosition_ / (float)clip_.frequency;
                GetComponent<AudioSource>().Play();
            }
        }
        else
        {
            //update deadline
            audioDeadLineTime_ += newSamples / (float)clip_.frequency;
        }
    }

    void UpdateVideo()
    {
        //ざる実装
        GL.IssuePluginEvent(GetRenderEventFunc(), 0);
    }


    // Update is called once per frame
    void Update()
    {
        //get audio buffer and apply to AudioClip
        UpdateAudio();

        //post as GL event
        UpdateVideo();
    }

    IEnumerator OnRender()
    {
        for (; ; )
        {
            yield return new WaitForEndOfFrame();
            GL.IssuePluginEvent(GetRenderEventFunc(), 0);
        }
    }


    void OnDestroy()
    {
        Finalize();
    }
}
