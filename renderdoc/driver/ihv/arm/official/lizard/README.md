## Lizard

Lizard is a library to capture ARM hardware counters on Android devices.

### Using the library with gatord

This step is optional, the library can query a few hardware counters directly.

1. Build `gatord` for Android.

2. Copy `gatord` to a directory which is publicly readable, writeable and executable on the Android device (e.g. /data/local/tmp)

```sh
$ adb push gatord /data/local/tmp/gatord
```

3. Start gatord on the Android device ( -M: Mali device type e.g. G71, G76, etc.)

```sh
$ adb shell ./data/local/tmp/gatord -M G71
```
