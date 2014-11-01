/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#define DECLARE_EMBED(filename) \
	extern char CONCAT( CONCAT(_binary_, filename) , _start) ; \
	extern char CONCAT( CONCAT(_binary_, filename) , _end) ;

DECLARE_EMBED(debuguniforms_h);
DECLARE_EMBED(blit_vert);
DECLARE_EMBED(blit_frag);
DECLARE_EMBED(texdisplay_frag);
DECLARE_EMBED(checkerboard_frag);
DECLARE_EMBED(mesh_vert);
DECLARE_EMBED(generic_vert);
DECLARE_EMBED(generic_frag);
DECLARE_EMBED(text_frag);
DECLARE_EMBED(text_vert);
DECLARE_EMBED(sourcecodepro_ttf);

#undef DECLARE_EMBED
