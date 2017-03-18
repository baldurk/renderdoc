In-application API
==================

Reference for RenderDoc in-application API version 1.1.1

Make sure to use a matching API header for your build - if you use a newer header, the API version may not be available. All RenderDoc builds supporting this API ship the header in their root directory.

This page describes the RenderDoc API exposed to applications being captured, both in overall organisation as well as a specific reference on each function.

To begin using the API you need to fetch the ``RENDERDOC_GetAPI`` function. You should do this dynamically, it is not recommended to actually link against RenderDoc's DLL as it's intended to be injected or loaded at runtime. The header does not declare ``RENDERDOC_GetAPI``, it declares a function pointer typedef ``pRENDERDOC_GetAPI`` that you can use.

The two common ways to integrate RenderDoc are either to passively check if the DLL is loaded, and use the API. This lets you continue to use RenderDoc entirely as normal, launching your program through the UI, but you can access additional functionality to e.g. trigger captures at custom times.

To do this you'll use your platforms dynamic library functions to see if the library is open already - e.g. ``GetModuleHandle`` on Windows, or ``dlopen`` with the ``RTLD_NOLOAD`` flag if available on \*nix systems. Just searching for the module name - ``renderdoc.dll`` or ``librenderdoc.so`` is sufficient here, so you don't need to know the path to where RenderDoc is running from. Then you can use ``GetProcAddress`` or ``dlsym`` to fetch the ``RENDERDOC_GetAPI`` function using the typedef above.

The other way is a closer integration, where your code will explicitly load up RenderDoc. This needs more care taken as it can be a bit more complex. You will need to locate the RenderDoc module yourself, and load it as soon as possible after startup of your program. Due to the nature of RenderDoc's API hooking, the earlier you can load it the better in general. Once you've loaded it you can fetch the  ``RENDERDOC_GetAPI`` entry point as above, and use the API as normal.

.. cpp:function:: int RENDERDOC_GetAPI(RENDERDOC_Version version, void **outAPIPointers)


    This function is the only entry point actually exported from the RenderDoc module. You call this function with the desired API version, and pass it the address of a pointer to the appropriate struct type. If successful, RenderDoc will set the pointer to point to a struct containing the function pointers for the API functions (detailed below) and return 1.

    Note that version numbers follow `semantic versioning <http://semver.org>`_ which means the implementation returned may have a higher minor and/or patch version than requested.
    
    :param RENDERDOC_Version version: is the version number of the API for which you want the interface struct.
    :param void** outAPIPointers: will be filled with the address of the API's function pointer struct, if supported. E.g. if ``eRENDERDOC_API_Version_1_1_1`` is requested, outAPIPointers will be filled with ``RENDERDOC_API_1_1_1*``.
    :return: The function returns 1 if the API version is valid and available, and the struct pointer is filled. The function returns 0 if the API version is invalid or not supported, or the pointer parameter is invalid.


.. cpp:function:: void GetAPIVersion(int *major, int *minor, int *patch)


    This function returns the actual API version of the implementation returned. Version numbers follow `semantic versioning <http://semver.org>`_ which means the implementation returned may have a higher minor and/or patch version than requested: New patch versions are identical and backwards compatible in functionality. New minor versions add new functionality in a backwards compatible way.

    :param int* major: will be filled with the major version of the implementation's version.
    :param int* minor: will be filled with the minor version of the implementation's version.
    :param int* patch: which will be filled with the patch version of the implementation's version.
    :return: None


.. cpp:function:: void SetCaptureOptionU32(RENDERDOC_CaptureOption opt, uint32_t val)

    Set one of the options for tweaking some behaviours of capturing. Note that each option only takes effect from after it is set - so it is advised to set these options as early as possible, ideally before any graphics API has been initialised.

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be set.
    :param uint32_t val: the unsigned integer value to set for the above option.
    :return: The function returns 1 if the option is valid, and the value set on the option is within valid ranges. The function returns 0 if the option is not a :cpp:enum:`RENDERDOC_CaptureOption` enum, or the value is not valid for the option.

