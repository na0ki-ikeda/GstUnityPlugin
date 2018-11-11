using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace GstPluginAndroid
{
    [RequireComponent(typeof(Renderer))]
    [RequireComponent(typeof(AudioSource))]
    public class GStreamerPipelineController : MonoBehaviour
    {
        int pipelineId_;
        Texture2D texture_;
        AudioClip clip_;
        int clipPosition_ = 0;
        float audioDeadLineTime_ = 0;

#if UNITY_EDITOR || UNITY_STANDALONE_WIN
#else
        Boolean textureUpdated_ = false;
#endif

        [SerializeField]
        int width_ = 854;

        [SerializeField]
        int height_ = 480;

        [SerializeField]
        int frequency_ = 44100;

        [SerializeField]
        int channels_ = 2;

        // Use this for initialization
        void Start()
        {
            var actualWidth = width_;
            var actualHeight = height_ * 3 / 2;//for I420 YUV

            //テクスチャの作成
            texture_ = new Texture2D(actualWidth, actualHeight, TextureFormat.Alpha8, false);

            //マテリアルの設定
            GetComponent<Renderer>().material.mainTexture = texture_;
            GetComponent<Renderer>().material.SetFloat("_TextureHeight", actualHeight);

            //クリップの作成
            clip_ = AudioClip.Create("clip", frequency_, channels_, frequency_, false);
            GetComponent<AudioSource>().clip = clip_;

            //パイプライン作成
            //"uridecodebin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm ! queue ! videoconvert ! appsink name=appVideoSink emit-signals=true !" +
            //"uridecodebin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm ! queue ! audioconvert ! appsink name=appAudioSink emit-signals=true"
            var video = string.Format("videotestsrc is-live=true ! video/x-raw,width={0},height={1} ! appsink name=appVideoSink emit-signals=true !", width_, height_);
            var audio = string.Format("audiotestsrc is-live=true ! audio/x-raw,channels={0},format=F32LE ! appsink name=appAudioSink emit-signals=true", channels_);

            //パイプライン作成
            pipelineId_ = GStreamerWrapper.AddPipeline(video + audio);

#if UNITY_EDITOR || UNITY_STANDALONE_WIN
            //テクスチャのセット
            GStreamerWrapper.SetTexture(pipelineId_, actualWidth, actualHeight, texture_.GetNativeTexturePtr());
#else
            //ビデオ情報
            GStreamerWrapper.SetVideoInfo(pipelineId_, actualWidth, actualHeight);
#endif

            //オーディオ情報
            GStreamerWrapper.SetAudioInfo(pipelineId_, channels_, frequency_, frequency_);

            //再生
            GStreamerWrapper.Play(pipelineId_);
        }

        void UpdateAudio()
        {
            //check deadline
            if (GetComponent<AudioSource>().isPlaying && Time.time > audioDeadLineTime_)
            {
                GetComponent<AudioSource>().Stop();
                clipPosition_ = 0;

                Debug.Log("GStreamerPipelineController deadline detected!                        " + Time.time);
            }

            //check length 
            var length = GStreamerWrapper.GetAudioBufferLength(pipelineId_);
            if (length <= 0) return;

            //copy to buffer
            var buffer = new float[length];
            IntPtr ptr = Marshal.AllocHGlobal(length * sizeof(float));
            {
                GStreamerWrapper.GetAudioBuffer(pipelineId_, ptr, length);
                Marshal.Copy(ptr, buffer, 0, length);
            }
            Marshal.FreeHGlobal(ptr);

            //[-1, 1] => [0, 1]
            for (var i = 0; i < buffer.Length; i++)
            {
                buffer[i] = (buffer[i] * 1.0f + 1) / 2;
            }

            //count samples
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
#if UNITY_EDITOR || UNITY_STANDALONE_WIN
#else
            //this code is for gl texture workaround (GL_ALPHA8 can not be created by unity in Android platform so GL_ALPHA8 texture will be generated in native code)
            if (!textureUpdated_)
            {
                var externalTexturePtr = GStreamerWrapper.GetTexturePtr(pipelineId_);
                //Debug.Log("TextureID!: " + (int)externalTexturePtr);

                //0 check
                if ((int)externalTexturePtr != 0)
                {
                    var externalTexture = Texture2D.CreateExternalTexture(texture_.width, texture_.height, TextureFormat.Alpha8, false, false, externalTexturePtr);
                    texture_.UpdateExternalTexture(externalTexture.GetNativeTexturePtr());
                    textureUpdated_ = true;
                }
            }
#endif
        }


        // Update is called once per frame
        void Update()
        {
            //get audio buffer and apply to AudioClip
            UpdateAudio();

            //for android
            UpdateVideo();
        }
    }
}
