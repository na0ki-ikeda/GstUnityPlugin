using UnityEngine;

[RequireComponent (typeof (Camera))]
public class CameraHandler : MonoBehaviour
{

    // Use this for initialization
    void Start()
    {

    }

    // Update is called once per frame
    void Update()
    {

    }

    void OnPostRender()
    {
        //GL.IssuePluginEvent(GStreamerWrapper.GetRenderEventFunc(), 0);
    }

}
