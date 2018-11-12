using System.Collections;
using UnityEngine;

namespace GstPluginAndroid
{
    //TODO: to be singleton class
    public class GStreamerManager : MonoBehaviour
    {
        [SerializeField]
        bool enableDebugLog_ = false;

        void Awake()
        {
#if UNITY_EDITOR || UNITY_STANDALONE_WIN
#else
            using (AndroidJavaClass gstreamer = new AndroidJavaClass("org.freedesktop.gstreamer.GStreamer"))
            {
                AndroidJavaClass unityPlayer = new AndroidJavaClass("com.unity3d.player.UnityPlayer");
                AndroidJavaObject context = unityPlayer.GetStatic<AndroidJavaObject>("currentActivity").Call<AndroidJavaObject>("getApplicationContext");
                gstreamer.CallStatic("init", context);
            }
#endif

            //Initialize debug log
            if (enableDebugLog_)
            {
                DebugLog.Initialize();
            }

            //Initialize Gstreamer wrapper
            GStreamerWrapper.Initialize();

            //Show all elements
            //GStreamerWrapper.ShowAllGstElements();

            //start native render loop
            StartCoroutine(OnRender());
        }

        IEnumerator OnRender()
        {
            for (; ; )
            {
                //Support multithread rendering
                yield return new WaitForEndOfFrame();
                GL.IssuePluginEvent(GStreamerWrapper.GetRenderEventFunc(), 0);
            }
        }

        void OnDestroy()
        {
            //Close Gstramer wrapper
            GStreamerWrapper.Finalize();
        }
    }
}
