This folder can be extracted out and placed into your source tree if you want to use RenderDoc.

* If you want to access RenderDoc while it's injected into your program, the app/ folder is what you want. This also contains functions for injecting RenderDoc into existing or new processes.
* If you want to write a program that utilises RenderDoc's replay and analysis capabilities (e.g. writing a new UI, or an auto-testing/offline analysis tool), the replay/ folder is what you want.

You will need both folders if you want to launch processes with RenderDoc injected.