.. cpp:function:: void SetCaptureOptionF32(RENDERDOC_CaptureOption opt, float val)

    Set one of the options for tweaking some behaviours of capturing. Note that each option only takes effect from after it is set - so it is advised to set these options as early as possible, ideally before any graphics API has been initialised..

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be set.
    :param float val: the floating point value to set for the above option.
    :return: The function returns 1 if the option is valid, and the value set on the option is within valid ranges. The function returns 0 if the option is not a :cpp:enum:`RENDERDOC_CaptureOption` enum, or the value is not valid for the option.

.. cpp:enum:: RENDERDOC_CaptureOption

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowVSync

    specifies whether the application is allowed to enable vsync. Default is on.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowFullscreen

    specifies whether the application is allowed to enter exclusive fullscreen. Default is on.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_APIValidation

    specifies whether (where possible) API-specific debugging is enabled. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacks

    specifies whether each API call should save a callstack. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacksOnlyDraws

    specifies whether - if `CaptureCallstacks` is enabled - callstacks are only saved on drawcalls. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_DelayForDebugger

    specifies a delay in seconds after launching a process to pause, to allow debuggers to attach. Default is 0.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_VerifyMapWrites

    specifies whether any 'map' type resource memory updates should be bounds-checked for overruns. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_HookIntoChildren

    specifies whether child processes launched by the initial application should be hooked as well - commonly if a launcher process is needed to run the application. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_RefAllResources

    specifies whether all live resources at the time of capture should be included in the log, even if they are not referenced by the frame. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_SaveAllInitials

    specifies whether all initial states of resources at the start of the frame should be saved, rather than omitting large resource contents which are detected to be likely unused. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureAllCmdLists

    specifies whether all command lists should be captured on APIs where multithreaded submission is not optimal such as D3D11, rather than only capturing those submitted during the frame. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_DebugOutputMute

    specifies whether to mute any API debug output messages when `APIValidation` is enabled. Default is on.


.. cpp:function:: uint32_t GetCaptureOptionU32(RENDERDOC_CaptureOption opt)

    Gets the current value of one of the different options listed above in :cpp:func:`SetCaptureOptionU32`.

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be retrieved.
    :return: The function returns the value of the capture option, if the option is a valid :cpp:enum:`RENDERDOC_CaptureOption` enum. Otherwise returns ``0xffffffff``.

.. cpp:function:: float GetCaptureOptionF32(RENDERDOC_CaptureOption opt)

    Gets the current value of one of the different options listed above in :cpp:func:`SetCaptureOptionF32`.

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be retrieved.
    :return: The function returns the value of the capture option, if the option is a valid :cpp:enum:`RENDERDOC_CaptureOption` enum. Otherwise returns `-FLT_MAX`.

.. cpp:function:: void SetFocusToggleKeys(RENDERDOC_InputButton *keys, int num)

    This function changes the key bindings in-application for changing the focussed window.

    :param RENDERDOC_InputButton* keys: lists the keys to bind. If this parameter is NULL, ``num`` must be 0.
    :param int num: specifies the number of keys in the ``keys`` array. If 0, the keybinding is disabled.

.. cpp:enum:: RENDERDOC_InputButton

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_0

    ``eRENDERDOC_Key_0`` to ``eRENDERDOC_Key_9`` are the number keys. The values of these match ASCII for '0' .. '9'.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_A

    ``eRENDERDOC_Key_A`` to ``eRENDERDOC_Key_Z`` are the letter keys. The values of these match ASCII for 'A' .. 'Z'.


.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Divide

    is the Divide key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Multiply

    is the Multiply key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Subtract

    is the Subtract key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Plus

    is the Plus key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_F1

    ``eRENDERDOC_Key_F1`` to ``eRENDERDOC_Key_F12`` are the function keys.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Home

    is the Home key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_End

    is the End key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Insert

    is the Insert key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Delete

    is the Delete key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_PageUp

    is the PageUp key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_PageDn

    is the PageDn key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Backspace

    is the Backspace key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Tab

    is the Tab key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_PrtScrn

    is the PrtScrn key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Pause

    is the Pause key.

.. cpp:function:: void SetCaptureKeys(RENDERDOC_InputButton *keys, int num)

    This function changes the key bindings in-application for triggering a capture on the current window.

    :param RENDERDOC_InputButton* keys: lists the keys to bind. If this parameter is NULL, ``num`` must be 0.
    :param int num: specifies the number of keys in the ``keys`` array. If 0, the keybinding is disabled.

