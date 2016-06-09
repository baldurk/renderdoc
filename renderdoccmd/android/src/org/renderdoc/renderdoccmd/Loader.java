package org.renderdoc.renderdoccmd;
import android.app.Activity;

public class Loader extends android.app.NativeActivity
{
    /* load our native library */
    static {
        System.loadLibrary("renderdoc");
    }
}
