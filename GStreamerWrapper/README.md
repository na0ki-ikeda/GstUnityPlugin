# How to make the project

1. Check GSTREAMER_SDK_ROOT_X64 is defined on environment variable
    - if not, define like below
    - variable name: GSTREAMER_SDK_ROOT_X64
    - variable value: C:\gstreamer\1.0\x86_64
    - .vcxproj files refer GSTREAMER_SDK_ROOT_X64 to resolve gstreamer library files and headers

# note
- you need to confirm GStreamer installed with "complete option"
  - "standard option" doesn't install gst-bad plugins which contains important dlls


