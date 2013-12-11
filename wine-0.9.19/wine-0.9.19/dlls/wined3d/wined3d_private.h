/*
 * Direct3D wine internal private include file
 *
 * Copyright 2002-2003 The wine-d3d team
 * Copyright 2002-2003 Raphael Junqueira
 * Copyright 2004 Jason Edmeades
 * Copyright 2005 Oliver Stieber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_WINED3D_PRIVATE_H
#define __WINE_WINED3D_PRIVATE_H

#include <stdarg.h>
#include <math.h>
#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/debug.h"
#include "wine/unicode.h"

#include "d3d9.h"
#include "d3d9types.h"
#include "ddraw.h"
#include "wine/wined3d_interface.h"
#include "wine/wined3d_gl.h"
#include "wine/list.h"

/* Device caps */
#define MAX_PALETTES      256
#define MAX_STREAMS       16
#define MAX_TEXTURES      8
#define MAX_SAMPLERS      16
#define MAX_ACTIVE_LIGHTS 8
#define MAX_CLIPPLANES    D3DMAXUSERCLIPPLANES
#define MAX_LEVELS        256

#define MAX_CONST_I 16
#define MAX_CONST_B 16

/* Used for CreateStateBlock */
#define NUM_SAVEDPIXELSTATES_R     35
#define NUM_SAVEDPIXELSTATES_T     18
#define NUM_SAVEDPIXELSTATES_S     12
#define NUM_SAVEDVERTEXSTATES_R    31
#define NUM_SAVEDVERTEXSTATES_T    2
#define NUM_SAVEDVERTEXSTATES_S    1

extern const DWORD SavedPixelStates_R[NUM_SAVEDPIXELSTATES_R];
extern const DWORD SavedPixelStates_T[NUM_SAVEDPIXELSTATES_T];
extern const DWORD SavedPixelStates_S[NUM_SAVEDPIXELSTATES_S];
extern const DWORD SavedVertexStates_R[NUM_SAVEDVERTEXSTATES_R];
extern const DWORD SavedVertexStates_T[NUM_SAVEDVERTEXSTATES_T];
extern const DWORD SavedVertexStates_S[NUM_SAVEDVERTEXSTATES_S];

typedef enum _WINELOOKUP {
    WINELOOKUP_WARPPARAM = 0,
    WINELOOKUP_MAGFILTER = 1,
    MAX_LOOKUPS          = 2
} WINELOOKUP;

extern int minLookup[MAX_LOOKUPS];
extern int maxLookup[MAX_LOOKUPS];
extern DWORD *stateLookup[MAX_LOOKUPS];

extern DWORD minMipLookup[D3DTEXF_ANISOTROPIC + 1][D3DTEXF_LINEAR + 1];

typedef struct _WINED3DGLTYPE {
    int         d3dType;
    GLint       size;
    GLenum      glType;
    GLboolean   normalized;
    int         typesize;
} WINED3DGLTYPE;

/* NOTE: Make sure these are in the correct numerical order. (see /include/d3d9types.h typedef enum _D3DDECLTYPE) */
static WINED3DGLTYPE const glTypeLookup[D3DDECLTYPE_UNUSED] = {
                                  {D3DDECLTYPE_FLOAT1,    1, GL_FLOAT           , GL_FALSE ,sizeof(float)},
                                  {D3DDECLTYPE_FLOAT2,    2, GL_FLOAT           , GL_FALSE ,sizeof(float)},
                                  {D3DDECLTYPE_FLOAT3,    3, GL_FLOAT           , GL_FALSE ,sizeof(float)},
                                  {D3DDECLTYPE_FLOAT4,    4, GL_FLOAT           , GL_FALSE ,sizeof(float)},
                                  {D3DDECLTYPE_D3DCOLOR,  4, GL_UNSIGNED_BYTE   , GL_TRUE  ,sizeof(BYTE)},
                                  {D3DDECLTYPE_UBYTE4,    4, GL_UNSIGNED_BYTE   , GL_FALSE ,sizeof(BYTE)},
                                  {D3DDECLTYPE_SHORT2,    2, GL_SHORT           , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_SHORT4,    4, GL_SHORT           , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_UBYTE4N,   4, GL_UNSIGNED_BYTE   , GL_FALSE ,sizeof(BYTE)},
                                  {D3DDECLTYPE_SHORT2N,   2, GL_SHORT           , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_SHORT4N,   4, GL_SHORT           , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_USHORT2N,  2, GL_UNSIGNED_SHORT  , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_USHORT4N,  4, GL_UNSIGNED_SHORT  , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_UDEC3,     3, GL_UNSIGNED_SHORT  , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_DEC3N,     3, GL_SHORT           , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_FLOAT16_2, 2, GL_FLOAT           , GL_FALSE ,sizeof(short int)},
                                  {D3DDECLTYPE_FLOAT16_4, 4, GL_FLOAT           , GL_FALSE ,sizeof(short int)}};

#define WINED3D_ATR_TYPE(type)          glTypeLookup[type].d3dType
#define WINED3D_ATR_SIZE(type)          glTypeLookup[type].size
#define WINED3D_ATR_GLTYPE(type)        glTypeLookup[type].glType
#define WINED3D_ATR_NORMALIZED(type)    glTypeLookup[type].normalized
#define WINED3D_ATR_TYPESIZE(type)      glTypeLookup[type].typesize

/**
 * Settings 
 */
#define VS_NONE    0
#define VS_HW      1
#define VS_SW      2

#define PS_NONE    0
#define PS_HW      1

#define VBO_NONE   0
#define VBO_HW     1

#define NP2_NONE   0
#define NP2_REPACK 1

#define SHADER_SW   0
#define SHADER_ARB  1
#define SHADER_GLSL 2
#define SHADER_NONE 3

#define RTL_DISABLE   -1
#define RTL_AUTO       0
#define RTL_READDRAW   1
#define RTL_READTEX    2
#define RTL_TEXDRAW    3
#define RTL_TEXTEX     4

typedef struct wined3d_settings_s {
/* vertex and pixel shader modes */
  int vs_mode;
  int ps_mode;
  int vbo_mode;
/* Ideally, we don't want the user to have to request GLSL.  If the hardware supports GLSL,
    we should use it.  However, until it's fully implemented, we'll leave it as a registry
    setting for developers. */
  BOOL glslRequested;
  int vs_selected_mode;
  int ps_selected_mode;
/* nonpower 2 function */
  int nonpower2_mode;
  int rendertargetlock_mode;
/* Memory tracking and object counting */
  unsigned int emulated_textureram;
} wined3d_settings_t;

extern wined3d_settings_t wined3d_settings;

/* X11 locking */

extern void (*wine_tsx11_lock_ptr)(void);
extern void (*wine_tsx11_unlock_ptr)(void);

/* As GLX relies on X, this is needed */
extern int num_lock;

#if 0
#define ENTER_GL() ++num_lock; if (num_lock > 1) FIXME("Recursive use of GL lock to: %d\n", num_lock); wine_tsx11_lock_ptr()
#define LEAVE_GL() if (num_lock != 1) FIXME("Recursive use of GL lock: %d\n", num_lock); --num_lock; wine_tsx11_unlock_ptr()
#else
#define ENTER_GL() wine_tsx11_lock_ptr()
#define LEAVE_GL() wine_tsx11_unlock_ptr()
#endif

/*****************************************************************************
 * Defines
 */

/* GL related defines */
/* ------------------ */
#define GL_SUPPORT(ExtName)           (GLINFO_LOCATION.supported[ExtName] != 0)
#define GL_LIMITS(ExtName)            (GLINFO_LOCATION.max_##ExtName)
#define GL_EXTCALL(FuncName)          (GLINFO_LOCATION.FuncName)
#define GL_VEND(_VendName)            (GLINFO_LOCATION.gl_vendor == VENDOR_##_VendName ? TRUE : FALSE)

#define D3DCOLOR_B_R(dw) (((dw) >> 16) & 0xFF)
#define D3DCOLOR_B_G(dw) (((dw) >>  8) & 0xFF)
#define D3DCOLOR_B_B(dw) (((dw) >>  0) & 0xFF)
#define D3DCOLOR_B_A(dw) (((dw) >> 24) & 0xFF)

#define D3DCOLOR_R(dw) (((float) (((dw) >> 16) & 0xFF)) / 255.0f)
#define D3DCOLOR_G(dw) (((float) (((dw) >>  8) & 0xFF)) / 255.0f)
#define D3DCOLOR_B(dw) (((float) (((dw) >>  0) & 0xFF)) / 255.0f)
#define D3DCOLOR_A(dw) (((float) (((dw) >> 24) & 0xFF)) / 255.0f)

#define D3DCOLORTOGLFLOAT4(dw, vec) \
  (vec)[0] = D3DCOLOR_R(dw); \
  (vec)[1] = D3DCOLOR_G(dw); \
  (vec)[2] = D3DCOLOR_B(dw); \
  (vec)[3] = D3DCOLOR_A(dw);

#define GLTEXTURECUBEMAP GL_TEXTURE_CUBE_MAP_ARB

