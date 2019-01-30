/*
  Copyright:	(c) 1999-2013 Apple Inc. All rights reserved.
*/

// combined from OpenGL framework headers, to provide CGL declarations without deprecation warnings
// or in future even deletion

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
  
/************************************************************************/
/* gltypes.h                                                            */
/************************************************************************/

#include <stdint.h>

typedef uint32_t GLbitfield;
typedef uint8_t  GLboolean;
typedef int8_t   GLbyte;
typedef float    GLclampf;
#ifndef GLenum
typedef uint32_t GLenum;
#endif
typedef float    GLfloat;
typedef int32_t  GLint;
typedef int16_t  GLshort;
typedef int32_t  GLsizei;
typedef uint8_t  GLubyte;
typedef uint32_t GLuint;
typedef uint16_t GLushort;
typedef void     GLvoid;

#if !defined(GL_VERSION_2_0)
typedef char     GLchar;
#endif
#if !defined(GL_ARB_shader_objects)
typedef char     GLcharARB;
typedef void    *GLhandleARB;
#endif
typedef double   GLdouble;
typedef double   GLclampd;
#if !defined(ARB_ES2_compatibility) && !defined(GL_VERSION_4_1)
typedef int32_t  GLfixed;
#endif
#if !defined(GL_ARB_half_float_vertex) && !defined(GL_VERSION_3_0)
typedef uint16_t GLhalf;
#endif
#if !defined(GL_ARB_half_float_pixel)
typedef uint16_t GLhalfARB;
#endif
#if !defined(GL_ARB_sync) && !defined(GL_VERSION_3_2)
typedef int64_t  GLint64;
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
#endif
#if !defined(GL_EXT_timer_query)
typedef int64_t  GLint64EXT;
typedef uint64_t GLuint64EXT;
#endif
#if !defined(GL_VERSION_1_5)
typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;
#endif
#if !defined(GL_ARB_vertex_buffer_object)
typedef intptr_t GLintptrARB;
typedef intptr_t GLsizeiptrARB;
#endif

/************************************************************************/
/* CGLTypes.h                                                           */
/************************************************************************/

#if __has_feature(assume_nonnull)
#define OPENGL_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define OPENGL_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#else
#define OPENGL_ASSUME_NONNULL_BEGIN
#define OPENGL_ASSUME_NONNULL_END
#endif

#if __has_feature(nullability)
#define OPENGL_NULLABLE __nullable
#define OPENGL_NONNULL __nonnull
#else
#define OPENGL_NULLABLE
#define OPENGL_NONNULL
#endif

#if __has_attribute(objc_bridge) && __has_feature(objc_bridge_id) && __has_feature(objc_bridge_id_on_typedefs)
#define OPENGL_BRIDGED_TYPE(T)		__attribute__((objc_bridge(T)))
#else
#define OPENGL_BRIDGED_TYPE(T)
#endif

#if __has_feature(objc_class_property)
#define OPENGL_SWIFT_NAME(name) __attribute__((swift_name(#name)))
#else
#define OPENGL_SWIFT_NAME(name)
#endif

/*
** CGL opaque data.
*/
typedef struct _CGLContextObject       *CGLContextObj;
typedef struct _CGLPixelFormatObject   *CGLPixelFormatObj;
typedef struct _CGLRendererInfoObject  *CGLRendererInfoObj;
typedef struct _CGLPBufferObject       *CGLPBufferObj;

