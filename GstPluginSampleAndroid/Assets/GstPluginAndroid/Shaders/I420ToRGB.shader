//this file is originally from mrayGStreamerUnity
Shader "Custom/I420ToRGB" {
    Properties {
        _MainTex ("Base (RGB)", 2D) = "white" {}
        _TextureHeight ("Texture Height", Float) = 0
    }
    SubShader {
        Pass{
            //ZTest Always 
            //Cull Off 
            //ZWrite Off
            Fog { Mode off }
            
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment frag

            #include "UnityCG.cginc"

            sampler2D _MainTex;
            float _TextureHeight;

            // YUV offset (reciprocals of 255 based offsets above)
            // half3 offset = half3(-0.0625, -0.5, -0.5);
            // RGB coefficients 
            // half3 rCoeff = half3(1.164,  0.000,  1.596);
            // half3 gCoeff = half3(1.164, -0.391, -0.813);
            // half3 bCoeff = half3(1.164,  2.018,  0.000);


            float4 frag(v2f_img IN) :COLOR{
                half3 yuv, rgb;

                float2 uv = IN.uv.xy;

                //flip Y axis
                uv.y = 1 - uv.y;

                //compress Y axis
                uv.y = uv.y * 0.666667;//0.66.. = 2/3            

                // lookup Y
                yuv.r = tex2D(_MainTex, uv).a;
                //yuv.r = pow(yuv.r, 2.2);//yuv gamma

                // co-ordinate conversion algorithm for i420:
                //    x /= 2.0; if modulo2(y) then x += width/2.0;
                uv.x *= 0.5;
/*
                //TODO: buffer debug
                if(fmod(uv.y * _TextureHeight, 2.0) >= 1.0)
                {
                    //uv.x += 0.5;
                    //yuv.r = 1;
                }
*/
                // lookup U
                uv.y = 0.666667 + (uv.y * 0.25);//0.66.. = 2/3, 0.25 = 1/4
                yuv.g = tex2D(_MainTex, uv.xy).a;

                // lookup V
                uv.y += 0.166667;//0.166.. = 0.666667 * 0.25, 0.66.. = 2/3, 0.25 = 1/4
                yuv.b = tex2D(_MainTex, uv.xy).a;

                // Convert
                yuv += half3(-0.0625, -0.5, -0.5);
                rgb.r = dot(yuv, half3(1.164,  0.000,  1.596));//rCoeff);
                rgb.g = dot(yuv, half3(1.164, -0.391, -0.813));//gCoeff);
                rgb.b = dot(yuv, half3(1.164,  2.018,  0.000));//bCoeff);

                //rgb gamma
                rgb = pow(rgb, 2.2);

                return float4(rgb,1);                
            }

            ENDCG
        }
    } 
}