/* DirectX Device Limits */
/* --------------------- */
#define MAX_LEVELS  256  /* Maximum number of mipmap levels. Guessed at 256 */

#define MAX_STREAMS  16  /* Maximum possible streams - used for fixed size arrays
                            See MaxStreams in MSDN under GetDeviceCaps */
                         /* Maximum number of constants provided to the shaders */
#define HIGHEST_TRANSFORMSTATE 512 
                         /* Highest value in D3DTRANSFORMSTATETYPE */
#define MAX_CLIPPLANES  D3DMAXUSERCLIPPLANES

#define MAX_PALETTES      256

/* Checking of API calls */
/* --------------------- */
#define checkGLcall(A)                                          \
{                                                               \
    GLint err = glGetError();                                   \
    if (err == GL_NO_ERROR) {                                   \
       TRACE("%s call ok %s / %d\n", A, __FILE__, __LINE__);    \
                                                                \
    } else do {                                                 \
       FIXME(">>>>>>>>>>>>>>>>> %x from %s @ %s / %d\n",        \
		err, A, __FILE__, __LINE__);                    \
       err = glGetError();                                      \
    } while (err != GL_NO_ERROR);                               \
} 

/* Trace routines / diagnostics */
/* ---------------------------- */

/* Dump out a matrix and copy it */
#define conv_mat(mat,gl_mat)                                                                \
do {                                                                                        \
    TRACE("%f %f %f %f\n", (mat)->u.s._11, (mat)->u.s._12, (mat)->u.s._13, (mat)->u.s._14); \
    TRACE("%f %f %f %f\n", (mat)->u.s._21, (mat)->u.s._22, (mat)->u.s._23, (mat)->u.s._24); \
    TRACE("%f %f %f %f\n", (mat)->u.s._31, (mat)->u.s._32, (mat)->u.s._33, (mat)->u.s._34); \
    TRACE("%f %f %f %f\n", (mat)->u.s._41, (mat)->u.s._42, (mat)->u.s._43, (mat)->u.s._44); \
    memcpy(gl_mat, (mat), 16 * sizeof(float));                                              \
} while (0)

/* Macro to dump out the current state of the light chain */
#define DUMP_LIGHT_CHAIN()                    \
{                                             \
  PLIGHTINFOEL *el = This->stateBlock->lights;\
  while (el) {                                \
    TRACE("Light %p (glIndex %ld, d3dIndex %ld, enabled %d)\n", el, el->glIndex, el->OriginalIndex, el->lightEnabled);\
    el = el->next;                            \
  }                                           \
}

/* Trace vector and strided data information */
#define TRACE_VECTOR(name) TRACE( #name "=(%f, %f, %f, %f)\n", name.x, name.y, name.z, name.w);
#define TRACE_STRIDED(sd,name) TRACE( #name "=(data:%p, stride:%ld, type:%ld)\n", sd->u.s.name.lpData, sd->u.s.name.dwStride, sd->u.s.name.dwType);

/* Defines used for optimizations */

/*    Only reapply what is necessary */
#define REAPPLY_ALPHAOP  0x0001
#define REAPPLY_ALL      0xFFFF

/* Advance declaration of structures to satisfy compiler */
typedef struct IWineD3DStateBlockImpl IWineD3DStateBlockImpl;
typedef struct IWineD3DSurfaceImpl    IWineD3DSurfaceImpl;
typedef struct IWineD3DPaletteImpl    IWineD3DPaletteImpl;

/* Tracking */

/* TODO: Move some of this to the device */
long globalChangeGlRam(long glram);

/* Memory and object tracking */

/*Structure for holding information on all direct3d objects
useful for making sure tracking is ok and when release is called on a device!
and probably quite handy for debugging and dumping states out
*/
typedef struct WineD3DGlobalStatistics {
    int glsurfaceram; /* The aproximate amount of glTexture memory allocated for textures */
} WineD3DGlobalStatistics;

extern WineD3DGlobalStatistics* wineD3DGlobalStatistics;

/* Global variables */
extern const float identity[16];

/*****************************************************************************
 * Compilable extra diagnostics
 */

/* Trace information per-vertex: (extremely high amount of trace) */
#if 0 /* NOTE: Must be 0 in cvs */
# define VTRACE(A) TRACE A
#else 
# define VTRACE(A) 
#endif

/* Checking of per-vertex related GL calls */
/* --------------------- */
#define vcheckGLcall(A)                                         \
{                                                               \
    GLint err = glGetError();                                   \
    if (err == GL_NO_ERROR) {                                   \
       VTRACE(("%s call ok %s / %d\n", A, __FILE__, __LINE__)); \
                                                                \
    } else do {                                                 \
       FIXME(">>>>>>>>>>>>>>>>> %x from %s @ %s / %d\n",        \
                err, A, __FILE__, __LINE__);                    \
       err = glGetError();                                      \
    } while (err != GL_NO_ERROR);                               \
}

/* TODO: Confirm each of these works when wined3d move completed */
#if 0 /* NOTE: Must be 0 in cvs */
  /* To avoid having to get gigabytes of trace, the following can be compiled in, and at the start
     of each frame, a check is made for the existence of C:\D3DTRACE, and if if exists d3d trace
     is enabled, and if it doesn't exists it is disabled.                                           */
# define FRAME_DEBUGGING
  /*  Adding in the SINGLE_FRAME_DEBUGGING gives a trace of just what makes up a single frame, before
      the file is deleted                                                                            */
# if 1 /* NOTE: Must be 1 in cvs, as this is mostly more useful than a trace from program start */
#  define SINGLE_FRAME_DEBUGGING
# endif  
  /* The following, when enabled, lets you see the makeup of the frame, by drawprimitive calls.
     It can only be enabled when FRAME_DEBUGGING is also enabled                               
     The contents of the back buffer are written into /tmp/backbuffer_* after each primitive 
     array is drawn.                                                                            */
# if 0 /* NOTE: Must be 0 in cvs, as this give a lot of ppm files when compiled in */                                                                                       
#  define SHOW_FRAME_MAKEUP 1
# endif  
  /* The following, when enabled, lets you see the makeup of the all the textures used during each
     of the drawprimitive calls. It can only be enabled when SHOW_FRAME_MAKEUP is also enabled.
     The contents of the textures assigned to each stage are written into 
     /tmp/texture_*_<Stage>.ppm after each primitive array is drawn.                            */
# if 0 /* NOTE: Must be 0 in cvs, as this give a lot of ppm files when compiled in */
#  define SHOW_TEXTURE_MAKEUP 0
# endif  
extern BOOL isOn;
extern BOOL isDumpingFrames;
extern LONG primCounter;
#endif

/*****************************************************************************
 * Prototypes
 */

/* Routine common to the draw primitive and draw indexed primitive routines */
void drawPrimitive(IWineD3DDevice *iface,
                    int PrimitiveType,
                    long NumPrimitives,
                    /* for Indexed: */
                    long  StartVertexIndex,
                    UINT  numberOfVertices,
                    long  StartIdx,
                    short idxBytes,
                    const void *idxData,
                    int   minIndex,
                    WineDirect3DVertexStridedData *DrawPrimStrideData);

void primitiveConvertToStridedData(IWineD3DDevice *iface, WineDirect3DVertexStridedData *strided, LONG BaseVertexIndex, BOOL *fixup);

void primitiveDeclarationConvertToStridedData(
     IWineD3DDevice *iface,
     BOOL useVertexShaderFunction,
     WineDirect3DVertexStridedData *strided,
     LONG BaseVertexIndex, 
     BOOL *fixup);

void primitiveConvertFVFtoOffset(DWORD thisFVF,
                                 DWORD stride,
                                 BYTE *data,
                                 WineDirect3DVertexStridedData *strided,
                                 GLint streamVBO);

DWORD get_flexible_vertex_size(DWORD d3dvtVertexType);

#define eps 1e-8

#define GET_TEXCOORD_SIZE_FROM_FVF(d3dvtVertexType, tex_num) \
    (((((d3dvtVertexType) >> (16 + (2 * (tex_num)))) + 1) & 0x03) + 1)

/* Routine to fill gl caps for swapchains and IWineD3D */
BOOL IWineD3DImpl_FillGLCaps(IWineD3D *iface, Display* display);

/*****************************************************************************
 * Internal representation of a light
 */
typedef struct PLIGHTINFOEL PLIGHTINFOEL;
struct PLIGHTINFOEL {
    WINED3DLIGHT OriginalParms; /* Note D3D8LIGHT == D3D9LIGHT */
    DWORD        OriginalIndex;
    LONG         glIndex;
    BOOL         lightEnabled;
    BOOL         changed;
    BOOL         enabledChanged;

    /* Converted parms to speed up swapping lights */
    float                         lightPosn[4];
    float                         lightDirn[4];
    float                         exponent;
    float                         cutoff;

    PLIGHTINFOEL *next;
    PLIGHTINFOEL *prev;
};

/* The default light parameters */
extern const WINED3DLIGHT WINED3D_default_light;

/*****************************************************************************
 * IWineD3D implementation structure
 */
