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

DECLARE_EMBED(glsl_debuguniforms_h);
DECLARE_EMBED(glsl_texsample_h);
DECLARE_EMBED(glsl_blit_vert);
DECLARE_EMBED(glsl_blit_frag);
DECLARE_EMBED(glsl_texdisplay_frag);
DECLARE_EMBED(glsl_checkerboard_frag);
DECLARE_EMBED(glsl_histogram_comp);
DECLARE_EMBED(glsl_quadoverdraw_frag);
DECLARE_EMBED(glsl_arraymscopy_comp);
DECLARE_EMBED(glsl_mesh_vert);
DECLARE_EMBED(glsl_mesh_frag);
DECLARE_EMBED(glsl_mesh_geom);
DECLARE_EMBED(glsl_mesh_comp);
DECLARE_EMBED(glsl_generic_vert);
DECLARE_EMBED(glsl_generic_frag);
DECLARE_EMBED(glsl_text_frag);
DECLARE_EMBED(glsl_text_vert);
DECLARE_EMBED(glsl_outline_frag);
DECLARE_EMBED(sourcecodepro_ttf);
DECLARE_EMBED(spv_blit_vert);
DECLARE_EMBED(spv_checkerboard_frag);
DECLARE_EMBED(spv_texdisplay_frag);
DECLARE_EMBED(spv_text_vert);
DECLARE_EMBED(spv_text_frag);
DECLARE_EMBED(spv_fixedcol_frag);
DECLARE_EMBED(spv_mesh_vert);
DECLARE_EMBED(spv_mesh_geom);
DECLARE_EMBED(spv_mesh_frag);
DECLARE_EMBED(spv_minmaxtile_comp);
DECLARE_EMBED(spv_minmaxresult_comp);
DECLARE_EMBED(spv_histogram_comp);
DECLARE_EMBED(spv_outline_frag);
DECLARE_EMBED(spv_debuguniforms_h);
DECLARE_EMBED(spv_texsample_h);
DECLARE_EMBED(spv_quadresolve_frag);
DECLARE_EMBED(spv_quadwrite_frag);

#undef DECLARE_EMBED