.. cpp:function:: uint32_t GetOverlayBits()

    This function returns the current mask which determines what sections of the overlay render on each window.

    :return: A mask containing bits from :cpp:enum:`RENDERDOC_OverlayBits`.

.. cpp:enum:: RENDERDOC_OverlayBits

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Enabled

    is an overall enable/disable bit. If this is disabled, no overlay renders.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameRate

    shows the average, min and max frame time in milliseconds, and the average framerate.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameNumber

    shows the current frame number, as counted by the number of presents.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_CaptureList

    shows how many total captures have been made, and a list of captured frames in the last few seconds.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Default

    is the default set of bits, which is the value of the mask at startup.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_All

    is equal to `~0U` so all bits are enabled.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_None

    is equal to `0` so all bits are disabled.

.. cpp:function:: void MaskOverlayBits(uint32_t And, uint32_t Or)

    This function modifies the current mask which determines what sections of the overlay render on each window.

    :param uint32_t And: is a 32-bit value the mask is binary-AND'd with before processing ``Or``.
    :param uint32_t Or: is a 32-bit value the mask is binary-OR'd with after processing ``And``.

.. cpp:function:: void Shutdown()

    This function will attempt to shut down and remove RenderDoc and its hooks from the target process. It must be called as early as possible in the process, and will have undefined results if any graphics API functions have been called.

.. cpp:function:: void UnloadCrashHandler()

    This function will remove RenderDoc's crash handler from the target process. If you have your own crash handler that you want to handle any exceptions, RenderDoc's handler could interfere so it can be disabled.

.. cpp:function:: void SetLogFilePathTemplate(const char *pathtemplate)

    Set the template for new captures. The template can either be a relative or absolute path, which determines where captures will be saved and how they will be named. If the path template is ``my_captures/example`` then captures saved will be e.g. ``my_captures/example_frame123.rdc`` and ``my_captures/example_frame456.rdc``. Relative paths will be saved relative to the process's current working directory. The default template is in a folder controlled by the UI - initially the system temporary folder, and the filename is the executable's filename.

    :param const char* pathtemplate: specifies the capture path template to set, as UTF-8 null-terminated string.

.. cpp:function:: const char *GetLogFilePathTemplate()

    Get the current log file path template.

    :return: the current capture path template as a UTF-8 null-terminated string.

.. cpp:function:: uint32_t GetNumCaptures()

    This function returns the number of frame captures that have been made.
    :return: Returns the number of frame captures that have been made

.. cpp:function:: uint32_t GetCapture(uint32_t idx, char *logfile, uint32_t *pathlength, uint64_t *timestamp)

    This function returns the details of a particular frame capture, as specified by an index from 0 to :cpp:func:`GetNumCaptures` - 1.

    :param uint32_t idx: specifies which capture to return the details of. Must be less than the return value of:cpp:func:`GetNumCaptures`.
    :param char* logfile: is an optional parameter filled with the UTF-8 null-terminated path to the file. There must be enough space in the array to contain all characters including the null terminator. If set to NULL, nothing is written.
    :param uint32_t* pathlength: is an optional parameter filled with the byte length of the above `logfile` including the null-terminator. If set to NULL, nothing is written.
    :param uint64_t* timestamp: is an optional parameter filled with the 64-bit timestamp of the file - equivalent to the `time()` system call. If set to NULL, nothing is written.
    :return: Returns ``1`` if the capture index was valid, or ``0`` if it was out of range.

.. note::

    It is advised to call this function twice - first to obtain ``pathlength`` so that sufficient space can be allocated. Then again to actually retrieve the path.


The path follows the template set in :cpp:func:`SetLogFilePathTemplate` so it may not be an absolute path.

.. cpp:function:: void TriggerCapture()

    This function will trigger a capture as if the user had pressed one of the capture hotkeys. The capture will be taken from the next frame presented to whichever window is considered current.

.. cpp:function:: uint32_t IsTargetControlConnected()

    This function returns a value to indicate whether the RenderDoc UI is currently connected to the current process.

    :return: Returns ``1`` if the RenderDoc UI is currently connected, or ``0`` otherwise.