typedef struct IWineD3DImpl
{
    /* IUnknown fields */
    const IWineD3DVtbl     *lpVtbl;
    LONG                    ref;     /* Note: Ref counting not required */

    /* WineD3D Information */
    IUnknown               *parent;
    UINT                    dxVersion;

    /* GL Information */
    BOOL                    isGLInfoValid;
    WineD3D_GL_Info         gl_info;
} IWineD3DImpl;

extern const IWineD3DVtbl IWineD3D_Vtbl;

/** Hacked out start of a context manager!! **/
typedef struct glContext {
    int Width;
    int Height;
    int usedcount;
    GLXContext context;

    Drawable drawable;
    IWineD3DSurface *pSurface;
#if 0 /* TODO: someway to represent the state of the context */
    IWineD3DStateBlock *pStateBlock;
#endif
/* a few other things like format */
} glContext;

/* TODO: setup some flags in the regestry to enable, disable pbuffer support
(since it will break quite a few things until contexts are managed properly!) */
extern BOOL pbuffer_support;
/* allocate one pbuffer per surface */
extern BOOL pbuffer_per_surface;

/* Maximum number of contexts/pbuffers to keep in cache,
set to 100 because ATI's drivers don't support deleting pBuffers properly
this needs to be migrated to a list and some option availalbe for controle the cache size.
*/
#define CONTEXT_CACHE 100

typedef struct ResourceList {
    IWineD3DResource         *resource;
    struct ResourceList      *next;
} ResourceList;

/* A helper function that dumps a resource list */
void dumpResources(ResourceList *resources);

/*****************************************************************************
 * IWineD3DDevice implementation structure
 */
typedef struct IWineD3DDeviceImpl
{
    /* IUnknown fields      */
    const IWineD3DDeviceVtbl *lpVtbl;
    LONG                    ref;     /* Note: Ref counting not required */

    /* WineD3D Information  */
    IUnknown               *parent;
    IWineD3D               *wineD3D;

    /* X and GL Information */
    GLint                   maxConcurrentLights;

    /* Optimization */
    BOOL                    modelview_valid;
    BOOL                    proj_valid;
    BOOL                    view_ident;        /* true iff view matrix is identity                */
    BOOL                    last_was_rhw;      /* true iff last draw_primitive was in xyzrhw mode */
    BOOL                    viewport_changed;  /* Was the viewport changed since the last draw?   */
    GLenum                  tracking_parm;     /* Which source is tracking current colour         */
    LONG                    tracking_color;    /* used iff GL_COLOR_MATERIAL was enabled          */
#define                         DISABLED_TRACKING  0  /* Disabled                                 */
#define                         IS_TRACKING        1  /* tracking_parm is tracking diffuse color  */
#define                         NEEDS_TRACKING     2  /* Tracking needs to be enabled when needed */
#define                         NEEDS_DISABLE      3  /* Tracking needs to be disabled when needed*/
    UINT                    srcBlend;
    UINT                    dstBlend;
    UINT                    alphafunc;
    BOOL                    texture_shader_active;  /* TODO: Confirm use is correct */
    BOOL                    last_was_notclipped;

    /* State block related */
    BOOL                    isRecordingState;
    IWineD3DStateBlockImpl *stateBlock;
    IWineD3DStateBlockImpl *updateStateBlock;

    /* Internal use fields  */
    WINED3DDEVICE_CREATION_PARAMETERS createParms;
    UINT                            adapterNo;
    D3DDEVTYPE                      devType;

    IWineD3DSwapChain     **swapchains;
    uint                    NumberOfSwapChains;

    ResourceList           *resources; /* a linked list to track resources created by the device */

    /* Render Target Support */
    IWineD3DSurface        *depthStencilBuffer;

    IWineD3DSurface        *renderTarget;
    IWineD3DSurface        *stencilBufferTarget;

    /* palettes texture management */
    PALETTEENTRY            palettes[MAX_PALETTES][256];
    UINT                    currentPalette;

    /* For rendering to a texture using glCopyTexImage */
    BOOL                    renderUpsideDown;

    /* Cursor management */
    BOOL                    bCursorVisible;
    UINT                    xHotSpot;
    UINT                    yHotSpot;
    UINT                    xScreenSpace;
    UINT                    yScreenSpace;
    UINT                    cursorWidth, cursorHeight;
    GLuint                  cursorTexture;

    /* Textures for when no other textures are mapped */
    UINT                          dummyTextureName[MAX_TEXTURES];

    /* Debug stream management */
    BOOL                     debug;

    /* Device state management */
    HRESULT                 state;
    BOOL                    d3d_initialized;

    /* Screen buffer resources */
    glContext contextCache[CONTEXT_CACHE];

    /* A flag to check if endscene has been called before changing the render tartet */
    BOOL sceneEnded;

    /* process vertex shaders using software or hardware */
    BOOL softwareVertexProcessing;

    /* DirectDraw stuff */
    HWND ddraw_window;
    IWineD3DSurface *ddraw_primary;
    DWORD ddraw_width, ddraw_height;
    WINED3DFORMAT ddraw_format;

    /* List of GLSL shader programs and their associated vertex & pixel shaders */
    struct list glsl_shader_progs;

} IWineD3DDeviceImpl;

extern const IWineD3DDeviceVtbl IWineD3DDevice_Vtbl;

/* Support for IWineD3DResource ::Set/Get/FreePrivateData. I don't think
 * anybody uses it for much so a good implementation is optional. */
typedef struct PrivateData
{
    struct PrivateData* next;

    GUID tag;
    DWORD flags; /* DDSPD_* */
    DWORD uniqueness_value;

    union
    {
        LPVOID data;
        LPUNKNOWN object;
    } ptr;

    DWORD size;
} PrivateData;

void d3ddevice_set_ortho(IWineD3DDeviceImpl *This);

/*****************************************************************************
 * IWineD3DResource implementation structure
 */
typedef struct IWineD3DResourceClass
{
    /* IUnknown fields */
    LONG                    ref;     /* Note: Ref counting not required */

    /* WineD3DResource Information */
    IUnknown               *parent;
    WINED3DRESOURCETYPE     resourceType;
    IWineD3DDeviceImpl     *wineD3DDevice;
    WINED3DPOOL             pool;
    UINT                    size;
    DWORD                   usage;
    WINED3DFORMAT           format;
    BYTE                   *allocatedMemory;
    PrivateData            *privateData;

} IWineD3DResourceClass;

typedef struct IWineD3DResourceImpl
{
    /* IUnknown & WineD3DResource Information     */
    const IWineD3DResourceVtbl *lpVtbl;
    IWineD3DResourceClass   resource;
} IWineD3DResourceImpl;


/*****************************************************************************
 * IWineD3DVertexBuffer implementation structure (extends IWineD3DResourceImpl)
 */
typedef struct IWineD3DVertexBufferImpl
{
    /* IUnknown & WineD3DResource Information     */
    const IWineD3DVertexBufferVtbl *lpVtbl;
    IWineD3DResourceClass     resource;

    /* WineD3DVertexBuffer specifics */
    DWORD                     fvf;

    /* Vertex buffer object support */
    GLuint                    vbo;
    BYTE                      Flags;
    UINT                      stream;

    UINT                      dirtystart, dirtyend;
    LONG                      lockcount;

    /* Last description of the buffer */
    WineDirect3DVertexStridedData strided;
} IWineD3DVertexBufferImpl;

extern const IWineD3DVertexBufferVtbl IWineD3DVertexBuffer_Vtbl;

#define VBFLAG_LOAD           0x01    /* Data is written from allocatedMemory to the VBO */
#define VBFLAG_OPTIMIZED      0x02    /* Optimize has been called for the VB */
#define VBFLAG_DIRTY          0x04    /* Buffer data has been modified */
#define VBFLAG_STREAM         0x08    /* The vertex buffer is in a stream */
#define VBFLAG_HASDESC        0x10    /* A vertex description has been found */
#define VBFLAG_VBOCREATEFAIL  0x20    /* An attempt to create a vbo has failed */

/*****************************************************************************
 * IWineD3DIndexBuffer implementation structure (extends IWineD3DResourceImpl)
 */
typedef struct IWineD3DIndexBufferImpl
{
    /* IUnknown & WineD3DResource Information     */
    const IWineD3DIndexBufferVtbl *lpVtbl;
    IWineD3DResourceClass     resource;

    /* WineD3DVertexBuffer specifics */
} IWineD3DIndexBufferImpl;

extern const IWineD3DIndexBufferVtbl IWineD3DIndexBuffer_Vtbl;

/*****************************************************************************
 * IWineD3DBaseTexture D3D- > openGL state map lookups
 */
#define WINED3DFUNC_NOTSUPPORTED  -2
#define WINED3DFUNC_UNIMPLEMENTED -1

