How do I use Renderdoc for Android?
======================================

Renderdoc supports debugging Vulkan applications on Android from host machines.  It is similar to using a network to target a remote machine, as in :doc:`../how/how_network_capture_replay`, but talks over ``adb`` to a device.

Getting started
---------------

Download a release or nightly build for your host OS from the `Builds page <https://renderdoc.org/builds>`_.
Android bits will be in ``android`` directory.

Or, you can build from source for each platform by following the steps on `github <https://github.com/baldurk/renderdoc/blob/master/CONTRIBUTING.md>`_.

Set up your APK for debugging
-----------------------------

In order to capture APIs from a package, the APK needs to be compiled with the following manifest permissions:

.. code::

        <manifest ...
            <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
            <uses-permission android:name="android.permission.INTERNET" />

It also must contain the RenderDoc Vulkan layer.  You must provide the layer for the ABI you are going to target, or the layer will not be loaded, i.e.:

.. code::

        $ jar tf game.apk | grep libVkLayer_RenderDoc
        lib/armeabi-v7a/libVkLayer_RenderDoc.so
        lib/arm64-v8a/libVkLayer_RenderDoc.so
        lib/x86/libVkLayer_RenderDoc.so
        lib/x86_64/libVkLayer_RenderDoc.so
        lib/mips/libVkLayer_RenderDoc.so
        lib/mips64/libVkLayer_RenderDoc.so

You can add the layer to your APK in many ways.  One easy way is using ``aapt``, which can be downloaded using Android Studio.

.. code::

        aapt add game.apk lib/armeabi-v7a/libVkLayer_RenderDoc.so
        aapt add game.apk lib/arm64-v8a/libVkLayer_RenderDoc.so

You may also need to re-sign your APK in order to install it.

.. code::

        mv game.apk game-unaligned.apk
        zip -d game-unaligned.apk META-INF/\*
        jarsigner -verbose -keystore ~/.android/debug.keystore -storepass android -keypass android game-unaligned.apk androiddebugkey
        zipalign 4 game-unaligned.apk game.apk

Set up your Android device
--------------------------

RenderDoc uses an APK on the target in order to capture and replay Vulkan events.  To install it, determine your target architecture and then install both APKs:

.. code::

        adb install -r --abi <target abi> RenderDocCmd.apk
        adb install -r --abi <target abi> game.apk

Steps to use RenderDoc UI for Android
----------------------------------

1. In ``Tools -> Options -> Android``, set the path to your ``adb`` executable.

#. Connect your Android device and look for ``"Allow USB debugging?"`` on its screen.  Click ``OK``

   * You may need to enable Developer Mode for this prompt to appear.
   * Check that your device is listed using ``adb devices``

#. Restart the RenderDoc UI.

#. Your device will be automatically listed in the Remote Hosts menu in the bottom left.  It will appear Offline until the following step.

#. Start the Remote Server using ``Tools -> Start Android Remote Server``

#. Check your device's screen and ``Allow`` RenderDocCmd to access files on your device.

   * This is required to store capture files on ``/sdcard/``

#. Change your current Replay Context to your device using the bottom left menu, which should now show your device as Online.

#. In the capture executable tab, there is a button on the right of ``Executable Path`` that lets you select an installed Android package for capture.

#. Select your package and press the ``Launch`` button in the bottom right of the tab to start the package on the device.

#. If everything went successfully, a new tab will open with a button to Trigger captures.

#. If any of these operations failed, (e.g. you dont have the RenderDocCmd.apk installed), you can see adb command output in ``Help -> View Diagnostic Log File``


Troubleshooting
---------------

1. If you see ``Didn't get proper handshake`` in your log, that may the RenderDoc layer wasn't loaded properly, which is critical for this workflow.  Double check the following:

   - Ensure your APK was packaged with the required permissions.

     * They will be granted automatically by the RenderDoc UI.

   - Ensure you have inserted the layer into the correct location in the APK, i.e. correct ABI.

   - Ensure you installed the correct ABI of both APKs, RenderDocCmd and target.

   - Check the device's screen and ensure RenderDocCmd.apk has been granted permissions.

