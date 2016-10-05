adb shell am force-stop org.renderdoc.renderdoccmd
adb forward tcp:39970 tcp:39920
adb forward tcp:38970 tcp:38920
adb shell setprop debug.vulkan.layers \"\"
adb shell pm grant org.renderdoc.renderdoccmd android.permission.READ_EXTERNAL_STORAGE
adb shell am start -n org.renderdoc.renderdoccmd/.Loader -e renderdoccmd remoteserver
