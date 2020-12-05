# PyCharm helpers

PyCharm does a fairly good job of code completion etc with RenderDoc's python modules, but when generating reflection for the modules it doesn't quite get everything - e.g. some parameter types and especially return types aren't properly parsed and don't get detected as the right types.

The information is in the docstrings in standard format but the skeleton generator PyCharm uses has some issues.

The file in this directory tree can be copied over the corresponding file in the PyCharm distribution to generate better skeletons. This is written against PyCharm Community Edition 2020.3, though most likely the patched file can be applied over other versions successfully.
