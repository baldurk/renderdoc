RenderDoc
==============

Welcome to RenderDoc - a graphics debugger, currently available for D3D11 and OpenGL development on windows. 

Quick Links:

* **Downloads**: https://renderdoc.org/builds
* **Documentation**: renderdoc.chm in the build, or http://docs.renderdoc.org/
* **Tutorials**: There are some video tutorials on Youtube: http://www.youtube.com/user/baldurkarlsson/
* **Email contact**: baldurk@baldurk.org
* **IRC channel**: #renderdoc on freenode
* **Roadmap/future development**: [Roadmap](https://github.com/baldurk/renderdoc/wiki/Roadmap)
* **Starting place for developing/contributing**: [CONTRIBUTING.md](CONTRIBUTING.md)
* **How to compile**: [COMPILE.md](COMPILE.md)

Screenshots
--------------

| [ ![Texture view](https://cloud.githubusercontent.com/assets/661798/9667117/3650453a-527a-11e5-9845-cebb26109b49.png) ](https://cloud.githubusercontent.com/assets/661798/8890997/634bf0f8-3316-11e5-9eb7-75d74e3a9356.png) | [ ![Pixel history & shader debug](https://cloud.githubusercontent.com/assets/661798/9667120/38e6d070-527a-11e5-884d-c7f11ca3e0da.png) ](https://cloud.githubusercontent.com/assets/661798/8891006/c7ad2670-3316-11e5-99e8-80f1f720f6f9.png) |
| --- | --- |
| [ ![Mesh viewer](https://cloud.githubusercontent.com/assets/661798/9667125/3ad817b8-527a-11e5-81d7-244b397092f0.png) ](https://cloud.githubusercontent.com/assets/661798/8891021/64ab5c9e-3317-11e5-827a-24002d174efc.png) | [ ![Pipeline viewer & constants](https://cloud.githubusercontent.com/assets/661798/9667129/3c5b143c-527a-11e5-9864-41ae50f74043.png) ](https://cloud.githubusercontent.com/assets/661798/8891033/ef5668ac-3317-11e5-82ff-adb97b040db1.png) |

API Support
--------------

|                  | Status                                 | Windows                  | Linux                           |
| ---------------- | -------------------------------------- | ------------------------ | ------------------------------- |
| D3D11            | Well supported, all features.          | :heavy_check_mark:       | :heavy_multiplication_x:        |
| OpenGL 3.2 core+ | Well supported, most features.\*       | :heavy_check_mark:       | :heavy_check_mark: No UI\*\*    |
| OpenGL Pre-3.2   | No immediate plans                     | :heavy_multiplication_x: | :heavy_multiplication_x:        |
| D3D10            | No immediate plans                     | :heavy_multiplication_x: | :heavy_multiplication_x:        |
| D3D9             | No immediate plans                     | :heavy_multiplication_x: | :heavy_multiplication_x:        |
| Mantle           | Plans cancelled, redirected to Vulkan. | :heavy_multiplication_x: | :heavy_multiplication_x:        |
| D3D12            | Planned for the future.                | :heavy_multiplication_x: | :heavy_multiplication_x:        |
| Vulkan           | Planned for the future.                | :heavy_multiplication_x: | :heavy_multiplication_x:        |

* D3D11 has full feature support and is stable & tested. Feature Level 11 hardware is assumed - Radeon 4000/5000+, GeForce 400+, Intel Ivy Bridge, falling back to WARP software emulation if this hardware isn't present.
* \*OpenGL is only explicitly supported for the core profile 3.2+ subset of features, check the [OpenGL wiki page](https://github.com/baldurk/renderdoc/wiki/OpenGL) for details.
* \*\*A Qt version of the UI is planned, with some [implementation notes on the wiki](https://github.com/baldurk/renderdoc/wiki/QRenderDoc-Notes).

Downloads
--------------

There are [binary releases](https://renderdoc.org/builds) available, built from the release targets. If you just want to use the program and you ended up here, this is what you want :).

It's recommended that if you're new you start with the stable builds. Beta builds are available for those who want more regular updates with the latest features and fixes, but might run into some bugs as well. Nightly builds are available every day from master branch here if you need it.

Documentation
--------------

As mentioned above there are some [youtube videos](http://www.youtube.com/user/baldurkarlsson/) showing the use of some basic features and an introduction/overview.

There is also a great presentation by [@Icetigris](https://twitter.com/Icetigris) which goes into some details of how RenderDoc can be used in real world situations: [slides are up here](https://docs.google.com/presentation/d/1LQUMIld4SGoQVthnhT1scoA3k4Sg0as14G4NeSiSgFU/edit#slide=id.p).

License
--------------

RenderDoc is released under the MIT license, see [LICENSE.md](LICENSE.md) for full text as well as 3rd party library acknowledgements.

Building
--------------

Building RenderDoc is fairly straight forward. See [COMPILE.md](COMPILE.md) for more details.

Contributing & Development
--------------

I've added some notes on how to contribute, as well as where to get started looking through the code in [COMPILE.md](COMPILE.md) - check there for more details on how to set up to build renderdoc and where to start contributing to its development.

