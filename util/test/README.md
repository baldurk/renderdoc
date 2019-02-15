# RenderDoc testing

This readme only covers the testing system specifically. For general information about renderdoc check out [the main github repository](https://github.com/baldurk/renderdoc).

## Building demos

A lot of the tests rely on a 'demos' program which contains small self-contained demos of API use.

To build on windows, open `demos.sln` and compile. There are no required external dependencies

To build on linux, compile using cmake with `demos/CMakeLists.txt`. You'll need `libX11`, `libxcb`, and `libX11-xcb`. These are all needed to build RenderDoc with GL support so you likely have them already.

**NOTE:** Currently there is one soft external dependency. If shaderc is not linked into the demos program, it expects to be able to run `glslc` at runtime to compile shaders to SPIR-V. Without this, some tests will be disabled.

Currently only windows supports linking shaderc, which happens automatically if it's found relative to the `$VULKAN_SDK` environment variable.

On linux to run the tests you'll need to modify your `PATH` variable to include wherever `demos_x64` was built to.

## Running tests

Running the tests requires the same python version as was used to build the version of RenderDoc you are testing. On windows this is likely python 3.6 as that's what comes with the repository.

**NOTE:** For windows users you also need to match the bitness, so a 64-bit python install will be needed to test a 64-bit build of RenderDoc, and similarly for 32-bit.

Then running the tests means invoking `run_tests.py` with any options you need:

* `--renderdoc` and `--pyrenderdoc` are common parameters, used to modify the OS library search path and the python module path respectively to locate the right libraries. E.g. on windows `--pyrenderdoc /path/to/renderdoc/x64/Development/pymodules --renderdoc /path/to/renderdoc/x64/Development`.
* `-l` or `--list` will list the available tests then exit.
* `-t` or `--test_include` will take a parameter giving a regexp of tests to include. Only tests matching this regexp will be included. If omitted, all tests will be run.
* `-x` or `--test_exclude` will take a parameter giving a regexp of tests to exclude. Any tests matching this regexp will be excluded. If omitted, all tests will be run.
* `--in-process` will cause the tests to be run in the same python process. By default, a child python process is created for each test so that if the test crashes it doesn't take down the whole run. Primarily useful for debugging.
* `--slow-tests` includes tests which are marked as potentially long-running. By default they are excluded so that a quick test run can be made.
* `--data` the path to the reference data folder, by default the `data/` here next to the script.
* `--artifacts` the path to the output artifacts folder, by default `artifacts/` here next to the script.
* `--temp` the path to the temporary working folder, by default `tmp/` here next to the script.
* `--data-extra` the path to the extra data folder. Some tests may reference captures which can'tbe committed to the repository here and are distributed separately or added custom by the user. By default refers to `data_extra/` here next to the script.

**NOTE:** When run, the temporary and artifacts folders will be erased.

After a run, the artifacts folder contains the output log. It's mostly plaintext but has javascript so that it displays nicely in a browser. All dependencies needed to view the log and any image diffs will be beside it, so the artifacts folder is self-contained.

## Adding a test

The demos project contains helper libraries, so the best way to get started is to copy-paste an existing test and modify it to your needs. Avoid uber-demos, try to do only one simple thing.

Similarly for tests, which often come 1:1 with demos, you can copy-paste an existing test and add your own checks.

License
--------------

RenderDoc is released under the MIT license, see [the main github repository](https://github.com/baldurk/renderdoc) for full details.

The tests use [GLAD](https://github.com/Dav1dde/glad) for extension loading, which is MIT licensed. [LZ4](https://github.com/lz4/lz4) for compression, which is BSD licensed. [volk](https://github.com/zeux/volk) for vulkan loading, which is MIT licensed. [nuklear](https://github.com/vurtun/nuklear) for the launcher UI, which is MIT licensed. [shaderc](https://github.com/google/shaderc) for building SPIR-V shaders, which is Apache-2.0 licensed.

The python tests use [pypng](https://github.com/drj11/pypng) as a pure dependency-less python png load/save library, which is MIT licensed.

A short clip from [Caminandes](http://www.caminandes.com/), available under the [Creative Commons Attribution 3.0 license (CC) caminandes.com](http://www.caminandes.com/sharing/), is used as a demo video.
