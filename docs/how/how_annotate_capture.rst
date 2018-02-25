How do I annotate a capture?
============================

RenderDoc allows some annotation of capture files, meaning that you can make notes and bookmark important events, then save your changes within the capture itself for sharing with other people. This can be useful for example when investigating a bug or repro case and passing on your findings natively to someone else, instead of having to include additional text like 'texture 148 is the buggy texture'.

All of these modifications can be saved with a capture. Pressing :kbd:`Ctrl-S` or :guilabel:`File` → :guilabel:`Save` will save the capture with any changes that have been made to it in the UI. If you haven't already saved a temporary capture, or the capture is on a remote context, this will need to you save it to a local path.

Bookmarks
---------

.. |asterisk_orange| image:: ../imgs/icons/asterisk_orange.png

The event browser allows you to make bookmarks on events of particular interest. This allows quick navigation of a frame or jumping back and forth between two events that may be quite separated.

The |asterisk_orange| bookmark button will allow you to bookmark an event, the shortcut key is :kbd:`Ctrl-B`. Once you have several bookmarks, you can jump between them by pressing the :kbd:`Ctrl-1` to :kbd:`Ctrl-0` shortcuts from anywhere in the UI, without any need to focus the event browser.

.. figure:: ../imgs/Screenshots/BookmarksBar.png

	Bookmarks bar: The bookmarks bar with several EIDs bookmarks.

When loading any capture with saved bookmarks they will be automatically populated into the UI. This will allow you to highlight particular problematic events and anyone opening the capture will be able to use the shortcuts above to jump immediately to where the problem is.

Resource Renaming
-----------------

From within the :doc:`../window/resource_inspector` window, you can rename any resource in the capture. Whether the resource already had a custom-specified name, or if it had a default-generated name, you can provide overrides at any time.

To do so, simply select the resource in question in the resource inspector - either by clicking a link from where it is bound, or searching for it by name or type. Then click on the :guilabel:`Rename Resource` button next to the name, and it will open an editing textbox to let you change the name. When you've set the name, press :kbd:`Enter` or click :guilabel:`Rename Resource` again. To cancel a rename, press :kbd:`Escape` or click :guilabel:`Reset name` to restore the name to its original value.

.. figure:: ../imgs/Screenshots/resource_rename.png

	Resource Inspector: Renaming a resource in a capture.

As with bookmarks, these renames can be saved with a capture and are automatically used when loading the capture subsequently. This can be useful to point the way to which resources are causing problems, or specifically how a given resource with a more general name is being used in this particular capture.

Capture Comments
----------------

In the :doc:`../window/capture_comments` window there is a simple text field allowing you to store any arbitrary text you want within the capture. This could be notes on the environment or build version that was stored.

By default, any capture that is newly opened that contains comments will show those comments first and foremost when opening. This behaviour can be disabled in the :doc:`../window/settings_window`.
