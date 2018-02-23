<p align="center"><img src="https://user-images.githubusercontent.com/661798/36482670-f81601c0-170b-11e8-8adb-2365b346ac27.png" /></p>

[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)
[![Travis CI](https://travis-ci.org/baldurk/renderdoc.svg?branch=master)](https://travis-ci.org/baldurk/renderdoc)
[![AppVeyor](https://ci.appveyor.com/api/projects/status/x46lrnvdy29ysgqp?svg=true)](https://ci.appveyor.com/project/baldurk/renderdoc)
[![Coverity Scan](https://scan.coverity.com/projects/8525/badge.svg)](https://scan.coverity.com/projects/baldurk-renderdoc)

RenderDoc is a frame-capture based graphics debugger, currently available for Vulkan, D3D11, D3D12, OpenGL, and OpenGL ES development on Windows 7 - 10, Linux, and Android. It is completely open-source under the MIT license.

If you have any questions, suggestions or problems or you can [create an issue](https://github.com/baldurk/renderdoc/issues/new) here on github, [email me directly](mailto:baldurk@baldurk.org) or [come into IRC](https://kiwiirc.com/client/irc.freenode.net/#renderdoc) to discuss it.

To install on windows run the appropriate installer for your OS ([64-bit](https://renderdoc.org/stable/1.0/RenderDoc_1.0_64.msi) | [32-bit](https://renderdoc.org/stable/1.0/RenderDoc_1.0_32.msi)) or download the portable zip from the [builds page](https://renderdoc.org/builds). On linux there is a [binary tarball](https://renderdoc.org/stable/1.0/renderdoc_1.0.tar.gz) available, or your distribution may package it. If not you can [build from source](CONTRIBUTING.md#compiling).

* **Downloads**: https://renderdoc.org/builds
* **Documentation**: [HTML online](https://renderdoc.org/docs), [CHM in builds](https://renderdoc.org/docs/renderdoc.chm), [Videos](http://www.youtube.com/user/baldurkarlsson/)
* **Contact**: [baldurk@baldurk.org](mailto:baldurk@baldurk.org), [#renderdoc on freenode IRC](https://kiwiirc.com/client/irc.freenode.net/#renderdoc)
* **Code of Conduct**: [Contributor Covenant](CODE_OF_CONDUCT.md)
* **Information for contributors**: [CONTRIBUTING.md](CONTRIBUTING.md), [Compilation instructions](CONTRIBUTING.md#compiling), [Roadmap](https://github.com/baldurk/renderdoc/wiki/Roadmap)

Screenshots
--------------

| [ ![Texture view](https://renderdoc.org/fp/ts_screen1.jpg) ](https://renderdoc.org/fp/screen1.jpg) | [ ![Pixel history & shader debug](https://renderdoc.org/fp/ts_screen2.jpg) ](https://renderdoc.org/fp/screen2.png) |
| --- | --- |
| [ ![Mesh viewer](https://renderdoc.org/fp/ts_screen3.jpg) ](https://renderdoc.org/fp/screen3.png) | [ ![Pipeline viewer & constants](https://renderdoc.org/fp/ts_screen4.jpg) ](https://renderdoc.org/fp/screen4.png) |

API Support
--------------

|                          | Windows                  | Linux                    | Android                   |
| ------------------------ | ------------------------ | ------------------------ | ------------------------  |
| Vulkan                   | :heavy_check_mark:       | :heavy_check_mark:       | :heavy_check_mark:        |
| OpenGL ES 2.0 - 3.2      | :heavy_multiplication_x: | :heavy_check_mark:       | :heavy_check_mark:        |
| OpenGL 3.2+ Core         | :heavy_check_mark:       | :heavy_check_mark:       |  N/A                      |
| D3D11 & D3D12            | :heavy_check_mark:       |  N/A                     |  N/A                      |
| OpenGL 2.0 Compatibility | :heavy_multiplication_x: | :heavy_multiplication_x: |  N/A                      |
| D3D9 & 10                | :heavy_multiplication_x: |  N/A                     |  N/A                      |
| Metal                    |  N/A                     |  N/A                     |  N/A                      |

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