/*
** Attribute names for CGLChoosePixelFormat and CGLDescribePixelFormat.
*/
typedef enum _CGLPixelFormatAttribute {
	kCGLPFAAllRenderers                                                   =   1, /* choose from all available renderers          */
	kCGLPFATripleBuffer                                                   =   3, /* choose a triple buffered pixel format        */
	kCGLPFADoubleBuffer                                                   =   5, /* choose a double buffered pixel format        */
	kCGLPFAColorSize                                                      =   8, /* number of color buffer bits                  */
	kCGLPFAAlphaSize                                                      =  11, /* number of alpha component bits               */
	kCGLPFADepthSize                                                      =  12, /* number of depth buffer bits                  */
	kCGLPFAStencilSize                                                    =  13, /* number of stencil buffer bits                */
	kCGLPFAMinimumPolicy                                                  =  51, /* never choose smaller buffers than requested  */
	kCGLPFAMaximumPolicy                                                  =  52, /* choose largest buffers of type requested     */
	kCGLPFASampleBuffers                                                  =  55, /* number of multi sample buffers               */
	kCGLPFASamples                                                        =  56, /* number of samples per multi sample buffer    */
	kCGLPFAColorFloat                                                     =  58, /* color buffers store floating point pixels    */
	kCGLPFAMultisample                                                    =  59, /* choose multisampling                         */
	kCGLPFASupersample                                                    =  60, /* choose supersampling                         */
	kCGLPFASampleAlpha                                                    =  61, /* request alpha filtering                      */
	kCGLPFARendererID                                                     =  70, /* request renderer by ID                       */
	kCGLPFANoRecovery                                                     =  72, /* disable all failure recovery systems         */
	kCGLPFAAccelerated                                                    =  73, /* choose a hardware accelerated renderer       */
	kCGLPFAClosestPolicy                                                  =  74, /* choose the closest color buffer to request   */
	kCGLPFABackingStore                                                   =  76, /* back buffer contents are valid after swap    */
	kCGLPFABackingVolatile                                                =  77, /* back buffer contents are volatile after swap */
	kCGLPFADisplayMask                                                    =  84, /* mask limiting supported displays             */
	kCGLPFAAllowOfflineRenderers                                          =  96, /* show offline renderers in pixel formats      */
	kCGLPFAAcceleratedCompute                                             =  97, /* choose a hardware accelerated compute device */
	kCGLPFAOpenGLProfile                                                  =  99, /* specify an OpenGL Profile to use             */
	kCGLPFASupportsAutomaticGraphicsSwitching                             = 101, /* responds to display changes                  */
	kCGLPFAVirtualScreenCount                                             = 128, /* number of virtual screens in this format     */

	/* Note: the following attributes are deprecated in Core Profile                    */
	kCGLPFAAuxBuffers                                  =   7, /* number of aux buffers                        */
	kCGLPFAAccumSize                                   =  14, /* number of accum buffer bits                  */
	kCGLPFAAuxDepthStencil                             =  57, /* each aux buffer has its own depth stencil    */

	kCGLPFAStereo                                            =   6,
	kCGLPFAOffScreen                                         =  53,
	kCGLPFAWindow                                            =  80,
	kCGLPFACompliant                                         =  83,
	kCGLPFAPBuffer                                           =  90,
	kCGLPFARemotePBuffer                                     =  91,

	kCGLPFASingleRenderer                                    =  71,
	kCGLPFARobust                                            =  75,
	kCGLPFAMPSafe                                            =  78,
	kCGLPFAMultiScreen                                       =  81,
	kCGLPFAFullScreen                                        =  54,
} CGLPixelFormatAttribute;

