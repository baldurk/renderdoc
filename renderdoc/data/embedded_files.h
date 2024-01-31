/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#if !defined(CONCAT)
#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)
#endif

#define DECLARE_EMBED(filename)                   \
  extern unsigned char CONCAT(data_, filename)[]; \
  extern int CONCAT(CONCAT(data_, filename), _len);

DECLARE_EMBED(sourcecodepro_ttf);
DECLARE_EMBED(glsl_blit_vert);
DECLARE_EMBED(glsl_checkerboard_frag);
DECLARE_EMBED(glsl_texdisplay_frag);
DECLARE_EMBED(glsl_gltext_vert);
DECLARE_EMBED(glsl_gltext_frag);
DECLARE_EMBED(glsl_vktext_vert);
DECLARE_EMBED(glsl_vktext_frag);
DECLARE_EMBED(glsl_fixedcol_frag);
DECLARE_EMBED(glsl_mesh_vert);
DECLARE_EMBED(glsl_mesh_geom);
DECLARE_EMBED(glsl_mesh_frag);
DECLARE_EMBED(glsl_trisize_geom);
DECLARE_EMBED(glsl_trisize_frag);
DECLARE_EMBED(glsl_minmaxtile_comp);
DECLARE_EMBED(glsl_minmaxresult_comp);
DECLARE_EMBED(glsl_histogram_comp);
DECLARE_EMBED(glsl_glsl_globals_h);
DECLARE_EMBED(glsl_glsl_ubos_h);
DECLARE_EMBED(glsl_gl_texsample_h);
DECLARE_EMBED(glsl_vk_texsample_h);
DECLARE_EMBED(glsl_quadresolve_frag);
DECLARE_EMBED(glsl_quadwrite_frag);
DECLARE_EMBED(glsl_mesh_comp);
DECLARE_EMBED(glsl_array2ms_comp);
DECLARE_EMBED(glsl_ms2array_comp);
DECLARE_EMBED(glsl_deptharr2ms_frag);
DECLARE_EMBED(glsl_depthms2arr_frag);
DECLARE_EMBED(glsl_gles_texsample_h);
DECLARE_EMBED(glsl_pixelhistory_mscopy_comp);
DECLARE_EMBED(glsl_pixelhistory_mscopy_depth_comp);
DECLARE_EMBED(glsl_pixelhistory_primid_frag);
DECLARE_EMBED(glsl_shaderdebug_sample_vert);
DECLARE_EMBED(glsl_texremap_frag);
DECLARE_EMBED(glsl_discard_frag);
DECLARE_EMBED(glsl_vk_ms2buffer_comp);
DECLARE_EMBED(glsl_vk_depthms2buffer_comp);
DECLARE_EMBED(glsl_vk_buffer2ms_comp);
DECLARE_EMBED(glsl_vk_depthbuf2ms_frag);
DECLARE_EMBED(glsl_depth_copy_frag);
DECLARE_EMBED(glsl_depth_copyms_frag);

#undef DECLARE_EMBED