typedef enum winetexturestates {
    WINED3DTEXSTA_ADDRESSU       = 0,
    WINED3DTEXSTA_ADDRESSV       = 1,
    WINED3DTEXSTA_ADDRESSW       = 2,
    WINED3DTEXSTA_BORDERCOLOR    = 3,
    WINED3DTEXSTA_MAGFILTER      = 4,
    WINED3DTEXSTA_MINFILTER      = 5,
    WINED3DTEXSTA_MIPFILTER      = 6,
    WINED3DTEXSTA_MAXMIPLEVEL    = 7,
    WINED3DTEXSTA_MAXANISOTROPY  = 8,
    WINED3DTEXSTA_SRGBTEXTURE    = 9,
    WINED3DTEXSTA_ELEMENTINDEX   = 10,
    WINED3DTEXSTA_DMAPOFFSET     = 11,
    WINED3DTEXSTA_TSSADDRESSW    = 12,
    MAX_WINETEXTURESTATES        = 13,
} winetexturestates;

typedef struct Wined3dTextureStateMap {
    CONST int state;
    int function;
} Wined3dTextureStateMap;

/*****************************************************************************
 * IWineD3DBaseTexture implementation structure (extends IWineD3DResourceImpl)
 */
typedef struct IWineD3DBaseTextureClass
{
    UINT                    levels;
    BOOL                    dirty;
    D3DFORMAT               format;
    DWORD                   usage;
    UINT                    textureName;
    UINT                    LOD;
    WINED3DTEXTUREFILTERTYPE filterType;
    DWORD                   states[MAX_WINETEXTURESTATES];

} IWineD3DBaseTextureClass;

typedef struct IWineD3DBaseTextureImpl
{
    /* IUnknown & WineD3DResource Information     */
    const IWineD3DBaseTextureVtbl *lpVtbl;
    IWineD3DResourceClass     resource;
    IWineD3DBaseTextureClass  baseTexture;

} IWineD3DBaseTextureImpl;

/*****************************************************************************
 * IWineD3DTexture implementation structure (extends IWineD3DBaseTextureImpl)
 */
typedef struct IWineD3DTextureImpl
{
    /* IUnknown & WineD3DResource/WineD3DBaseTexture Information     */
    const IWineD3DTextureVtbl *lpVtbl;
    IWineD3DResourceClass     resource;
    IWineD3DBaseTextureClass  baseTexture;

    /* IWineD3DTexture */
    IWineD3DSurface          *surfaces[MAX_LEVELS];
    
    UINT                      width;
    UINT                      height;
    float                     pow2scalingFactorX;
    float                     pow2scalingFactorY;

} IWineD3DTextureImpl;

extern const IWineD3DTextureVtbl IWineD3DTexture_Vtbl;

/*****************************************************************************
 * IWineD3DCubeTexture implementation structure (extends IWineD3DBaseTextureImpl)
 */
typedef struct IWineD3DCubeTextureImpl
{
    /* IUnknown & WineD3DResource/WineD3DBaseTexture Information     */
    const IWineD3DCubeTextureVtbl *lpVtbl;
    IWineD3DResourceClass     resource;
    IWineD3DBaseTextureClass  baseTexture;

    /* IWineD3DCubeTexture */
    IWineD3DSurface          *surfaces[6][MAX_LEVELS];

    UINT                      edgeLength;
    float                     pow2scalingFactor;

} IWineD3DCubeTextureImpl;

extern const IWineD3DCubeTextureVtbl IWineD3DCubeTexture_Vtbl;

typedef struct _WINED3DVOLUMET_DESC
{
    UINT                    Width;
    UINT                    Height;
    UINT                    Depth;
} WINED3DVOLUMET_DESC;

/*****************************************************************************
 * IWineD3DVolume implementation structure (extends IUnknown)
 */
typedef struct IWineD3DVolumeImpl
{
    /* IUnknown & WineD3DResource fields */
    const IWineD3DVolumeVtbl  *lpVtbl;
    IWineD3DResourceClass      resource;

    /* WineD3DVolume Information */
    WINED3DVOLUMET_DESC      currentDesc;
    IWineD3DBase            *container;
    UINT                    bytesPerPixel;

    BOOL                    lockable;
    BOOL                    locked;
    WINED3DBOX              lockedBox;
    WINED3DBOX              dirtyBox;
    BOOL                    dirty;


} IWineD3DVolumeImpl;

extern const IWineD3DVolumeVtbl IWineD3DVolume_Vtbl;

/*****************************************************************************
 * IWineD3DVolumeTexture implementation structure (extends IWineD3DBaseTextureImpl)
 */
typedef struct IWineD3DVolumeTextureImpl
{
    /* IUnknown & WineD3DResource/WineD3DBaseTexture Information     */
    const IWineD3DVolumeTextureVtbl *lpVtbl;
    IWineD3DResourceClass     resource;
    IWineD3DBaseTextureClass  baseTexture;

    /* IWineD3DVolumeTexture */
    IWineD3DVolume           *volumes[MAX_LEVELS];

    UINT                      width;
    UINT                      height;
    UINT                      depth;
} IWineD3DVolumeTextureImpl;

extern const IWineD3DVolumeTextureVtbl IWineD3DVolumeTexture_Vtbl;

typedef struct _WINED3DSURFACET_DESC
{
    WINED3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD                   MultiSampleQuality;
    UINT                    Width;
    UINT                    Height;
} WINED3DSURFACET_DESC;

/*****************************************************************************
 * Structure for DIB Surfaces (GetDC and GDI surfaces)
 */
typedef struct wineD3DSurface_DIB {
    HBITMAP DIBsection;
    void* bitmap_data;
    HGDIOBJ holdbitmap;
    BOOL client_memory;
} wineD3DSurface_DIB;

/*****************************************************************************
 * IWineD3DSurface implementation structure
 */
struct IWineD3DSurfaceImpl
{
    /* IUnknown & IWineD3DResource Information     */
    const IWineD3DSurfaceVtbl *lpVtbl;
    IWineD3DResourceClass     resource;

    /* IWineD3DSurface fields */
    IWineD3DBase              *container;
    WINED3DSURFACET_DESC      currentDesc;
    IWineD3DPaletteImpl      *palette;

    UINT                      textureName;
    UINT                      bytesPerPixel;

    /* TODO: move this off into a management class(maybe!) */
    DWORD                      Flags;

    UINT                      pow2Width;
    UINT                      pow2Height;
    UINT                      pow2Size;

    /* Oversized texture */
    RECT                      glRect;

#if 0
    /* precalculated x and y scalings for texture coords */
    float                     pow2scalingFactorX; /* =  (Width  / pow2Width ) */
    float                     pow2scalingFactorY; /* =  (Height / pow2Height) */
#endif

    RECT                      lockedRect;
    RECT                      dirtyRect;

    glDescriptor              glDescription;

    /* For GetDC */
    wineD3DSurface_DIB        dib;
    HDC                       hDC;

    /* Color keys for DDraw */
    DDCOLORKEY                DestBltCKey;
    DDCOLORKEY                DestOverlayCKey;
    DDCOLORKEY                SrcOverlayCKey;
    DDCOLORKEY                SrcBltCKey;
    DWORD                     CKeyFlags;

    DDCOLORKEY                glCKey;
};

extern const IWineD3DSurfaceVtbl IWineD3DSurface_Vtbl;
extern const IWineD3DSurfaceVtbl IWineGDISurface_Vtbl;