/*
** Property names for CGLDescribeRenderer.
*/
typedef enum _CGLRendererProperty {
	kCGLRPOffScreen                                          =  53,
	kCGLRPRendererID                                         =  70,
	kCGLRPAccelerated                                        =  73,
	kCGLRPBackingStore                                       =  76,
	kCGLRPWindow                                             =  80,
	kCGLRPCompliant                                          =  83,
	kCGLRPDisplayMask                                        =  84,
	kCGLRPBufferModes                                        = 100, /* a bitfield of supported buffer modes             */
	kCGLRPColorModes                                         = 103, /* a bitfield of supported color buffer formats     */
	kCGLRPAccumModes                                         = 104, /* a bitfield of supported accum buffer formats     */
	kCGLRPDepthModes                                         = 105, /* a bitfield of supported depth buffer depths      */
	kCGLRPStencilModes                                       = 106, /* a bitfield of supported stencil buffer depths    */
	kCGLRPMaxAuxBuffers                                      = 107, /* maximum number of auxilliary buffers             */
	kCGLRPMaxSampleBuffers                                   = 108, /* maximum number of sample buffers                 */
	kCGLRPMaxSamples                                         = 109, /* maximum number of samples                        */
	kCGLRPSampleModes                                        = 110, /* a bitfield of supported sample modes             */
	kCGLRPSampleAlpha                                        = 111, /* support for alpha sampling                       */
	kCGLRPGPUVertProcCapable                                 = 122, /* renderer capable of GPU vertex processing        */
	kCGLRPGPUFragProcCapable                                 = 123, /* renderer capable of GPU fragment processing      */
	kCGLRPRendererCount                                      = 128, /* the number of renderers in this renderer info    */
	kCGLRPOnline                                             = 129, /* a boolean stating if renderer is on/offline      */
	kCGLRPAcceleratedCompute                                 = 130, /* hardware accelerated compute device              */
	kCGLRPVideoMemoryMegabytes                               = 131, /* total video memory (in megabytes)                */
	kCGLRPTextureMemoryMegabytes                             = 132, /* video memory useable for textures (in megabytes) */
	kCGLRPMajorGLVersion                                     = 133, /* maximum supported major GL revision              */

	kCGLRPRegistryIDLow                                      = 140, /* Low 32-bits of registryID */
	kCGLRPRegistryIDHigh                                     = 141, /* High 32-bits of registryID */
	kCGLRPRemovable                                          = 142, /* renderer is removable (eGPU) */
	
	kCGLRPRobust                                             =  75,
	kCGLRPMPSafe                                             =  78,
	kCGLRPMultiScreen                                        =  81,
	kCGLRPFullScreen                                         =  54,
	kCGLRPVideoMemory                                        = 120,
	kCGLRPTextureMemory                                      = 121,
} CGLRendererProperty;

/*
** Enable names for CGLEnable, CGLDisable, and CGLIsEnabled.
*/
typedef enum _CGLContextEnable {
	kCGLCESwapRectangle                                       = 201, /* Enable or disable the swap rectangle              */
	kCGLCESwapLimit                                           = 203, /* Enable or disable the swap async limit            */
	kCGLCERasterization                                       = 221, /* Enable or disable all rasterization               */
	kCGLCEStateValidation                                     = 301, /* Validate state for multi-screen functionality     */
	kCGLCESurfaceBackingSize                                  = 305, /* Enable or disable surface backing size override   */
	kCGLCEDisplayListOptimization                             = 307, /* Ability to turn off display list optimizer        */
	kCGLCEMPEngine                                            = 313, /* Enable or disable multi-threaded GL engine        */
	kCGLCECrashOnRemovedFunctions                             = 316  /* Die on call to function removed from Core Profile */
} CGLContextEnable;

/*
** GPURestartStatus names
*/
typedef enum _CGLGPURestartStatus { /* GPU Restart Status */
	kCGLCPGPURestartStatusNone        = 0, /* current context has not caused recent GPU restart */
	kCGLCPGPURestartStatusCaused      = 1, /* current context caused recent GPU restart (auto-clear on query) */
	kCGLCPGPURestartStatusBlacklisted = 2, /* current context is being ignored for excessive GPU restarts (won't clear on query) */
} CGLGPURestartStatus;

