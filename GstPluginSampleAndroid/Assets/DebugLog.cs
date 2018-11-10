using System.Runtime.InteropServices;
using UnityEngine;

public class DebugLog
{
    public delegate void DebugLogDelegate(string str);
    static DebugLogDelegate debugLogFunc = msg => Debug.Log(msg);

    [DllImport("GStreamerWrapper")]
    public static extern void set_debug_log_func(DebugLogDelegate func);
    [DllImport("GStreamerWrapper")]
    public static extern void debug_log_test();

    public static void Initialize()
    {
        set_debug_log_func(debugLogFunc);
        //debug_log_test();
    }
}
