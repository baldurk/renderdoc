RenderDoc
==============

[![Travis CI](https://travis-ci.org/baldurk/renderdoc.svg?branch=master)](https://travis-ci.org/baldurk/renderdoc)
[![AppVeyor](https://ci.appveyor.com/api/projects/status/x46lrnvdy29ysgqp?svg=true)](https://ci.appveyor.com/project/baldurk/renderdoc)
[![Coverity Scan](https://scan.coverity.com/projects/8525/badge.svg)](https://scan.coverity.com/projects/baldurk-renderdoc)
[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

RenderDoc - a graphics debugger, currently available for Vulkan, D3D11, D3D12, and OpenGL development on windows.

If you have any questions, suggestions or problems or you can [create an issue](https://github.com/baldurk/renderdoc/issues/new) here on github, [email me directly](mailto:baldurk@baldurk.org) or [come into IRC](https://kiwiirc.com/client/irc.freenode.net/#renderdoc) to discuss it.

* **Downloads**: https://renderdoc.org/builds
* **Documentation**: [renderdoc.chm](https://renderdoc.org/docs/renderdoc.chm) in builds, https://renderdoc.org/docs, http://www.youtube.com/user/baldurkarlsson/
* **Contact**: [baldurk@baldurk.org](mailto:baldurk@baldurk.org), [#renderdoc on freenode IRC](https://kiwiirc.com/client/irc.freenode.net/#renderdoc)
* **Information for developing/contributing**: [CONTRIBUTING.md](CONTRIBUTING.md), [Compilation instructions](CONTRIBUTING.md#compiling), [Roadmap](https://github.com/baldurk/renderdoc/wiki/Roadmap)
* **Code of Conduct**: [Contributor Covenant](CODE_OF_CONDUCT.md)

Screenshots
--------------

| [ ![Texture view](https://renderdoc.org/fp/ts_screen1.jpg) ](https://renderdoc.org/fp/screen1.jpg) | [ ![Pixel history & shader debug](https://renderdoc.org/fp/ts_screen2.jpg) ](https://renderdoc.org/fp/screen2.png) |
| --- | --- |
| [ ![Mesh viewer](https://renderdoc.org/fp/ts_screen3.jpg) ](https://renderdoc.org/fp/screen3.png) | [ ![Pipeline viewer & constants](https://renderdoc.org/fp/ts_screen4.jpg) ](https://renderdoc.org/fp/screen4.png) |

API Support
--------------

|                            | Status                           | Windows                  | Linux                    |
| -------------------------- | -------------------------------- | ------------------------ | ------------------------ |
| D3D11                      | Well supported, all features.    | :heavy_check_mark:       | :heavy_multiplication_x: |
| OpenGL 3.2 core+           | Well supported, most features.\* | :heavy_check_mark:       | :heavy_check_mark:       |
| Vulkan                     | Well supported, most features.   | :heavy_check_mark:       | :heavy_check_mark:       |
| D3D12                      | Well supported, most features.   | :heavy_check_mark:       | :heavy_multiplication_x: |
| OpenGL Compatibility, GLES | No immediate plans               | :heavy_multiplication_x: | :heavy_multiplication_x: |
| D3D9 & 10                  | No immediate plans               | :heavy_multiplication_x: | :heavy_multiplication_x: |
| Metal                      | No immediate plans               | :heavy_multiplication_x: | :heavy_multiplication_x: |

* D3D11 has full feature support and is stable & tested. Feature Level 11 hardware is assumed - Radeon 4000/5000+, GeForce 400+, Intel Ivy Bridge, falling back to WARP software emulation if this hardware isn't present.
* \*OpenGL is only explicitly supported for the core profile 3.2+ subset of features, check the [OpenGL wiki page](https://github.com/baldurk/renderdoc/wiki/OpenGL) for details.
* Currently [the Qt UI](qrenderdoc) is only used on linux. It is working well with a [TODO list of remaining work](https://github.com/baldurk/renderdoc/issues/494). Work is on-going for it to replace the .NET UI on windows.

Downloads
--------------

There are [binary releases](https://renderdoc.org/builds) available, built from the release targets. If you just want to use the program and you ended up here, this is what you want :).

It's recommended that if you're new you start with the stable builds. Nightly builds are available every day from master branch here if you need it, but correspondingly may be less stable.

Documentation
--------------

The text documentation is available [online for the latest stable version](https://renderdoc.org/docs/), as well as in [renderdoc.chm](https://renderdoc.org/docs/renderdoc.chm) in any build. It's built from [restructured text with sphinx](docs).

As mentioned above there are some [youtube videos](http://www.youtube.com/user/baldurkarlsson/) showing the use of some basic features and an introduction/overview.

There is also a great presentation by [@Icetigris](https://twitter.com/Icetigris) which goes into some details of how RenderDoc can be used in real world situations: [slides are up here](https://docs.google.com/presentation/d/1LQUMIld4SGoQVthnhT1scoA3k4Sg0as14G4NeSiSgFU/edit#slide=id.p).

License
--------------

RenderDoc is released under the MIT license, see [LICENSE.md](LICENSE.md) for full text as well as 3rd party library acknowledgements.

Contributing & Development
--------------

Building RenderDoc is fairly straight forward. See [CONTRIBUTING.md](CONTRIBUTING.md#compiling) for more details.

I've added some notes on how to contribute, as well as where to get started looking through the code in [CONTRIBUTING.md](CONTRIBUTING.md).