/*
** Parameter names for CGLSetParameter and CGLGetParameter.
*/
typedef enum _CGLContextParameter {
	kCGLCPSwapRectangle                                      = 200, /* 4 params.  Set or get the swap rectangle {x, y, w, h}        */
	kCGLCPSwapInterval                                       = 222, /* 1 param.   0 -> Don't sync, 1 -> Sync to vertical retrace    */
	kCGLCPDispatchTableSize                                  = 224, /* 1 param.   Get the dispatch table size                       */
	/* Note: kCGLCPClientStorage is always a pointer-sized parameter, even though the API claims GLint. */
	kCGLCPClientStorage                                      = 226, /* 1 param.   Context specific generic storage                  */
	kCGLCPSurfaceTexture                                     = 228, /* 3 params.  Context, target, internal_format                  */
/*  - Used by AGL - */
/*  AGL_STATE_VALIDATION                                       230 */
/*  AGL_BUFFER_NAME                                            231 */
/*  AGL_ORDER_CONTEXT_TO_FRONT                                 232 */
/*  AGL_CONTEXT_SURFACE_ID                                     233 */
/*  AGL_CONTEXT_DISPLAY_ID                                     234 */
	kCGLCPSurfaceOrder                                       = 235, /* 1 param.   1 -> Above window, -1 -> Below Window             */
	kCGLCPSurfaceOpacity                                     = 236, /* 1 param.   1 -> Surface is opaque (default), 0 -> non-opaque */
/*  - Used by AGL - */
/*  AGL_CLIP_REGION                                            254 */
/*  AGL_FS_CAPTURE_SINGLE                                      255 */
	kCGLCPSurfaceBackingSize                                 = 304, /* 2 params.  Width/height of surface backing size              */
/*  AGL_SURFACE_VOLATILE                                       306 */
	kCGLCPSurfaceSurfaceVolatile                             = 306, /* 1 param.   Surface volatile state                            */
	kCGLCPReclaimResources                                   = 308, /* 0 params.                                                    */
	kCGLCPCurrentRendererID                                  = 309, /* 1 param.   Retrieves the current renderer ID                 */
	kCGLCPGPUVertexProcessing                                = 310, /* 1 param.   Currently processing vertices with GPU (get)      */
	kCGLCPGPUFragmentProcessing                              = 311, /* 1 param.   Currently processing fragments with GPU (get)     */
	kCGLCPHasDrawable                                        = 314, /* 1 param.   Boolean returned if drawable is attached			*/
	kCGLCPMPSwapsInFlight                                    = 315, /* 1 param.   Max number of swaps queued by the MP GL engine	*/
	kCGLCPGPURestartStatus                                   = 317, /* 1 param.   Retrieves and clears the current CGLGPURestartStatus */
	kCGLCPAbortOnGPURestartStatusBlacklisted                 = 318, /* 1 param.  Establish action to take upon blacklisting */
	kCGLCPSupportGPURestart                                  = 319, /* 1 param.   Does driver support auto-restart of GPU on hang/crash? */
	kCGLCPSupportSeparateAddressSpace                        = 320, /* 1 param. Does context get its own GPU address space?   */
	kCGLCPContextPriorityRequest                             = 608, /* 1 param. kCGLCPContextPriorityRequest[High|Normal|Low] 0|1|2 */
} CGLContextParameter;

typedef enum
{
	kCGLCPContextPriorityRequestHigh   = 0,
	kCGLCPContextPriorityRequestNormal = 1,
	kCGLCPContextPriorityRequestLow    = 2
} CGLCPContextPriorityRequest;

/*
** Option names for CGLSetOption and CGLGetOption.
*/
typedef enum _CGLGlobalOption {
	kCGLGOFormatCacheSize                           = 501, /* Set the size of the pixel format cache        */
	kCGLGOClearFormatCache                          = 502, /* Reset the pixel format cache if true          */
	kCGLGORetainRenderers                           = 503, /* Whether to retain loaded renderers in memory  */
	kCGLGOUseBuildCache                             = 506, /* Enable the function compilation block cache.  */
	                                                       /* Off by default.  Must be enabled at startup.  */
	
	kCGLGOResetLibrary                                        = 504,
	kCGLGOUseErrorHandler                                     = 505,
} CGLGlobalOption;

/*
** OpenGL Implementation Profiles
*/
typedef enum _CGLOpenGLProfile {
	kCGLOGLPVersion_Legacy                               = 0x1000, /* choose a renderer compatible with GL1.0       */
	kCGLOGLPVersion_3_2_Core                             = 0x3200, /* choose a renderer capable of GL3.2 or later   */
	kCGLOGLPVersion_GL3_Core                             = 0x3200, /* choose a renderer capable of GL3.2 or later   */
	kCGLOGLPVersion_GL4_Core                             = 0x4100, /* choose a renderer capable of GL4.1 or later   */
} CGLOpenGLProfile;

