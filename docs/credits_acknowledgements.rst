Credits & Acknowledgements
==========================

RenderDoc wouldn't have been possible without both the tools and libraries that were used in its construction, as well as the many people who helped and contributed to make it the program it is today.

3rd Party Credits
-----------------

The following libraries and components are incorporated into RenderDoc, listed here in no particular order. Thanks go out to the developers and contributors behind each of these libraries who generously donated their work to other free projects!

* `TreeView with Columns <http://www.codeproject.com/Articles/23746/TreeView-with-Columns>`_ - Copyright 2008 by jkristia, distributed under the `CPOL license <http://www.codeproject.com/info/cpol10.aspx>`_.

  An invaluable control filling a much needed niche in .NET winforms.

* `DockPanel <http://dockpanelsuite.com/>`_ - Copyright 2007 Weifen Luo, distributed under the MIT license.

  A mature and stable library that adds the docking and flexibility of RenderDoc's UI.

* `famfamfam Silk Icon set <http://www.famfamfam.com/lab/icons/silk/>`_ - Authored by Mark James, distributed under Creative Commons Attribution 2.5.

  Lending an air of professionalism and artistic quality to the UI, the Silk icon set is used throughout RenderDoc.

* `Scintilla.NET <http://scintillanet.codeplex.com/>`_ - ScintillaNET Copyright 2002-2006 Garrett Serack, `Scintilla <http://www.scintilla.org/>`_ Copyright 1998-2006 Neil Hodgson, distributed under the MIT license.

  Scintilla and the wrapper Scintilla.NET provide a powerful text editor for the shader viewers.

* `Google Breakpad <https://chromium.googlesource.com/breakpad/breakpad/>`_ - Copyright 2006 Google Inc, distributed under the New BSD License (3 Clause).

  provides a rock-solid crash handling and reporting base that help keep RenderDoc stable.

* `miniz <https://code.google.com/p/miniz/>`_ - Released to the Public Domain by Rich Geldreich.

  Public domain zip library is used to compress the crash reports for sending.

* `ILM's half implementation <https://github.com/openexr/openexr/tree/master/IlmBase/Half>`_ - Copyright 2002 Industrial Light & Magic, a division of Lucas Digital Ltd. LLC, distributed under BSD license.

  Used for decoding half data for display.

* `jpeg-compressor <https://code.google.com/p/jpeg-compressor/>`_ - Released to the Public Domain by Rich Geldreich.

  Used to compress screenshots into jpg format for thumbnail previews.

* `lz4 <https://github.com/Cyan4973/lz4>`_ - Copyright 2013 Yann Collet, distributed under the New BSD License (3 Clause).

  compresses large data transfers (textures and buffers) when going across network connections as well as in the capture files themselves.

* `stb <https://github.com/nothings/stb>`_ - Released to the Public Domain by Sean Barrett.

  Used to read, write and resize various image formats.

* `Source Code Pro <https://github.com/adobe-fonts/source-code-pro>`_ - Copyright 2010, 2012 Adobe Systems Incorporated, distributed under the SIL Open Font License 1.1.

  Font used for the in-program overlay.

* `IronPython <http://ironpython.net/>`_ - Copyright IronPython Team, distributed under the Apache 2.0 license.

  Used for the Python shell/integration in the UI.

* `tinyexr <https://github.com/syoyo/tinyexr>`_ - Copyright 2014 Syoyo Fujita, distributed under the New BSD License (3 Clause).

  Used for the OpenEXR file loading and saving.

* `glslang <https://github.com/KhronosGroup/glslang>`_ - Copyright 2002-2005 3Dlabs Inc. Ltd, Copyright 2012-2013 LunarG, Inc, distributed under the New BSD License (3 Clause).

  Used for compiling GLSL to SPIR-V.

* `Qt <http://www.qt.io/>`_ - Copyright 2015 The Qt Company Ltd, distributed under the GNU Lesser General Public License (LGPL) version 2.1.

  Used for QRenderDoc replay UI program.