.. cpp:function:: uint32_t LaunchReplayUI(uint32_t connectTargetControl, const char *cmdline)

    This function will determine the closest matching replay UI executable for the current RenderDoc module and launch it.

    :param uint32_t connectTargetControl should be set to 1 if the UI should immediately connect to the application.
    :param const char* cmdline: is an optional UTF-8 null-terminated string to be appended to the command line, e.g. a capture filename. If this parameter is NULL, the command line will be unmodified.
    :return: If the UI was successfully launched, this function will return the PID of the new process. Otherwise it will return ``0``.

.. cpp:function:: void SetActiveWindow(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function will explicitly set which window is considered active. The active window is the one that will be captured when the keybind to trigger a capture is pressed.

    :param RENDERDOC_DevicePointer device: is a handle to the API 'device' object that will be set active. Must be valid.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform window handle that will be set active. Must be valid.

.. note::

    ``RENDERDOC_DevicePointer`` is a typedef to ``void *``. The contents of it are API specific:

    * For D3D11 it must be the ``ID3D11Device`` device object.
    * For OpenGL it must be the ``HGLRC`` or ``GLXContext`` context object.
    * For Vulkan it must be the dispatch table pointer within the ``VkInstance``. This is a pointer-sized value at the location pointed to by the ``VkInstance``. NOTE - this is not the actual ``VkInstance`` pointer itself.

    ``RENDERDOC_WindowHandle`` is a typedef to ``void *``. It is the platform specific ``HWND``, ``xcb_window_t``, or Xlib ``Window``.

.. cpp:function:: void StartFrameCapture(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function will immediately begin a capture for the specified device/window combination.

    :param RENDERDOC_DevicePointer device: is a handle to the API 'device' object that will be set active. May be NULL to wildcard match.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform window handle that will be set active. May be NULL to wildcard match.

.. note::

    ``RENDERDOC_DevicePointer`` and ``RENDERDOC_WindowHandle`` are described above in :cpp:func:`SetActiveWindow`.
    ``device`` and ``wndHandle`` can either or both be set to NULL to wildcard match against active device/window combinations. This wildcard matching can be used if the handle is difficult to obtain where frame captures are triggered.

    For example if ``device`` is NULL but ``wndHandle`` is set, RenderDoc will begin a capture on the first API it finds that is active on that window.

    If the wildcard match has multiple possible candidates, it is not defined which will be chosen. Wildcard matching should only be used when e.g. it is known that only one API is active on a window, or there is only one window active for a given API.

    If no window has been created and all rendering is off-screen, NULL can be specified for the window handle and the device object can be passed to select that API. If both are set to NULL, RenderDoc will simply choose one at random so is only recommended for the case where only one is present.

.. cpp:function:: uint32_t IsFrameCapturing()

    This function returns a value to indicate whether the current frame is capturing.

    :return: Returns ``1`` if the frame is currently capturing, or ``0`` otherwise.

.. cpp:function:: void EndFrameCapture(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function will immediately end an active capture for the specified device/window combination.

    :param RENDERDOC_DevicePointer device: is a handle to the API 'device' object that will be set active. May be NULL to wildcard match.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform window handle that will be set active. May be NULL to wildcard match.

.. note::

    ``RENDERDOC_DevicePointer`` and ``RENDERDOC_WindowHandle`` are described above in :cpp:func:`SetActiveWindow`.
    ``device`` and ``wndHandle`` can either or both be set to NULL to wildcard match against active device/window combinations. This wildcard matching can be used if the handle is difficult to obtain where frame captures are triggered.

    Wildcard matching of `device` and `wndHandle` is described above in :cpp:func:`BeginFrameCapture`.

    There will be undefined results if there is not an active frame capture for the device/window combination.

.. cpp:function:: void TriggerMultiFrameCapture(uint32_t numFrames)

    This function will trigger multiple sequential frame captures as if the user had pressed one of the capture hotkeys before each frame. The captures will be taken from the next frames presented to whichever window is considered current.

    :param uint32_t numFrames: the number of frames to capture, as an unsigned integer.