/*
** Error return values from CGLGetError.
*/
typedef enum _CGLError {
	kCGLNoError            = 0,     /* no error */
	kCGLBadAttribute       = 10000,	/* invalid pixel format attribute  */
	kCGLBadProperty        = 10001,	/* invalid renderer property       */
	kCGLBadPixelFormat     = 10002,	/* invalid pixel format            */
	kCGLBadRendererInfo    = 10003,	/* invalid renderer info           */
	kCGLBadContext         = 10004,	/* invalid context                 */
	kCGLBadDrawable        = 10005,	/* invalid drawable                */
	kCGLBadDisplay         = 10006,	/* invalid graphics device         */
	kCGLBadState           = 10007,	/* invalid context state           */
	kCGLBadValue           = 10008,	/* invalid numerical value         */
	kCGLBadMatch           = 10009,	/* invalid share context           */
	kCGLBadEnumeration     = 10010,	/* invalid enumerant               */
	kCGLBadOffScreen       = 10011,	/* invalid offscreen drawable      */
	kCGLBadFullScreen      = 10012,	/* invalid fullscreen drawable     */
	kCGLBadWindow          = 10013,	/* invalid window                  */
	kCGLBadAddress         = 10014,	/* invalid pointer                 */
	kCGLBadCodeModule      = 10015,	/* invalid code module             */
	kCGLBadAlloc           = 10016,	/* invalid memory allocation       */
	kCGLBadConnection      = 10017 	/* invalid CoreGraphics connection */
} CGLError;


/* 
** Buffer modes
*/
#define kCGLMonoscopicBit   0x00000001
#define kCGLStereoscopicBit 0x00000002
#define kCGLSingleBufferBit 0x00000004
#define kCGLDoubleBufferBit 0x00000008
#define kCGLTripleBufferBit 0x00000010

/*
** Depth and stencil buffer depths
*/
#define kCGL0Bit            0x00000001
#define kCGL1Bit            0x00000002
#define kCGL2Bit            0x00000004
#define kCGL3Bit            0x00000008
#define kCGL4Bit            0x00000010
#define kCGL5Bit            0x00000020
#define kCGL6Bit            0x00000040
#define kCGL8Bit            0x00000080
#define kCGL10Bit           0x00000100
#define kCGL12Bit           0x00000200
#define kCGL16Bit           0x00000400
#define kCGL24Bit           0x00000800
#define kCGL32Bit           0x00001000
#define kCGL48Bit           0x00002000
#define kCGL64Bit           0x00004000
#define kCGL96Bit           0x00008000
#define kCGL128Bit          0x00010000

/*
** Color and accumulation buffer formats.
*/
#define kCGLRGB444Bit       0x00000040  /* 16 rgb bit/pixel,    R=11:8, G=7:4, B=3:0              */
#define kCGLARGB4444Bit     0x00000080  /* 16 argb bit/pixel,   A=15:12, R=11:8, G=7:4, B=3:0     */
#define kCGLRGB444A8Bit     0x00000100  /* 8-16 argb bit/pixel, A=7:0, R=11:8, G=7:4, B=3:0       */
#define kCGLRGB555Bit       0x00000200  /* 16 rgb bit/pixel,    R=14:10, G=9:5, B=4:0             */
#define kCGLARGB1555Bit     0x00000400  /* 16 argb bit/pixel,   A=15, R=14:10, G=9:5, B=4:0       */
#define kCGLRGB555A8Bit     0x00000800  /* 8-16 argb bit/pixel, A=7:0, R=14:10, G=9:5, B=4:0      */
#define kCGLRGB565Bit       0x00001000  /* 16 rgb bit/pixel,    R=15:11, G=10:5, B=4:0            */
#define kCGLRGB565A8Bit     0x00002000  /* 8-16 argb bit/pixel, A=7:0, R=15:11, G=10:5, B=4:0     */
#define kCGLRGB888Bit       0x00004000  /* 32 rgb bit/pixel,    R=23:16, G=15:8, B=7:0            */
#define kCGLARGB8888Bit     0x00008000  /* 32 argb bit/pixel,   A=31:24, R=23:16, G=15:8, B=7:0   */
#define kCGLRGB888A8Bit     0x00010000  /* 8-32 argb bit/pixel, A=7:0, R=23:16, G=15:8, B=7:0     */
#define kCGLRGB101010Bit    0x00020000  /* 32 rgb bit/pixel,    R=29:20, G=19:10, B=9:0           */
#define kCGLARGB2101010Bit  0x00040000  /* 32 argb bit/pixel,   A=31:30  R=29:20, G=19:10, B=9:0  */
#define kCGLRGB101010_A8Bit 0x00080000  /* 8-32 argb bit/pixel, A=7:0  R=29:20, G=19:10, B=9:0    */
#define kCGLRGB121212Bit    0x00100000  /* 48 rgb bit/pixel,    R=35:24, G=23:12, B=11:0          */
#define kCGLARGB12121212Bit 0x00200000  /* 48 argb bit/pixel,   A=47:36, R=35:24, G=23:12, B=11:0 */
#define kCGLRGB161616Bit    0x00400000  /* 64 rgb bit/pixel,    R=63:48, G=47:32, B=31:16         */
#define kCGLRGBA16161616Bit 0x00800000  /* 64 argb bit/pixel,   R=63:48, G=47:32, B=31:16, A=15:0 */
#define kCGLRGBFloat64Bit   0x01000000  /* 64 rgb bit/pixel,    half float                        */
#define kCGLRGBAFloat64Bit  0x02000000  /* 64 argb bit/pixel,   half float                        */
#define kCGLRGBFloat128Bit  0x04000000  /* 128 rgb bit/pixel,   ieee float                        */
#define kCGLRGBAFloat128Bit 0x08000000  /* 128 argb bit/pixel,  ieee float                        */
#define kCGLRGBFloat256Bit  0x10000000  /* 256 rgb bit/pixel,   ieee double                       */
#define kCGLRGBAFloat256Bit 0x20000000  /* 256 argb bit/pixel,  ieee double                       */

