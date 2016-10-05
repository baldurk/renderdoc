adb forward tcp:38970 tcp:38920
adb shell setprop debug.vulkan.layers VK_LAYER_RENDERDOC_Capture
adb shell monkey -p %* -c android.intent.category.LAUNCHER 1
timeout 5
adb shell setprop debug.vulkan.layers \"\"
