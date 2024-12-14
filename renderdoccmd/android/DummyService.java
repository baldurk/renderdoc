package @RENDERDOC_ANDROID_PACKAGE_NAME@;
import android.app.Notification;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Process;

public class DummyService extends android.app.Service
{
    private Looper serviceLooper;
    private Handler serviceHandler;

    @Override
    public void onCreate() {
        HandlerThread thread = new HandlerThread("DummyServiceThread", Process.THREAD_PRIORITY_BACKGROUND);
        thread.start();

        serviceLooper = thread.getLooper();
        serviceHandler = new Handler(serviceLooper);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Notification notification = new Notification.Builder(this)
            .setPriority(Notification.PRIORITY_LOW)
            .setSmallIcon(R.drawable.icon)
            .setContentTitle("RenderDoc Dummy Foreground Service")
            .setContentText("RenderDoc needs this dummy foreground service to prevent cached app freezer from interrupting the network connection")
            .build();
        startForeground(startId, notification);

        // If we get killed, don't restart
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
    }
}