/*
** Sampling modes
*/
#define kCGLSupersampleBit 0x00000001
#define kCGLMultisampleBit 0x00000002

/* Obsolete */
#define kCGLARGB16161616Bit kCGLRGBA16161616Bit

/************************************************************************/
/* OpenGL.h                                                             */
/************************************************************************/

OPENGL_ASSUME_NONNULL_BEGIN

/*
** CGL API version.
*/
#define CGL_VERSION_1_0  1
#define CGL_VERSION_1_1  1
#define CGL_VERSION_1_2  1
#define CGL_VERSION_1_3  1


/*
** Pixel format functions
*/
extern CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs, CGLPixelFormatObj OPENGL_NULLABLE * OPENGL_NONNULL pix, GLint *npix);
extern CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix);
extern CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, GLint pix_num, CGLPixelFormatAttribute attrib, GLint *value);
extern void CGLReleasePixelFormat(CGLPixelFormatObj pix);
extern CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix); 
extern GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix);

/*
** Renderer information functions
*/
extern CGLError CGLQueryRendererInfo(GLuint display_mask, CGLRendererInfoObj OPENGL_NULLABLE * OPENGL_NONNULL rend, GLint *nrend);
extern CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend);
extern CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, GLint rend_num, CGLRendererProperty prop, GLint * OPENGL_NULLABLE value);

/*
** Context functions
*/
extern CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj OPENGL_NULLABLE share, CGLContextObj OPENGL_NULLABLE * OPENGL_NONNULL ctx);
extern CGLError CGLDestroyContext(CGLContextObj ctx);
extern CGLError CGLCopyContext(CGLContextObj src, CGLContextObj dst, GLbitfield mask);
extern CGLContextObj CGLRetainContext(CGLContextObj ctx);
extern void CGLReleaseContext(CGLContextObj ctx);
extern GLuint CGLGetContextRetainCount(CGLContextObj ctx);
extern CGLPixelFormatObj OPENGL_NULLABLE CGLGetPixelFormat(CGLContextObj ctx);

/*
** PBuffer functions
*/
extern CGLError CGLCreatePBuffer(GLsizei width, GLsizei height, GLenum target, GLenum internalFormat, GLint max_level, CGLPBufferObj OPENGL_NULLABLE * OPENGL_NONNULL pbuffer);
extern CGLError CGLDestroyPBuffer(CGLPBufferObj pbuffer);
extern CGLError CGLDescribePBuffer(CGLPBufferObj obj, GLsizei *width, GLsizei *height, GLenum *target, GLenum *internalFormat, GLint *mipmap);
extern CGLError CGLTexImagePBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum source);
extern CGLPBufferObj CGLRetainPBuffer(CGLPBufferObj pbuffer);
extern void CGLReleasePBuffer(CGLPBufferObj pbuffer);
extern GLuint CGLGetPBufferRetainCount(CGLPBufferObj pbuffer);

