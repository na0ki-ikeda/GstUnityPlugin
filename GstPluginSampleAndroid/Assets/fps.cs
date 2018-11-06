using UnityEngine;
using UnityEngine.UI;

public class fps : MonoBehaviour
{

    int counter_ = 0;

    float lastUpdate_ = 0;

    // Use this for initialization
    void Start()
    {

        lastUpdate_ = Time.time;
    }

    // Update is called once per frame
    void Update()
    {
        counter_++;


        if (lastUpdate_ + 1.0f < Time.time)
        {
            GetComponent<Text>().text = "FPS: " + counter_;
            counter_ = 0;
            lastUpdate_ = Time.time;
        }

    }
}
