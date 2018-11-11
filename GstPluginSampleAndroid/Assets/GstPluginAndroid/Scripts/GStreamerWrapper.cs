using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;

namespace GstPluginAndroid
{
    public class GStreamerWrapper
    {
        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool Initialize();

        [DllImport("GStreamerWrapper")]
        public static extern int AddPipeline(string description);

#if UNITY_EDITOR || UNITY_STANDALONE_WIN
        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool SetTexture(int id, int width, int height, IntPtr ptr);
#else
        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool SetVideoInfo(int id, int width, int height);

        [DllImport("GStreamerWrapper")]
        public static extern IntPtr GetTexturePtr(int id);
#endif

        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool Play(int id);

        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool Finalize();

        [DllImport("GStreamerWrapper")]
        public static extern IntPtr GetRenderEventFunc();

        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool SetAudioInfo(int id, int channels, int sampleRate, int maxBufferLength);

        [DllImport("GStreamerWrapper")]
        public static extern int GetAudioBufferLength(int id);

        [DllImport("GStreamerWrapper")]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool GetAudioBuffer(int id, IntPtr buffer, int bufferLength);
    }
}
