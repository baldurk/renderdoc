Analytics
=========

RenderDoc has some very minimal analytics gathering. The data is gathered only in the UI and **not during capture**. It is **not personally identifiable** and contains **absolutely no data from captures**.

`The source <https://github.com/baldurk/renderdoc/blob/v1.x/qrenderdoc/Code/Interface/Analytics.h>`_ is freely available and auditable. If desired, a single ``#define RENDERDOC_ANALYTICS_ENABLE`` can be set to ``0`` in that linked file to disable all analytics code.

A report is generated monthly and sent securely to RenderDoc's server. If you want, you can choose to manually approve each report before it's sent.

If you wish to opt-out entirely then no statistics will be gathered or reported. However please consider this carefully as it will make it harder for me to decide which features to prioritise.

To see a complete list of what data is gathered, go to the :doc:`../window/settings_window` in your build and under the :guilabel:`Anonymous Analytics` there will be a link to open a description of the currently gathered data. You can change your mind at any point in the settings window.

For more information go to `the analytics homepage <https://renderdoc.org/analytics>`_.

What data is gathered
---------------------

The precise data gathered may vary by build, but the principle is to gather as little data as possible while maximising the value of the data that is obtained.

Each report will contain metadata such as operating system version, RenderDoc version, which APIs have been used, which GPU vendor is in use (AMD, Intel, nVidia, etc) and whether a development or release build was run.

It may also include a handful of counters such as the average time taken to load a captured frame, and how many days in the month (as a number from 1-31) the program was used, to give a rough idea of how often people use RenderDoc.

Otherwise the majority of data is simple boolean flags. For each feature in the UI a flag is kept - these flags are left as false by default, and if the feature is ever used then the flag is set to true. There is nothing that stores how often the feature is used, or what it's used for.
