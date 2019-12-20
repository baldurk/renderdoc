# RenderDoc documentation

This readme only covers the documentation. For general information about renderdoc check out [the main github repository](https://github.com/baldurk/renderdoc).

## Generating documentation

Generating the documentation requires the same python version as was used to build the version of RenderDoc you are testing. On windows this is likely python 3.6 as that's what comes with the repository.

The documentation uses restructured text with [Sphinx](http://www.sphinx-doc.org/en/master/). Sphinx can be acquired via `pip install Sphinx`

To generate the documentation, run make.bat or make.sh found in this folder. Run `make help` to see all options, but `make html` is a likely place to start.

License
--------------

RenderDoc is released under the MIT license, see [the main github repository](https://github.com/baldurk/renderdoc) for full details.

The documentation uses [Sphinx](http://www.sphinx-doc.org/en/master/), which is BSD licensed.