/* Predeclare the shared Surface functions */
HRESULT WINAPI IWineD3DSurfaceImpl_QueryInterface(IWineD3DSurface *iface, REFIID riid, LPVOID *ppobj);
ULONG WINAPI IWineD3DSurfaceImpl_AddRef(IWineD3DSurface *iface);
ULONG WINAPI IWineD3DSurfaceImpl_Release(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_GetParent(IWineD3DSurface *iface, IUnknown **pParent);
HRESULT WINAPI IWineD3DSurfaceImpl_GetDevice(IWineD3DSurface *iface, IWineD3DDevice** ppDevice);
HRESULT WINAPI IWineD3DSurfaceImpl_SetPrivateData(IWineD3DSurface *iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags);
HRESULT WINAPI IWineD3DSurfaceImpl_GetPrivateData(IWineD3DSurface *iface, REFGUID refguid, void* pData, DWORD* pSizeOfData);
HRESULT WINAPI IWineD3DSurfaceImpl_FreePrivateData(IWineD3DSurface *iface, REFGUID refguid);
DWORD   WINAPI IWineD3DSurfaceImpl_SetPriority(IWineD3DSurface *iface, DWORD PriorityNew);
DWORD   WINAPI IWineD3DSurfaceImpl_GetPriority(IWineD3DSurface *iface);
void    WINAPI IWineD3DSurfaceImpl_PreLoad(IWineD3DSurface *iface);
WINED3DRESOURCETYPE WINAPI IWineD3DSurfaceImpl_GetType(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_GetContainerParent(IWineD3DSurface* iface, IUnknown **ppContainerParent);
HRESULT WINAPI IWineD3DSurfaceImpl_GetContainer(IWineD3DSurface* iface, REFIID riid, void** ppContainer);
HRESULT WINAPI IWineD3DSurfaceImpl_GetDesc(IWineD3DSurface *iface, WINED3DSURFACE_DESC *pDesc);
HRESULT WINAPI IWineD3DSurfaceImpl_GetBltStatus(IWineD3DSurface *iface, DWORD Flags);
HRESULT WINAPI IWineD3DSurfaceImpl_GetFlipStatus(IWineD3DSurface *iface, DWORD Flags);
HRESULT WINAPI IWineD3DSurfaceImpl_IsLost(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_Restore(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_SetPixelFormat(IWineD3DSurface *iface, WINED3DFORMAT Format, BYTE *Surface, DWORD Size);
HRESULT WINAPI IWineD3DSurfaceImpl_GetPalette(IWineD3DSurface *iface, IWineD3DPalette **Pal);
HRESULT WINAPI IWineD3DSurfaceImpl_SetPalette(IWineD3DSurface *iface, IWineD3DPalette *Pal);
HRESULT WINAPI IWineD3DSurfaceImpl_SetColorKey(IWineD3DSurface *iface, DWORD Flags, DDCOLORKEY *CKey);
HRESULT WINAPI IWineD3DSurfaceImpl_CleanDirtyRect(IWineD3DSurface *iface);
extern HRESULT WINAPI IWineD3DSurfaceImpl_AddDirtyRect(IWineD3DSurface *iface, CONST RECT* pDirtyRect);
HRESULT WINAPI IWineD3DSurfaceImpl_SetContainer(IWineD3DSurface *iface, IWineD3DBase *container);
HRESULT WINAPI IWineD3DSurfaceImpl_SetPBufferState(IWineD3DSurface *iface, BOOL inPBuffer, BOOL  inTexture);
void WINAPI IWineD3DSurfaceImpl_SetGlTextureDesc(IWineD3DSurface *iface, UINT textureName, int target);
void WINAPI IWineD3DSurfaceImpl_GetGlDesc(IWineD3DSurface *iface, glDescriptor **glDescription);
const void *WINAPI IWineD3DSurfaceImpl_GetData(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_SetFormat(IWineD3DSurface *iface, WINED3DFORMAT format);
HRESULT WINAPI IWineGDISurfaceImpl_Blt(IWineD3DSurface *iface, RECT *DestRect, IWineD3DSurface *SrcSurface, RECT *SrcRect, DWORD Flags, DDBLTFX *DDBltFx);
HRESULT WINAPI IWineGDISurfaceImpl_BltFast(IWineD3DSurface *iface, DWORD dstx, DWORD dsty, IWineD3DSurface *Source, RECT *rsrc, DWORD trans);
HRESULT WINAPI IWineD3DSurfaceImpl_SetPalette(IWineD3DSurface *iface, IWineD3DPalette *Pal);
HRESULT WINAPI IWineD3DSurfaceImpl_GetDC(IWineD3DSurface *iface, HDC *pHDC);
HRESULT WINAPI IWineD3DSurfaceImpl_ReleaseDC(IWineD3DSurface *iface, HDC hDC);
DWORD WINAPI IWineD3DSurfaceImpl_GetPitch(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_RealizePalette(IWineD3DSurface *iface);
HRESULT WINAPI IWineD3DSurfaceImpl_SetMem(IWineD3DSurface *iface, void *Mem);

/* Surface flags: */
#define SFLAG_OVERSIZE    0x00000001 /* Surface is bigger than gl size, blts only */
#define SFLAG_CONVERTED   0x00000002 /* Converted for color keying or Palettized */
#define SFLAG_DIBSECTION  0x00000004 /* Has a DIB section attached for getdc */
#define SFLAG_DIRTY       0x00000008 /* Surface was locked by the app */
#define SFLAG_LOCKABLE    0x00000010 /* Surface can be locked */
#define SFLAG_DISCARD     0x00000020 /* ??? */
#define SFLAG_LOCKED      0x00000040 /* Surface is locked atm */
#define SFLAG_ACTIVELOCK  0x00000080 /* Not locked, but surface memory is needed */
#define SFLAG_INTEXTURE   0x00000100 /* ??? */
#define SFLAG_INPBUFFER   0x00000200 /* ??? */
#define SFLAG_NONPOW2     0x00000400 /* Surface sizes are not a power of 2 */
#define SFLAG_DYNLOCK     0x00000800 /* Surface is often locked by the app */
#define SFLAG_DYNCHANGE   0x00001800 /* Surface contents are changed very often, implies DYNLOCK */
#define SFLAG_DCINUSE     0x00002000 /* Set between GetDC and ReleaseDC calls */
#define SFLAG_GLDIRTY     0x00004000 /* The opengl texture is more up to date than the surface mem */
#define SFLAG_LOST        0x00008000 /* Surface lost flag for DDraw */
#define SFLAG_FORCELOAD   0x00010000 /* To force PreLoading of a scratch cursor */
#define SFLAG_USERPTR     0x00020000 /* The application allocated the memory for this surface */
#define SFLAG_GLCKEY      0x00040000 /* The gl texture was created with a color key */

/* In some conditions the surface memory must not be freed:
 * SFLAG_OVERSIZE: Not all data can be kept in GL
 * SFLAG_CONVERTED: Converting the data back would take too long
 * SFLAG_DIBSECTION: The dib code manages the memory
 * SFLAG_DIRTY: GL surface isn't up to date
 * SFLAG_LOCKED: The app requires access to the surface data
 * SFLAG_ACTIVELOCK: Some wined3d code needs the memory
 * SFLAG_DYNLOCK: Avoid freeing the data for performance
 * SFLAG_DYNCHANGE: Same reason as DYNLOCK
 */
#define SFLAG_DONOTFREE  (SFLAG_OVERSIZE   | \
                          SFLAG_CONVERTED  | \
                          SFLAG_DIBSECTION | \
                          SFLAG_DIRTY      | \
                          SFLAG_LOCKED     | \
                          SFLAG_ACTIVELOCK | \
                          SFLAG_DYNLOCK    | \
                          SFLAG_DYNCHANGE  | \
                          SFLAG_USERPTR)

BOOL CalculateTexRect(IWineD3DSurfaceImpl *This, RECT *Rect, float glTexCoord[4]);

/*****************************************************************************
 * IWineD3DVertexDeclaration implementation structure
 */
typedef struct IWineD3DVertexDeclarationImpl {
 /* IUnknown  Information     */
  const IWineD3DVertexDeclarationVtbl *lpVtbl;
  LONG                    ref;     /* Note: Ref counting not required */

  IUnknown               *parent;
  /** precomputed fvf if simple declaration */
  IWineD3DDeviceImpl     *wineD3DDevice;
  DWORD   fvf[MAX_STREAMS];
  DWORD   allFVF;

  /** dx8 compatible Declaration fields */
  DWORD*  pDeclaration8;
  DWORD   declaration8Length;

  /** dx9+ */
  D3DVERTEXELEMENT9 *pDeclaration9;
  UINT               declaration9NumElements;

  WINED3DVERTEXELEMENT  *pDeclarationWine;
  UINT                   declarationWNumElements;
  
  float                 *constants;
  
} IWineD3DVertexDeclarationImpl;

extern const IWineD3DVertexDeclarationVtbl IWineD3DVertexDeclaration_Vtbl;

/*****************************************************************************
 * IWineD3DStateBlock implementation structure
 */

/* Internal state Block for Begin/End/Capture/Create/Apply info  */
/*   Note: Very long winded but gl Lists are not flexible enough */
/*   to resolve everything we need, so doing it manually for now */
typedef struct SAVEDSTATES {
        BOOL                      indices;
        BOOL                      material;
        BOOL                      fvf;
        BOOL                      streamSource[MAX_STREAMS];
        BOOL                      streamFreq[MAX_STREAMS];
        BOOL                      textures[MAX_SAMPLERS];
        BOOL                      transform[HIGHEST_TRANSFORMSTATE + 1];
        BOOL                      viewport;
        BOOL                      renderState[WINEHIGHEST_RENDER_STATE + 1];
        BOOL                      textureState[MAX_TEXTURES][WINED3D_HIGHEST_TEXTURE_STATE + 1];
        BOOL                      samplerState[MAX_SAMPLERS][WINED3D_HIGHEST_SAMPLER_STATE + 1];
        BOOL                      clipplane[MAX_CLIPPLANES];
        BOOL                      vertexDecl;
        BOOL                      pixelShader;
        BOOL                      pixelShaderConstantsB[MAX_CONST_B];
        BOOL                      pixelShaderConstantsI[MAX_CONST_I];
        BOOL                     *pixelShaderConstantsF;
        BOOL                      vertexShader;
        BOOL                      vertexShaderConstantsB[MAX_CONST_B];
        BOOL                      vertexShaderConstantsI[MAX_CONST_I];
        BOOL                     *vertexShaderConstantsF;
} SAVEDSTATES;

struct IWineD3DStateBlockImpl
{
    /* IUnknown fields */
    const IWineD3DStateBlockVtbl *lpVtbl;
    LONG                      ref;     /* Note: Ref counting not required */

    /* IWineD3DStateBlock information */
    IUnknown                 *parent;
    IWineD3DDeviceImpl       *wineD3DDevice;
    WINED3DSTATEBLOCKTYPE     blockType;

    /* Array indicating whether things have been set or changed */
    SAVEDSTATES               changed;
    SAVEDSTATES               set;

    /* Drawing - Vertex Shader or FVF related */
    DWORD                     fvf;
    /* Vertex Shader Declaration */
    IWineD3DVertexDeclaration *vertexDecl;

    IWineD3DVertexShader      *vertexShader;

    /* Vertex Shader Constants */
    BOOL                       vertexShaderConstantB[MAX_CONST_B];
    INT                        vertexShaderConstantI[MAX_CONST_I * 4];
    float                     *vertexShaderConstantF;

    /* Stream Source */
    BOOL                      streamIsUP;
    UINT                      streamStride[MAX_STREAMS];
    UINT                      streamOffset[MAX_STREAMS];
    IWineD3DVertexBuffer     *streamSource[MAX_STREAMS];
    UINT                      streamFreq[MAX_STREAMS];
    UINT                      streamFlags[MAX_STREAMS];     /*0 | D3DSTREAMSOURCE_INSTANCEDATA | D3DSTREAMSOURCE_INDEXEDDATA  */

    /* Indices */
    IWineD3DIndexBuffer*      pIndexData;
    UINT                      baseVertexIndex; /* Note: only used for d3d8 */

    /* Transform */
    D3DMATRIX                 transforms[HIGHEST_TRANSFORMSTATE + 1];

    /* Lights */
    PLIGHTINFOEL             *lights; /* NOTE: active GL lights must be front of the chain */

    /* Clipping */
    double                    clipplane[MAX_CLIPPLANES][4];
    WINED3DCLIPSTATUS         clip_status;

    /* ViewPort */
    WINED3DVIEWPORT           viewport;

    /* Material */
    WINED3DMATERIAL           material;

    /* Pixel Shader */
    IWineD3DPixelShader      *pixelShader;

    /* Pixel Shader Constants */
    BOOL                       pixelShaderConstantB[MAX_CONST_B];
    INT                        pixelShaderConstantI[MAX_CONST_I * 4];
    float                     *pixelShaderConstantF;

    /* Indexed Vertex Blending */
    D3DVERTEXBLENDFLAGS       vertex_blend;
    FLOAT                     tween_factor;

    /* RenderState */
    DWORD                     renderState[WINEHIGHEST_RENDER_STATE + 1];

    /* Texture */
    IWineD3DBaseTexture      *textures[MAX_SAMPLERS];
    int                       textureDimensions[MAX_SAMPLERS];

    /* Texture State Stage */
    DWORD                     textureState[MAX_TEXTURES][WINED3D_HIGHEST_TEXTURE_STATE + 1];
    /* Sampler States */
    DWORD                     samplerState[MAX_SAMPLERS][WINED3D_HIGHEST_SAMPLER_STATE + 1];

    /* Current GLSL Shader Program */
    GLhandleARB               shaderPrgId;
};

extern void stateblock_savedstates_set(
    IWineD3DStateBlock* iface,
    SAVEDSTATES* states,
    BOOL value);

extern void stateblock_savedstates_copy(
    IWineD3DStateBlock* iface,
    SAVEDSTATES* dest,
    SAVEDSTATES* source);

extern void stateblock_copy(
    IWineD3DStateBlock* destination,
    IWineD3DStateBlock* source);

extern const IWineD3DStateBlockVtbl IWineD3DStateBlock_Vtbl;

/*****************************************************************************
 * IWineD3DQueryImpl implementation structure (extends IUnknown)
 */
typedef struct IWineD3DQueryImpl
{
    const IWineD3DQueryVtbl  *lpVtbl;
    LONG                      ref;     /* Note: Ref counting not required */
    
    IUnknown                 *parent;
    /*TODO: replace with iface usage */
#if 0
    IWineD3DDevice         *wineD3DDevice;
#else
    IWineD3DDeviceImpl       *wineD3DDevice;
#endif
    /* IWineD3DQuery fields */

    D3DQUERYTYPE              type;
    /* TODO: Think about using a IUnknown instead of a void* */
    void                     *extendedData;
    
  
} IWineD3DQueryImpl;

extern const IWineD3DQueryVtbl IWineD3DQuery_Vtbl;

/* Datastructures for IWineD3DQueryImpl.extendedData */
typedef struct  WineQueryOcclusionData {
    GLuint  queryId;
} WineQueryOcclusionData;


/*****************************************************************************
 * IWineD3DSwapChainImpl implementation structure (extends IUnknown)
 */

typedef struct IWineD3DSwapChainImpl
{
    /*IUnknown part*/
    IWineD3DSwapChainVtbl    *lpVtbl;
    LONG                      ref;     /* Note: Ref counting not required */

    IUnknown                 *parent;
    IWineD3DDeviceImpl       *wineD3DDevice;

    /* IWineD3DSwapChain fields */
    IWineD3DSurface         **backBuffer;
    IWineD3DSurface          *frontBuffer;
    BOOL                      wantsDepthStencilBuffer;
    D3DPRESENT_PARAMETERS     presentParms;

    /* TODO: move everything up to drawable off into a context manager
      and store the 'data' in the contextManagerData interface.
    IUnknown                  *contextManagerData;
    */

    HWND                    win_handle;
    Window                  win;
    Display                *display;

    GLXContext              glCtx;
    XVisualInfo            *visInfo;
    GLXContext              render_ctx;
    /* This has been left in device for now, but needs moving off into a rendertarget management class and separated out from swapchains and devices. */
    Drawable                drawable;
} IWineD3DSwapChainImpl;

extern IWineD3DSwapChainVtbl IWineD3DSwapChain_Vtbl;

/*****************************************************************************
 * Utility function prototypes 
 */

/* Trace routines */
const char* debug_d3dformat(WINED3DFORMAT fmt);
const char* debug_d3ddevicetype(D3DDEVTYPE devtype);
const char* debug_d3dresourcetype(WINED3DRESOURCETYPE res);
const char* debug_d3dusage(DWORD usage);
const char* debug_d3dusagequery(DWORD usagequery);
const char* debug_d3ddeclmethod(WINED3DDECLMETHOD method);
const char* debug_d3ddecltype(WINED3DDECLTYPE type);
const char* debug_d3ddeclusage(BYTE usage);
const char* debug_d3dprimitivetype(D3DPRIMITIVETYPE PrimitiveType);
const char* debug_d3drenderstate(DWORD state);
const char* debug_d3dsamplerstate(DWORD state);
const char* debug_d3dtexturestate(DWORD state);
const char* debug_d3dtstype(WINED3DTRANSFORMSTATETYPE tstype);
const char* debug_d3dpool(WINED3DPOOL pool);

/* Routines for GL <-> D3D values */
GLenum StencilOp(DWORD op);
GLenum StencilFunc(DWORD func);
void   set_tex_op(IWineD3DDevice *iface, BOOL isAlpha, int Stage, D3DTEXTUREOP op, DWORD arg1, DWORD arg2, DWORD arg3);
void   set_tex_op_nvrc(IWineD3DDevice *iface, BOOL is_alpha, int stage, D3DTEXTUREOP op, DWORD arg1, DWORD arg2, DWORD arg3, INT texture_idx);
void   set_texture_matrix(const float *smat, DWORD flags, BOOL calculatedCoords);

int D3DFmtMakeGlCfg(D3DFORMAT BackBufferFormat, D3DFORMAT StencilBufferFormat, int *attribs, int* nAttribs, BOOL alternate);

/* Math utils */
void multiply_matrix(D3DMATRIX *dest, D3DMATRIX *src1, D3DMATRIX *src2);

/*****************************************************************************
 * To enable calling of inherited functions, requires prototypes 
 *
 * Note: Only require classes which are subclassed, ie resource, basetexture, 
 */
    /*** IUnknown methods ***/
    extern HRESULT WINAPI IWineD3DResourceImpl_QueryInterface(IWineD3DResource *iface, REFIID riid, void** ppvObject);
    extern ULONG WINAPI IWineD3DResourceImpl_AddRef(IWineD3DResource *iface);
    extern ULONG WINAPI IWineD3DResourceImpl_Release(IWineD3DResource *iface);
    /*** IWineD3DResource methods ***/
    extern HRESULT WINAPI IWineD3DResourceImpl_GetParent(IWineD3DResource *iface, IUnknown **pParent);
    extern HRESULT WINAPI IWineD3DResourceImpl_GetDevice(IWineD3DResource *iface, IWineD3DDevice ** ppDevice);
    extern HRESULT WINAPI IWineD3DResourceImpl_SetPrivateData(IWineD3DResource *iface, REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags);
    extern HRESULT WINAPI IWineD3DResourceImpl_GetPrivateData(IWineD3DResource *iface, REFGUID  refguid, void * pData, DWORD * pSizeOfData);
    extern HRESULT WINAPI IWineD3DResourceImpl_FreePrivateData(IWineD3DResource *iface, REFGUID  refguid);
    extern DWORD WINAPI IWineD3DResourceImpl_SetPriority(IWineD3DResource *iface, DWORD  PriorityNew);
    extern DWORD WINAPI IWineD3DResourceImpl_GetPriority(IWineD3DResource *iface);
    extern void WINAPI IWineD3DResourceImpl_PreLoad(IWineD3DResource *iface);
    extern WINED3DRESOURCETYPE WINAPI IWineD3DResourceImpl_GetType(IWineD3DResource *iface);
    /*** class static members ***/
    void IWineD3DResourceImpl_CleanUp(IWineD3DResource *iface);

    /*** IUnknown methods ***/
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_QueryInterface(IWineD3DBaseTexture *iface, REFIID riid, void** ppvObject);
    extern ULONG WINAPI IWineD3DBaseTextureImpl_AddRef(IWineD3DBaseTexture *iface);
    extern ULONG WINAPI IWineD3DBaseTextureImpl_Release(IWineD3DBaseTexture *iface);
    /*** IWineD3DResource methods ***/
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_GetParent(IWineD3DBaseTexture *iface, IUnknown **pParent);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_GetDevice(IWineD3DBaseTexture *iface, IWineD3DDevice ** ppDevice);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_SetPrivateData(IWineD3DBaseTexture *iface, REFGUID  refguid, CONST void * pData, DWORD  SizeOfData, DWORD  Flags);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_GetPrivateData(IWineD3DBaseTexture *iface, REFGUID  refguid, void * pData, DWORD * pSizeOfData);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_FreePrivateData(IWineD3DBaseTexture *iface, REFGUID  refguid);
    extern DWORD WINAPI IWineD3DBaseTextureImpl_SetPriority(IWineD3DBaseTexture *iface, DWORD  PriorityNew);
    extern DWORD WINAPI IWineD3DBaseTextureImpl_GetPriority(IWineD3DBaseTexture *iface);
    extern void WINAPI IWineD3DBaseTextureImpl_PreLoad(IWineD3DBaseTexture *iface);
    extern WINED3DRESOURCETYPE WINAPI IWineD3DBaseTextureImpl_GetType(IWineD3DBaseTexture *iface);
    /*** IWineD3DBaseTexture methods ***/
    extern DWORD WINAPI IWineD3DBaseTextureImpl_SetLOD(IWineD3DBaseTexture *iface, DWORD LODNew);
    extern DWORD WINAPI IWineD3DBaseTextureImpl_GetLOD(IWineD3DBaseTexture *iface);
    extern DWORD WINAPI IWineD3DBaseTextureImpl_GetLevelCount(IWineD3DBaseTexture *iface);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_SetAutoGenFilterType(IWineD3DBaseTexture *iface, WINED3DTEXTUREFILTERTYPE FilterType);
    extern WINED3DTEXTUREFILTERTYPE WINAPI IWineD3DBaseTextureImpl_GetAutoGenFilterType(IWineD3DBaseTexture *iface);
    extern void WINAPI IWineD3DBaseTextureImpl_GenerateMipSubLevels(IWineD3DBaseTexture *iface);
    extern BOOL WINAPI IWineD3DBaseTextureImpl_SetDirty(IWineD3DBaseTexture *iface, BOOL);
    extern BOOL WINAPI IWineD3DBaseTextureImpl_GetDirty(IWineD3DBaseTexture *iface);

    extern BYTE* WINAPI IWineD3DVertexBufferImpl_GetMemory(IWineD3DVertexBuffer* iface, DWORD iOffset, GLint *vbo);
    extern HRESULT WINAPI IWineD3DVertexBufferImpl_ReleaseMemory(IWineD3DVertexBuffer* iface);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_BindTexture(IWineD3DBaseTexture *iface);
    extern HRESULT WINAPI IWineD3DBaseTextureImpl_UnBindTexture(IWineD3DBaseTexture *iface);
    extern void WINAPI IWineD3DBaseTextureImpl_ApplyStateChanges(IWineD3DBaseTexture *iface, const DWORD textureStates[WINED3D_HIGHEST_TEXTURE_STATE + 1], const DWORD samplerStates[WINED3D_HIGHEST_SAMPLER_STATE + 1]);
    /*** class static members ***/
    void IWineD3DBaseTextureImpl_CleanUp(IWineD3DBaseTexture *iface);

struct SHADER_OPCODE_ARG;
typedef void (*shader_fct_t)();
typedef void (*SHADER_HANDLER) (struct SHADER_OPCODE_ARG*);

/* Struct to maintain a list of GLSL shader programs and their associated pixel and
 * vertex shaders.  A list of this type is maintained on the DeviceImpl, and is only
 * used if the user is using GLSL shaders. */
struct glsl_shader_prog_link {
    struct list             entry;
    GLhandleARB             programId;
    IWineD3DVertexShader*   vertexShader;
    IWineD3DPixelShader*    pixelShader;
};

/* TODO: Make this dynamic, based on shader limits ? */
#define MAX_REG_ADDR 1
#define MAX_REG_TEMP 32
#define MAX_REG_TEXCRD 8
#define MAX_REG_INPUT 12
#define MAX_REG_OUTPUT 12
#define MAX_ATTRIBS 16
#define MAX_CONST_I 16
#define MAX_CONST_B 16

/* FIXME: This needs to go up to 2048 for
 * Shader model 3 according to msdn (and for software shaders) */
#define MAX_LABELS 16

typedef struct semantic {
    DWORD usage;
    DWORD reg;
} semantic;

typedef struct local_constant {
    struct list entry;
    unsigned int idx;
    DWORD value[4];
} local_constant;

typedef struct shader_reg_maps {

    char texcoord[MAX_REG_TEXCRD];          /* pixel < 3.0 */
    char temporary[MAX_REG_TEMP];           /* pixel, vertex */
    char address[MAX_REG_ADDR];             /* vertex */
    char packed_input[MAX_REG_INPUT];       /* pshader >= 3.0 */
    char packed_output[MAX_REG_OUTPUT];     /* vertex >= 3.0 */
    char attributes[MAX_ATTRIBS];           /* vertex */
    char labels[MAX_LABELS];                /* pixel, vertex */

    /* Sampler usage tokens 
     * Use 0 as default (bit 31 is always 1 on a valid token) */
    DWORD samplers[MAX_SAMPLERS];

    /* Whether or not a loop is used in this shader */
    char loop;

    /* Whether or not this shader uses fog */
    char fog;

} shader_reg_maps;

#define SHADER_PGMSIZE 65535
typedef struct SHADER_BUFFER {
    char* buffer;
    unsigned int bsize;
    unsigned int lineNo;
} SHADER_BUFFER;

/* Undocumented opcode controls */
#define INST_CONTROLS_SHIFT 16
#define INST_CONTROLS_MASK 0x00ff0000

typedef enum COMPARISON_TYPE {
    COMPARISON_GT = 1,
    COMPARISON_EQ = 2,
    COMPARISON_GE = 3,
    COMPARISON_LT = 4,
    COMPARISON_NE = 5,
    COMPARISON_LE = 6
} COMPARISON_TYPE;

typedef struct SHADER_OPCODE {
    unsigned int  opcode;
    const char*   name;
    const char*   glname;
    char          dst_token;
    CONST UINT    num_params;
    shader_fct_t  soft_fct;
    SHADER_HANDLER hw_fct;
    SHADER_HANDLER hw_glsl_fct;
    DWORD         min_version;
    DWORD         max_version;
} SHADER_OPCODE;

typedef struct SHADER_OPCODE_ARG {
    IWineD3DBaseShader* shader;
    shader_reg_maps* reg_maps;
    CONST SHADER_OPCODE* opcode;
    DWORD opcode_token;
    DWORD dst;
    DWORD dst_addr;
    DWORD predicate;
    DWORD src[4];
    DWORD src_addr[4];
    SHADER_BUFFER* buffer;
} SHADER_OPCODE_ARG;

typedef struct SHADER_LIMITS {
    unsigned int temporary;
    unsigned int texcoord;
    unsigned int sampler;
    unsigned int constant_int;
    unsigned int constant_float;
    unsigned int constant_bool;
    unsigned int address;
    unsigned int packed_output;
    unsigned int packed_input;
    unsigned int attributes;
    unsigned int label;
} SHADER_LIMITS;

/** Keeps track of details for TEX_M#x# shader opcodes which need to 
    maintain state information between multiple codes */
typedef struct SHADER_PARSE_STATE {
    unsigned int current_row;
    DWORD texcoord_w[2];
} SHADER_PARSE_STATE;

/* Base Shader utility functions. 
 * (may move callers into the same file in the future) */
extern int shader_addline(
    SHADER_BUFFER* buffer,
    const char* fmt, ...);

extern const SHADER_OPCODE* shader_get_opcode(
    IWineD3DBaseShader *iface, 
    const DWORD code);

extern void shader_delete_constant_list(
    struct list* clist);

/* Vertex shader utility functions */
extern BOOL vshader_get_input(
    IWineD3DVertexShader* iface,
    BYTE usage_req, BYTE usage_idx_req,
    unsigned int* regnum);

extern BOOL vshader_input_is_color(
    IWineD3DVertexShader* iface,
    unsigned int regnum);

extern HRESULT allocate_shader_constants(IWineD3DStateBlockImpl* object);

/* ARB_[vertex/fragment]_program helper functions */
extern void shader_arb_load_constants(
    IWineD3DStateBlock* iface,
    char usePixelShader,
    char useVertexShader);

/* ARB shader program Prototypes */
extern void shader_hw_def(SHADER_OPCODE_ARG *arg);

/* ARB pixel shader prototypes */
extern void pshader_hw_cnd(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_cmp(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_map2gl(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_tex(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texcoord(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texreg2ar(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texreg2gb(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texbem(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texm3x2pad(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texm3x2tex(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texm3x3pad(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texm3x3tex(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texm3x3spec(SHADER_OPCODE_ARG* arg);
extern void pshader_hw_texm3x3vspec(SHADER_OPCODE_ARG* arg);

/* ARB vertex shader prototypes */
extern void vshader_hw_map2gl(SHADER_OPCODE_ARG* arg);
extern void vshader_hw_mnxn(SHADER_OPCODE_ARG* arg);

/* GLSL helper functions */
extern void set_glsl_shader_program(IWineD3DDevice *iface);
extern void shader_glsl_add_instruction_modifiers(SHADER_OPCODE_ARG *arg);
extern void shader_glsl_load_constants(
    IWineD3DStateBlock* iface,
    char usePixelShader,
    char useVertexShader);

/** The following translate DirectX pixel/vertex shader opcodes to GLSL lines */
extern void shader_glsl_map2gl(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_arith(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_mov(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_mad(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_mnxn(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_lrp(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_dot(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_rcp(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_cnd(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_compare(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_def(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_defi(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_defb(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_expp(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_cmp(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_lit(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_dst(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_sincos(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_loop(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_end(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_if(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_ifc(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_else(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_break(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_breakc(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_rep(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_call(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_callnz(SHADER_OPCODE_ARG* arg);
extern void shader_glsl_label(SHADER_OPCODE_ARG* arg);

/** GLSL Pixel Shader Prototypes */
extern void pshader_glsl_tex(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texcoord(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texdp3tex(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texdp3(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texdepth(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x2depth(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x2pad(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x2tex(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x3(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x3pad(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x3tex(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x3spec(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texm3x3vspec(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texkill(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texbem(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texreg2ar(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texreg2gb(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_texreg2rgb(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_dp2add(SHADER_OPCODE_ARG* arg);
extern void pshader_glsl_input_pack(
   SHADER_BUFFER* buffer,
   semantic* semantics_out);

/** GLSL Vertex Shader Prototypes */
extern void vshader_glsl_output_unpack(
   SHADER_BUFFER* buffer,
   semantic* semantics_out);

/*****************************************************************************
 * IDirect3DBaseShader implementation structure
 */
typedef struct IWineD3DBaseShaderClass
{
    DWORD                           hex_version;
    SHADER_LIMITS                   limits;
    SHADER_PARSE_STATE              parse_state;
    CONST SHADER_OPCODE             *shader_ins;
    CONST DWORD                     *function;
    UINT                            functionLength;
    GLuint                          prgId;
    BOOL                            is_compiled;

    /* Type of shader backend */
    int shader_mode;

    /* Immediate constants (override global ones) */
    struct list constantsB;
    struct list constantsF;
    struct list constantsI;
    shader_reg_maps reg_maps;

} IWineD3DBaseShaderClass;

typedef struct IWineD3DBaseShaderImpl {
    /* IUnknown */
    const IWineD3DBaseShaderVtbl    *lpVtbl;
    LONG                            ref;

    /* IWineD3DBaseShader */
    IWineD3DBaseShaderClass         baseShader;
} IWineD3DBaseShaderImpl;

extern HRESULT shader_get_registers_used(
    IWineD3DBaseShader *iface,
    shader_reg_maps* reg_maps,
    semantic* semantics_in,
    semantic* semantics_out,
    CONST DWORD* pToken);

extern void shader_generate_glsl_declarations(
    IWineD3DBaseShader *iface,
    shader_reg_maps* reg_maps,
    SHADER_BUFFER* buffer,
    WineD3D_GL_Info* gl_info);

extern void shader_generate_arb_declarations(
    IWineD3DBaseShader *iface,
    shader_reg_maps* reg_maps,
    SHADER_BUFFER* buffer,
    WineD3D_GL_Info* gl_info);

extern void shader_generate_main(
    IWineD3DBaseShader *iface,
    SHADER_BUFFER* buffer,
    shader_reg_maps* reg_maps,
    CONST DWORD* pFunction);

extern void shader_dump_ins_modifiers(
    const DWORD output);

extern void shader_dump_param(
    IWineD3DBaseShader *iface,
    const DWORD param,
    const DWORD addr_token,
    int input);

extern void shader_trace_init(
    IWineD3DBaseShader *iface,
    const DWORD* pFunction);

extern int shader_get_param(
    IWineD3DBaseShader* iface,
    const DWORD* pToken,
    DWORD* param,
    DWORD* addr_token);

extern int shader_skip_unrecognized(
    IWineD3DBaseShader* iface,
    const DWORD* pToken);

extern void print_glsl_info_log(
    WineD3D_GL_Info *gl_info,
    GLhandleARB obj);

inline static int shader_get_regtype(const DWORD param) {
    return (((param & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT) |
            ((param & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2));
}

extern unsigned int shader_get_float_offset(const DWORD reg);

inline static BOOL shader_is_pshader_version(DWORD token) {
    return 0xFFFF0000 == (token & 0xFFFF0000);
}

inline static BOOL shader_is_vshader_version(DWORD token) {
    return 0xFFFE0000 == (token & 0xFFFF0000);
}

inline static BOOL shader_is_comment(DWORD token) {
    return D3DSIO_COMMENT == (token & D3DSI_OPCODE_MASK);
}

/*****************************************************************************
 * IDirect3DVertexShader implementation structure
 */
typedef struct IWineD3DVertexShaderImpl {
    /* IUnknown parts*/   
    const IWineD3DVertexShaderVtbl *lpVtbl;
    LONG                        ref;     /* Note: Ref counting not required */

    /* IWineD3DBaseShader */
    IWineD3DBaseShaderClass     baseShader;

    /* IWineD3DVertexShaderImpl */
    IUnknown                    *parent;
    IWineD3DDeviceImpl          *wineD3DDevice;

    char                        usesFog;
    DWORD                       usage;

    /* Vertex shader input and output semantics */
    semantic semantics_in [MAX_ATTRIBS];
    semantic semantics_out [MAX_REG_OUTPUT];

    /* run time datas...  */
    VSHADERDATA                *data;
    IWineD3DVertexDeclaration  *vertexDeclaration;
#if 0 /* needs reworking */
    /* run time datas */
    VSHADERINPUTDATA input;
    VSHADEROUTPUTDATA output;
#endif
} IWineD3DVertexShaderImpl;
extern const SHADER_OPCODE IWineD3DVertexShaderImpl_shader_ins[];
extern const IWineD3DVertexShaderVtbl IWineD3DVertexShader_Vtbl;

/*****************************************************************************
 * IDirect3DPixelShader implementation structure
 */
typedef struct IWineD3DPixelShaderImpl {
    /* IUnknown parts */
    const IWineD3DPixelShaderVtbl *lpVtbl;
    LONG                        ref;     /* Note: Ref counting not required */

    /* IWineD3DBaseShader */
    IWineD3DBaseShaderClass     baseShader;

    /* IWineD3DPixelShaderImpl */
    IUnknown                   *parent;
    IWineD3DDeviceImpl         *wineD3DDevice;

    /* Pixel shader input semantics */
    semantic semantics_in [MAX_REG_INPUT];

    /* run time data */
    PSHADERDATA                *data;

#if 0 /* needs reworking */
    PSHADERINPUTDATA input;
    PSHADEROUTPUTDATA output;
#endif
} IWineD3DPixelShaderImpl;

extern const SHADER_OPCODE IWineD3DPixelShaderImpl_shader_ins[];
extern const IWineD3DPixelShaderVtbl IWineD3DPixelShader_Vtbl;

/*****************************************************************************
 * IWineD3DPalette implementation structure
 */
struct IWineD3DPaletteImpl {
    /* IUnknown parts */
    const IWineD3DPaletteVtbl  *lpVtbl;
    LONG                       ref;

    IUnknown                   *parent;
    IWineD3DDeviceImpl         *wineD3DDevice;

    /* IWineD3DPalette */
    HPALETTE                   hpal;
    WORD                       palVersion;     /*|               */
    WORD                       palNumEntries;  /*|  LOGPALETTE   */
    PALETTEENTRY               palents[256];   /*|               */
    /* This is to store the palette in 'screen format' */
    int                        screen_palents[256];
    DWORD                      Flags;
};

extern const IWineD3DPaletteVtbl IWineD3DPalette_Vtbl;
DWORD IWineD3DPaletteImpl_Size(DWORD dwFlags);

/* DirectDraw utility functions */
extern WINED3DFORMAT pixelformat_for_depth(DWORD depth);

/*****************************************************************************
 * Pixel format management
 */
typedef struct {
    WINED3DFORMAT           format;
    DWORD                   alphaMask, redMask, greenMask, blueMask;
    UINT                    bpp;
    BOOL                    isFourcc;
    GLint                   glInternal, glFormat, glType;
} PixelFormatDesc;

const PixelFormatDesc *getFormatDescEntry(WINED3DFORMAT fmt);
#endif