/*
** Drawable Functions
*/
extern CGLError CGLSetOffScreen(CGLContextObj ctx, GLsizei width, GLsizei height, GLint rowbytes, void *baseaddr);
extern CGLError CGLGetOffScreen(CGLContextObj ctx, GLsizei *width, GLsizei *height, GLint *rowbytes, void * OPENGL_NULLABLE * OPENGL_NONNULL baseaddr);
extern CGLError CGLSetFullScreen(CGLContextObj ctx);
extern CGLError CGLSetFullScreenOnDisplay(CGLContextObj ctx, GLuint display_mask);

extern CGLError CGLSetPBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum face, GLint level, GLint screen);
extern CGLError CGLGetPBuffer(CGLContextObj ctx, CGLPBufferObj OPENGL_NULLABLE * OPENGL_NONNULL pbuffer, GLenum *face, GLint *level, GLint *screen);

extern CGLError CGLClearDrawable(CGLContextObj ctx);
extern CGLError CGLFlushDrawable(CGLContextObj ctx);

/*
** Per context enables and parameters
*/
extern CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname);
extern CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname);
extern CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable);
extern CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter pname, const GLint *params);
extern CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter pname, GLint *params);

/*
** Virtual screen functions
*/
extern CGLError CGLSetVirtualScreen(CGLContextObj ctx, GLint screen);
extern CGLError CGLGetVirtualScreen(CGLContextObj ctx, GLint *screen);

extern CGLError CGLUpdateContext(CGLContextObj ctx);

/*
** Global library options
*/
extern CGLError CGLSetGlobalOption(CGLGlobalOption pname, const GLint * OPENGL_NULLABLE params);
extern CGLError CGLGetGlobalOption(CGLGlobalOption pname, GLint *params);

extern CGLError CGLSetOption(CGLGlobalOption pname, GLint param) ;  /* Use CGLSetGlobalOption */
extern CGLError CGLGetOption(CGLGlobalOption pname, GLint *param); /* Use CGLGetGlobalOption */

/*
** Locking functions
*/
extern CGLError CGLLockContext(CGLContextObj ctx);

extern CGLError CGLUnlockContext(CGLContextObj ctx);

/*
** Version numbers
*/
extern void CGLGetVersion(GLint * OPENGL_NULLABLE majorvers, GLint * OPENGL_NULLABLE minorvers);

/*
** Convert an error code to a string
*/
const char *CGLErrorString(CGLError error);

OPENGL_ASSUME_NONNULL_END

/************************************************************************/
/* CGLCurrent.h                                                         */
/************************************************************************/

OPENGL_ASSUME_NONNULL_BEGIN

/*
** Current context functions
*/
extern CGLError CGLSetCurrentContext(CGLContextObj OPENGL_NULLABLE ctx);
extern CGLContextObj OPENGL_NULLABLE CGLGetCurrentContext(void);

OPENGL_ASSUME_NONNULL_END

/************************************************************************/
/* CoreGraphics.framework/Headers/CGGeometry.h                          */
/************************************************************************/

#if defined(__LP64__) && __LP64__
#define CGFLOAT_TYPE double
#else
#define CGFLOAT_TYPE float
#endif

typedef CGFLOAT_TYPE CGFloat;

struct
CGPoint {
    CGFloat x;
    CGFloat y;
};
typedef struct CGPoint CGPoint;

struct
CGSize {
    CGFloat width;
    CGFloat height;
};
typedef struct CGSize CGSize;

struct
CGRect {
    CGPoint origin;
    CGSize size;
};
typedef struct CGRect CGRect;

/************************************************************************/
/* Undocumented/internal functions                                      */
/************************************************************************/

typedef int CGSConnectionID;
typedef int CGSWindowID;
typedef int CGSSurfaceID;

extern CGLError CGLSetSurface(CGLContextObj OPENGL_NULLABLE gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);
extern CGLError CGLGetSurface(CGLContextObj OPENGL_NULLABLE ctx, CGSConnectionID * OPENGL_NULLABLE cid, CGSWindowID * OPENGL_NULLABLE wid, CGSSurfaceID * OPENGL_NULLABLE sid);
extern CGLError CGSSetSurfaceBounds(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, CGRect rect);
extern CGLError CGSGetSurfaceBounds(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid, CGRect * OPENGL_NULLABLE rect);

#ifdef __cplusplus
}
#endif