* `cmdline <https://github.com/tanakh/cmdline>`_ - Copyright 2009 Hideyuki Tanaka, distributed under the New BSD License (3 Clause).

  Used for parsing command line arguments to renderdoccmd.

* `include-bin <https://github.com/tanakh/cmdline>`_ - Copyright 2016 Hubert Jarosz, distributed under the zlib license.

  Used to compile in data files embedded into the source on non-Windows platforms.

* `plthook <https://github.com/kubo/plthook>`_ - Copyright 2013-2014 Kubo Takehiro, distributed under the 2-clause BSD license.

  Used for hooking some libraries loaded with DEEPBIND on linux.

* `tinyfiledialogs <https://sourceforge.net/projects/tinyfiledialogs/>`_ - Copyright (c) 2014 - 2016 Guillaume Vareille, distributed under the zlib license.

  Used to display message boxes cross-platform from the non-UI core code.

* `AMD GPUPerfAPI <https://github.com/GPUOpen-Tools/GPA>`_ - Copyright (c) 2016 Advanced Micro Devices, Inc., distributed under the MIT license.

  Provides hardware-specific counters over and above what individual hardware-agnostic graphics APIs are able to provide.

* `Farm-Fresh Web Icons <http://www.fatcow.com/free-icons>`_ - Copyright (c) 2009-2014 FatCow Web Hosting, distributed under Creative Commons Attribution 3.0 License.

  Providing higher-resolution icons than the famfamfam Silk set, these icons allow scaling to those using high-DPI displays.

* `AMD Radeon GPU Analyzer <https://github.com/GPUOpen-Tools/RGA>`_ - Copyright (c) 2015 Advanced Micro Devices, Inc., distributed under the MIT license.

  Provides the ability to disassemble shaders from any API representation into compiled GCN ISA for lower level analysis.

Thanks
------

There have been many people who have helped in the creation of RenderDoc. Whether testing, feedback or contributing artwork and design critique everyone listed here and many more besides have been invaluable in taking RenderDoc from an idea on paper to its current state. Greets fly out to the following people, listed in no particular order.

* Chris Bunner, Charlie Cole, James Chilvers, Andrew Khan, Benjamin Hill, Jake Turner, Alex Weighell and the rest of the Crytek UK R&D team.
* Colin Bonstead, Marco Corbetta, Pascal Eggert, Marcel Hatam, Sascha Hoba, Theodor Mader, Mathieu Pinard, Chris Raine, Nicolas Schulz, Tiago Sousa, Sean Tracy, Carsten Wenzel, and everyone else at the rest of the Crytek Studios.
* Daniel Sexton
* Jason Mendel
* Jacob Kapostins
* Iain Cantlay
* Luke Lambert
* Gareth Thomas
* George Ilenei
* Matías N. Goldberg
* Louis De Carufel
* Steve Marton
* Elizabeth Baumel
* Jon Ashburn
* Greg Fischer
* Karen Ghavem
* Jens Owen
* Derrick Owens
* Jon Kennedy
* Matthäus G. Chajdas
* Dan Ginsburg
* Dean Sekulic
* Rolando Caloca Olivares
* Arne Schober
* Michael Vance
* Dominik Witczak
* Chia-I Wu
* Cory Bloor
* John McDonald
* Pierre-Loup Griffais
* Jason Mitchell
* Michael Rennie
* Ian Elliot
* Callan McInally
* Gordon Selley

Contributors
------------

The following list highlights notable open source contributions. Many other people have contributed individual bug fixes and tweaks, which can be seen `on github <https://github.com/baldurk/renderdoc/graphs/contributors>`_ !

* Michael Vance - Implemented a sophisticated frame statistics system for D3D11 around binding and draw API calls.
* Matthäus G. Chajdas - Converted this documentation from sandcastle to sphinx.
* Michael Rennie - Added support for Android platform capture.
* Adrian Bucur - Added custom SPIR-V disassembler support.
* James Fulop - Updated the vertex picking algorithm.
* Balazs Torok - Implemented the RenderDoc in-application overlay for D3D9.
