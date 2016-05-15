Capture Connection
==================

.. toctree::


The Live capture window opens up when you launch a capture of a program, as well as when you attach to an existing program.

Attaching to an existing instance
---------------------------------

After you've launched a program through RenderDoc and its hooks are added you can freely disconnect (by closing the live capture window) or close the main UI. You can then connect to this again later, either from the same computer or another computer connecting over the network.

To connect to an existing hooked program, select Attach to Running Instance from the File menu. This opens up the attach window that allows you to select a remote host to connect to. By default localhost is already in the list, but you can add and remove other hosts.

.. warning::

	Please note that none of the connections RenderDoc makes or uses are encrypted or protected, so if this is a concern you should look into securing the connections by hand.

.. figure:: ../images/AttachInstance.png

	Remote Hosts: Attaching to a running instance either locally or remotely.

When the window opens, when you add a new host or when you click refresh then the hosts will be queried across the network to see if a connection exists. While this is in progress the host will be listed in italics and with a busy icon.

Once a host has been scanned, if any instances are found then that host can be expanded to see the list, and details are listed about each instance. The name is OS-dependent but is usually the executable name. The API name is listed, as well as the username of any user that is already connected.

When you click connect, a live capture window will be opened up - just the same as the window that is automatically opened when you start a program. Any captures that have already been made before you connect will then populate.

.. note::

	If you connect to a running instance, any existing user will be kicked off. Just seeing the instances running on a host will not.


Capture Connection window
-------------------------

When a capture is launched (or attached to) the connection window is opened in the main UI. If you end up only taking one capture and closing the program afterwards the connection window will automatically close - likewise if you take no captures at all. These cases don't need the management options the connection window provides.

In addition to managing captures that have been taken, you can also trigger a capture (optionally with a countdown timer).

.. figure:: ../images/MultipleCaptures.png

	Connection Window: Viewing multiple captures taken in a program.

In this example we have a connection window open to the CascadedShadowMaps11 sample from the DirectX SDK. Two captures have been made and we can see their thumbnails to help distinguish between them. This is visible at any point, regardless of whether you have close the program or not - you can simply switch back to RenderDoc while it's running.

.. note::

	Note, if you have remotely connected you will need to wait while the captures copy across the network to your PC, after which point everything behaves the same as a local capture.

From here you can save these captures out - as currently they are only temporary copies that will be cleaned up on close. You can also manually delete any capture you wish to discard.

Double clicking on any capture will close any current open capture in the RenderDoc UI, and open up that capture for inspection. You may also right click or use the drop-down menu on the open button to launch a new instance of RenderDoc for viewing the log. This is mostly useful if you want to compare two captures side-by-side easily.

.. figure:: ../images/OpenCapNewInstance.png

	New instance: Launch new RenderDoc instance to open this capture.<br />

Child Processes
---------------

RenderDoc is able to automatically inject into any child processes started by the initial process launched from the UI. To do this simply check "Hook into Children" in the :doc:`capture_log_attach`.


RenderDoc has a particular handling of child processes to help you navigate to the process of interest. Whenever a child process is launched, the UI is notified and a list of processes is displayed in a box on the capture connection window. You can double click on any of these entries to open up a new connection to that process, in a new window.


If a process exits, instead of just closing the connection window if there have been no captures, instead RenderDoc looks at the child processes - if there is only one child process, it assume that process must be of interest and immediately switches to tracking that process. If there are *more* than one child process open, the capture connection window will stay open to give you a chance to double click on those child processes to open a new connection window.
