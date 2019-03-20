Credits & Acknowledgements
==========================

RenderDoc wouldn't have been possible without both the tools and libraries that were used in its construction, as well as the many people who helped and contributed to make it the program it is today.

3rd Party Credits
-----------------

The following libraries and components are incorporated into RenderDoc, listed here in no particular order. Thanks go out to the developers and contributors behind each of these libraries who generously donated their work to other free projects!

* `Google Breakpad <https://chromium.googlesource.com/breakpad/breakpad/>`_ - Copyright 2006 Google Inc, distributed under the New BSD License (3 Clause).

  Crash handling and report preparation system.

* `miniz <https://code.google.com/p/miniz/>`_ - Released to the Public Domain by Rich Geldreich.

  Used for zip read/write in several places.

* `ILM's half implementation <https://github.com/openexr/openexr/tree/master/IlmBase/Half>`_ - Copyright 2002 Industrial Light & Magic, a division of Lucas Digital Ltd. LLC, distributed under BSD license.

  Used for decoding half data for display.

* `jpeg-compressor <https://code.google.com/p/jpeg-compressor/>`_ - Released to the Public Domain by Rich Geldreich.

  Used for jpg reading and writing.

* `lz4 <https://github.com/lz4/lz4>`_ - Copyright 2013 Yann Collet, distributed under the BSD 2-Clause license.

  Used for fast compression where speed is more important than compression ratio.

* `stb <https://github.com/nothings/stb>`_ - Released to the Public Domain by Sean Barrett.

  Used to read, write and resize various image formats.

* `Source Code Pro <https://github.com/adobe-fonts/source-code-pro>`_ - Copyright 2010, 2012 Adobe Systems Incorporated, distributed under the SIL Open Font License 1.1.

  Font used for the in-program overlay.

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

* `AMD GPUPerfAPI <https://github.com/GPUOpen-Tools/GPA>`_ - Copyright (c) 2016-2018 Advanced Micro Devices, Inc., distributed under the MIT license.

  Provides hardware-specific counters over and above what individual hardware-agnostic graphics APIs are able to provide.

* `Farm-Fresh Web Icons <http://www.fatcow.com/free-icons>`_ - Copyright (c) 2009-2014 FatCow Web Hosting, distributed under Creative Commons Attribution 3.0 License.

  Providing higher-resolution icons than the famfamfam Silk set, these icons allow scaling to those using high-DPI displays.

* `AMD Radeon GPU Analyzer <https://github.com/GPUOpen-Tools/RGA>`_ - Copyright (c) 2015-2018 Advanced Micro Devices, Inc., distributed under the MIT license.

  Provides the ability to disassemble shaders from any API representation into compiled GCN ISA for lower level analysis.

* `Catch <https://github.com/philsquared/Catch>`_ - Copyright (c) 2012 Two Blue Cubes Ltd., distributed under the Boost Software License.

  Implements unit testing during development.

* `zstd <https://github.com/facebook/zstd>`_ - Copyright (c) 2016-present, Facebook, Inc., distributed under the BSD License.

  Compresses capture files at a higher rate of compression off-line (not at capture time).

* `pugixml <https://pugixml.org/>`_ - Copyright (c) 2006-2017 Arseny Kapoulkine, distributed under the MIT License.

  Used for converting capture files to and from xml.

* `AOSP <https://source.android.com/>`_ - Copyright (c) 2006-2016, The Android Open Source Project, distributed under the Apache 2.0 License.

  Used to simplify Android workflows by distributing some tools from the android SDK, as well as patching android manifest files to enable debugging.

* `interceptor-lib <https://github.com/google/gapid>`_ - Copyright (c) 2017, Google Inc., distributed under the Apache 2.0 License.

  Taken from the GAPID project, used to inject hooks into android library functions if LLVM is available at build-time.

* `LLVM <http://llvm.org/>`_ - Copyright (c) 2003-2017 University of Illinois at Urbana-Champaign, distributed under the University of Illinois/NCSA Open Source License.

  Used to support interceptor-lib to inject hooks into android library functions.

* `OpenSSL <https://www.openssl.org/>`_ - Copyright (c) 1998-2018 The OpenSSL Project. Copyright (C) 1995-1998 Eric Young. Distributed under the double license of the OpenSSL and SSLeay licenses.

  Used to connect securely to RenderDoc's servers for update checks and bug reports.

* `Microsoft PDB Information <https://github.com/Microsoft/microsoft-pdb/>`_ - Copyright (c) 2015 Microsoft Corporation. Distributed under the MIT License.

  Used to recover debug information from DXBC shaders on D3D11/D3D12.

Thanks
------

Screenshots in this documentation are from `Sascha Willems' Vulkan demos <https://github.com/SaschaWillems/Vulkan>`_.

There have been many people who have helped in the creation of RenderDoc. Whether testing, providing feedback, or contributing artwork and design critique everyone listed here and many more besides have been invaluable in taking RenderDoc from an idea on paper to its current state. Greets fly out to the following people, listed in no particular order.

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
* Cody Northrop
* Dominik Baumeister
* Adrian Bucur
* Peter Gal
* Janos Pantos
* Marton Tamas
* Nat Duca
* Ben Clayton
* Aliya Pazylbekova
* Benson Joeris
* Haiyu Zhen
* Alex Kharlamov

Contributors
------------

The following list highlights notable open source contributions. Many other people have contributed individual bug fixes and tweaks, which can be seen `on github <https://github.com/baldurk/renderdoc/graphs/contributors>`_ !

* Michael Vance - Implemented a sophisticated frame statistics system for D3D11 around binding and draw API calls.
* Matthäus G. Chajdas - Converted this documentation from sandcastle to sphinx.
* Michael Rennie, Peter Gal, and Janos Pantos at Samsung - Added support for Android platform capture as well as Vulkan and OpenGL ES support.
* Adrian Bucur - Added custom SPIR-V disassembler support.
* James Fulop - Updated the vertex picking algorithm.
* Balazs Torok - Implemented the RenderDoc in-application overlay for D3D9.
