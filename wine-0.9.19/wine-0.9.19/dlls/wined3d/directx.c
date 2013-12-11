/*
 * IWineD3D implementation
 *
 * Copyright 2002-2004 Jason Edmeades
 * Copyright 2003-2004 Raphael Junqueira
 * Copyright 2004 Christian Costa
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

/* Compile time diagnostics: */

/* Uncomment this to force only a single display mode to be exposed: */
/*#define DEBUG_SINGLE_MODE*/


#include "config.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);
WINE_DECLARE_DEBUG_CHANNEL(d3d_caps);
#define GLINFO_LOCATION This->gl_info

/**********************************************************
 * Utility functions follow
 **********************************************************/

/* x11drv GDI escapes */
#define X11DRV_ESCAPE 6789
enum x11drv_escape_codes
{
    X11DRV_GET_DISPLAY,   /* get X11 display for a DC */
    X11DRV_GET_DRAWABLE,  /* get current drawable for a DC */
    X11DRV_GET_FONT,      /* get current X font for a DC */
};

/* retrieve the X display to use on a given DC */
inline static Display *get_display( HDC hdc )
{
    Display *display;
    enum x11drv_escape_codes escape = X11DRV_GET_DISPLAY;

    if (!ExtEscape( hdc, X11DRV_ESCAPE, sizeof(escape), (LPCSTR)&escape,
                    sizeof(display), (LPSTR)&display )) display = NULL;
    return display;
}

/* lookup tables */
int minLookup[MAX_LOOKUPS];
int maxLookup[MAX_LOOKUPS];
DWORD *stateLookup[MAX_LOOKUPS];

DWORD minMipLookup[WINED3DTEXF_ANISOTROPIC + 1][WINED3DTEXF_LINEAR + 1];



/**
 * Note: GL seems to trap if GetDeviceCaps is called before any HWND's created
 * ie there is no GL Context - Get a default rendering context to enable the
 * function query some info from GL
 */
static WineD3D_Context* WineD3D_CreateFakeGLContext(void) {
    static WineD3D_Context ctx = { NULL, NULL, NULL, 0, 0 };
    WineD3D_Context* ret = NULL;

    if (glXGetCurrentContext() == NULL) {
       BOOL         gotContext  = FALSE;
       BOOL         created     = FALSE;
       XVisualInfo  template;
       HDC          device_context;
       Visual*      visual;
       BOOL         failed = FALSE;
       int          num;
       XWindowAttributes win_attr;
       TRACE_(d3d_caps)("Creating Fake GL Context\n");

       ctx.drawable = (Drawable) GetPropA(GetDesktopWindow(), "__wine_x11_whole_window");

       /* Get the display */
       device_context = GetDC(0);
       ctx.display = get_display(device_context);
       ReleaseDC(0, device_context);

       /* Get the X visual */
       ENTER_GL();
       if (XGetWindowAttributes(ctx.display, ctx.drawable, &win_attr)) {
           visual = win_attr.visual;
       } else {
           visual = DefaultVisual(ctx.display, DefaultScreen(ctx.display));
       }
       template.visualid = XVisualIDFromVisual(visual);
       ctx.visInfo = XGetVisualInfo(ctx.display, VisualIDMask, &template, &num);
       if (ctx.visInfo == NULL) {
           LEAVE_GL();
           WARN_(d3d_caps)("Error creating visual info for capabilities initialization\n");
           failed = TRUE;
       }

       /* Create a GL context */
       if (!failed) {
           ctx.glCtx = glXCreateContext(ctx.display, ctx.visInfo, NULL, GL_TRUE);

           if (ctx.glCtx == NULL) {
               LEAVE_GL();
               WARN_(d3d_caps)("Error creating default context for capabilities initialization\n");
               failed = TRUE;
           }
       }

       /* Make it the current GL context */
       if (!failed && glXMakeCurrent(ctx.display, ctx.drawable, ctx.glCtx) == False) {
           glXDestroyContext(ctx.display, ctx.glCtx);
           LEAVE_GL();
           WARN_(d3d_caps)("Error setting default context as current for capabilities initialization\n");
           failed = TRUE;
       }

       /* It worked! Wow... */
       if (!failed) {
           gotContext = TRUE;
           created = TRUE;
           ret = &ctx;
       } else {
           ret = NULL;
       }

   } else {
     if (ctx.ref > 0) ret = &ctx;
   }

   if (NULL != ret) InterlockedIncrement(&ret->ref);
   return ret;
}

static void WineD3D_ReleaseFakeGLContext(WineD3D_Context* ctx) {
    /* If we created a dummy context, throw it away */
    if (NULL != ctx) {
        if (0 == InterlockedDecrement(&ctx->ref)) {
            glXMakeCurrent(ctx->display, None, NULL);
            glXDestroyContext(ctx->display, ctx->glCtx);
            ctx->display = NULL;
            ctx->glCtx = NULL;
            LEAVE_GL();
        }
    }
}

/**********************************************************
 * IUnknown parts follows
 **********************************************************/

static HRESULT WINAPI IWineD3DImpl_QueryInterface(IWineD3D *iface,REFIID riid,LPVOID *ppobj)
{
    IWineD3DDeviceImpl *This = (IWineD3DDeviceImpl *)iface;

    TRACE("(%p)->(%s,%p)\n",This,debugstr_guid(riid),ppobj);
    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IWineD3DBase)
        || IsEqualGUID(riid, &IID_IWineD3DDevice)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI IWineD3DImpl_AddRef(IWineD3D *iface) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef increasing from %ld\n", This, refCount - 1);
    return refCount;
}

static ULONG WINAPI IWineD3DImpl_Release(IWineD3D *iface) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    ULONG ref;
    TRACE("(%p) : Releasing from %ld\n", This, This->ref);
    ref = InterlockedDecrement(&This->ref);
    if (ref == 0) {
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/* Set the shader type for this device, depending on the given capabilities,
 * the device type, and the user preferences in wined3d_settings */

static void select_shader_mode(
    WineD3D_GL_Info *gl_info,
    WINED3DDEVTYPE DeviceType,
    int* ps_selected,
    int* vs_selected) {

    /* Give priority to user disable/emulation request.
     * Then respect REF device for software.
     * Then check capabilities for hardware, and fallback to software */

    if (wined3d_settings.vs_mode == VS_NONE) {
        *vs_selected = SHADER_NONE;
    } else if (DeviceType == WINED3DDEVTYPE_REF || wined3d_settings.vs_mode == VS_SW) {
        *vs_selected = SHADER_SW;
    } else if (gl_info->supported[ARB_VERTEX_SHADER] && wined3d_settings.glslRequested) {
        *vs_selected = SHADER_GLSL;
    } else if (gl_info->supported[ARB_VERTEX_PROGRAM]) {
        *vs_selected = SHADER_ARB;
    } else {
        *vs_selected = SHADER_SW;
    }

    /* Fallback to SHADER_NONE where software pixel shaders should be used */
    if (wined3d_settings.ps_mode == PS_NONE) {
        *ps_selected = SHADER_NONE;
    } else if (DeviceType == WINED3DDEVTYPE_REF) {
        *ps_selected = SHADER_NONE;
    } else if (gl_info->supported[ARB_FRAGMENT_SHADER] && wined3d_settings.glslRequested) {
        *ps_selected = SHADER_GLSL;
    } else if (gl_info->supported[ARB_FRAGMENT_PROGRAM]) {
        *ps_selected = SHADER_ARB;
    } else {
        *ps_selected = SHADER_NONE;
    }
}

/** Select the number of report maximum shader constants based on the selected shader modes */
void select_shader_max_constants(WineD3D_GL_Info *gl_info) {

    switch (wined3d_settings.vs_selected_mode) {
        case SHADER_GLSL:
            /* Subtract the other potential uniforms from the max available (bools, ints, and 1 row of projection matrix) */
            gl_info->max_vshader_constantsF = gl_info->vs_glsl_constantsF - MAX_CONST_B - MAX_CONST_I - 1;
            break;
        case SHADER_ARB:
            /* We have to subtract any other PARAMs that we might use in our shader programs.
             * ATI seems to count 2 implicit PARAMs when we use fog and NVIDIA counts 1,
             * and we reference one row of the PROJECTION matrix which counts as 1 PARAM. */
            gl_info->max_vshader_constantsF = gl_info->vs_arb_constantsF - 3;
            break;
        case SHADER_SW:
            gl_info->max_vshader_constantsF = 96;  /* TODO: Fixup software shaders */
            break;
        default:
            gl_info->max_vshader_constantsF = 0;
            break;
    }

    switch (wined3d_settings.ps_selected_mode) {
        case SHADER_GLSL:
            /* Subtract the other potential uniforms from the max available (bools & ints) */
            gl_info->max_pshader_constantsF = gl_info->ps_glsl_constantsF - MAX_CONST_B - MAX_CONST_I;
            break;
        case SHADER_ARB:
            gl_info->max_pshader_constantsF = gl_info->ps_arb_constantsF;
            break;
        case SHADER_SW:
            gl_info->max_pshader_constantsF = 96;  /* TODO: Fixup software shaders */
            break;
        default:
            gl_info->max_pshader_constantsF = 0;
            break;
    }
}

/**********************************************************
 * IWineD3D parts follows
 **********************************************************/

BOOL IWineD3DImpl_FillGLCaps(IWineD3D *iface, Display* display) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    WineD3D_GL_Info *gl_info = &This->gl_info;

    const char *GL_Extensions    = NULL;
    const char *GLX_Extensions   = NULL;
    const char *gl_string        = NULL;
    const char *gl_string_cursor = NULL;
    GLint       gl_max;
    GLfloat     gl_floatv[2];
    Bool        test = 0;
    int         major, minor;
    WineD3D_Context *fake_ctx = NULL;
    BOOL        gotContext    = FALSE;
    int         i;

    /* Make sure that we've got a context */
    if (glXGetCurrentContext() == NULL) {
        /* TODO: CreateFakeGLContext should really take a display as a parameter  */
        fake_ctx = WineD3D_CreateFakeGLContext();
        if (NULL != fake_ctx) gotContext = TRUE;
    } else {
        gotContext = TRUE;
    }

    TRACE_(d3d_caps)("(%p, %p)\n", gl_info, display);

    gl_string = (const char *) glGetString(GL_RENDERER);
    strcpy(gl_info->gl_renderer, gl_string);

    /* Fill in the GL info retrievable depending on the display */
    if (NULL != display) {
        test = glXQueryVersion(display, &major, &minor);
        gl_info->glx_version = ((major & 0x0000FFFF) << 16) | (minor & 0x0000FFFF);
    } else {
        FIXME("Display must not be NULL, use glXGetCurrentDisplay or getAdapterDisplay()\n");
    }
    gl_string = (const char *) glGetString(GL_VENDOR);

    TRACE_(d3d_caps)("Filling vendor string %s\n", gl_string);
    if (gl_string != NULL) {
        /* Fill in the GL vendor */
        if (strstr(gl_string, "NVIDIA")) {
            gl_info->gl_vendor = VENDOR_NVIDIA;
        } else if (strstr(gl_string, "ATI")) {
            gl_info->gl_vendor = VENDOR_ATI;
        } else if (strstr(gl_string, "Intel(R)") || 
		   strstr(gl_info->gl_renderer, "Intel(R)")) {
            gl_info->gl_vendor = VENDOR_INTEL;
        } else if (strstr(gl_string, "Mesa")) {
            gl_info->gl_vendor = VENDOR_MESA;
        } else {
            gl_info->gl_vendor = VENDOR_WINE;
        }
    } else {
        gl_info->gl_vendor = VENDOR_WINE;
    }


    TRACE_(d3d_caps)("found GL_VENDOR (%s)->(0x%04x)\n", debugstr_a(gl_string), gl_info->gl_vendor);

    /* Parse the GL_VERSION field into major and minor information */
    gl_string = (const char *) glGetString(GL_VERSION);
    if (gl_string != NULL) {

        switch (gl_info->gl_vendor) {
        case VENDOR_NVIDIA:
            gl_string_cursor = strstr(gl_string, "NVIDIA");
            if (!gl_string_cursor) {
                ERR_(d3d_caps)("Invalid nVidia version string: %s\n", debugstr_a(gl_string));
                break;
            }

            gl_string_cursor = strstr(gl_string_cursor, " ");
            if (!gl_string_cursor) {
                ERR_(d3d_caps)("Invalid nVidia version string: %s\n", debugstr_a(gl_string));
                break;
            }

            while (*gl_string_cursor == ' ') {
                ++gl_string_cursor;
            }

            if (!*gl_string_cursor) {
                ERR_(d3d_caps)("Invalid nVidia version string: %s\n", debugstr_a(gl_string));
                break;
            }

            major = atoi(gl_string_cursor);
            while (*gl_string_cursor <= '9' && *gl_string_cursor >= '0') {
                ++gl_string_cursor;
            }

            if (*gl_string_cursor++ != '.') {
                ERR_(d3d_caps)("Invalid nVidia version string: %s\n", debugstr_a(gl_string));
                break;
            }

            minor = atoi(gl_string_cursor);
            minor = major*100+minor;
            major = 10;

            break;

        case VENDOR_ATI:
            major = minor = 0;
            gl_string_cursor = strchr(gl_string, '-');
            if (gl_string_cursor) {
                int error = 0;
                gl_string_cursor++;

                /* Check if version number is of the form x.y.z */
                if (*gl_string_cursor > '9' && *gl_string_cursor < '0')
                    error = 1;
                if (!error && *(gl_string_cursor+2) > '9' && *(gl_string_cursor+2) < '0')
                    error = 1;
                if (!error && *(gl_string_cursor+4) > '9' && *(gl_string_cursor+4) < '0')
                    error = 1;
                if (!error && *(gl_string_cursor+1) != '.' && *(gl_string_cursor+3) != '.')
                    error = 1;

                /* Mark version number as malformed */
                if (error)
                    gl_string_cursor = 0;
            }

            if (!gl_string_cursor)
                WARN_(d3d_caps)("malformed GL_VERSION (%s)\n", debugstr_a(gl_string));
            else {
                major = *gl_string_cursor - '0';
                minor = (*(gl_string_cursor+2) - '0') * 256 + (*(gl_string_cursor+4) - '0');
            }
            break;

	case VENDOR_INTEL:
        case VENDOR_MESA:
            gl_string_cursor = strstr(gl_string, "Mesa");
            gl_string_cursor = strstr(gl_string_cursor, " ");
            while (*gl_string_cursor && ' ' == *gl_string_cursor) ++gl_string_cursor;
            if (*gl_string_cursor) {
                char tmp[16];
                int cursor = 0;

                while (*gl_string_cursor <= '9' && *gl_string_cursor >= '0') {
                    tmp[cursor++] = *gl_string_cursor;
                    ++gl_string_cursor;
                }
                tmp[cursor] = 0;
                major = atoi(tmp);

                if (*gl_string_cursor != '.') WARN_(d3d_caps)("malformed GL_VERSION (%s)\n", debugstr_a(gl_string));
                ++gl_string_cursor;

                cursor = 0;
                while (*gl_string_cursor <= '9' && *gl_string_cursor >= '0') {
                    tmp[cursor++] = *gl_string_cursor;
                    ++gl_string_cursor;
                }
                tmp[cursor] = 0;
                minor = atoi(tmp);
            }
            break;

        default:
            major = 0;
            minor = 9;
        }
        gl_info->gl_driver_version = MAKEDWORD_VERSION(major, minor);
        TRACE_(d3d_caps)("found GL_VERSION  (%s)->%i.%i->(0x%08lx)\n", debugstr_a(gl_string), major, minor, gl_info->gl_driver_version);

        /* Fill in the renderer information */

        switch (gl_info->gl_vendor) {
        case VENDOR_NVIDIA:
            if (strstr(gl_info->gl_renderer, "GeForce4 Ti")) {
                gl_info->gl_card = CARD_NVIDIA_GEFORCE4_TI4600;
            } else if (strstr(gl_info->gl_renderer, "GeForceFX")) {
                gl_info->gl_card = CARD_NVIDIA_GEFORCEFX_5900ULTRA;
            } else if (strstr(gl_info->gl_renderer, "Quadro FX 3000")) {
                gl_info->gl_card = CARD_NVIDIA_QUADROFX_3000;
            } else if (strstr(gl_info->gl_renderer, "GeForce 6800")) {
                gl_info->gl_card = CARD_NVIDIA_GEFORCE_6800ULTRA;
            } else if (strstr(gl_info->gl_renderer, "Quadro FX 4000")) {
                gl_info->gl_card = CARD_NVIDIA_QUADROFX_4000;
            } else if (strstr(gl_info->gl_renderer, "GeForce 7800")) {
                gl_info->gl_card = CARD_NVIDIA_GEFORCE_7800ULTRA;
            } else {
                gl_info->gl_card = CARD_NVIDIA_GEFORCE4_TI4600;
            }
            break;

        case VENDOR_ATI:
            if (strstr(gl_info->gl_renderer, "RADEON 9800 PRO")) {
                gl_info->gl_card = CARD_ATI_RADEON_9800PRO;
            } else if (strstr(gl_info->gl_renderer, "RADEON 9700 PRO")) {
                gl_info->gl_card = CARD_ATI_RADEON_9700PRO;
            } else {
                gl_info->gl_card = CARD_ATI_RADEON_8500;
            }
            break;

        case VENDOR_INTEL:
            if (strstr(gl_info->gl_renderer, "915GM")) {
                gl_info->gl_card = CARD_INTEL_I915GM;
            } else if (strstr(gl_info->gl_renderer, "915G")) {
                gl_info->gl_card = CARD_INTEL_I915G;
            } else if (strstr(gl_info->gl_renderer, "865G")) {
                gl_info->gl_card = CARD_INTEL_I865G;
            } else if (strstr(gl_info->gl_renderer, "855G")) {
                gl_info->gl_card = CARD_INTEL_I855G;
            } else if (strstr(gl_info->gl_renderer, "830G")) {
                gl_info->gl_card = CARD_INTEL_I830G;
            } else {
	      gl_info->gl_card = CARD_INTEL_I915G;
            }
            break;

        default:
            gl_info->gl_card = CARD_WINE;
            break;
        }
    } else {
        FIXME("get version string returned null\n");
    }

    TRACE_(d3d_caps)("found GL_RENDERER (%s)->(0x%04x)\n", debugstr_a(gl_info->gl_renderer), gl_info->gl_card);

    /*
     * Initialize openGL extension related variables
     *  with Default values
     */
    memset(&gl_info->supported, 0, sizeof(gl_info->supported));
    gl_info->max_buffers        = 1;
    gl_info->max_textures       = 1;
    gl_info->max_texture_stages = 1;
    gl_info->max_samplers       = 1;
    gl_info->max_sampler_stages = 1;
    gl_info->ps_arb_version = PS_VERSION_NOT_SUPPORTED;
    gl_info->ps_arb_max_temps = 0;
    gl_info->ps_arb_max_instructions = 0;
    gl_info->vs_arb_version = VS_VERSION_NOT_SUPPORTED;
    gl_info->vs_arb_max_temps = 0;
    gl_info->vs_arb_max_instructions = 0;
    gl_info->vs_nv_version  = VS_VERSION_NOT_SUPPORTED;
    gl_info->vs_ati_version = VS_VERSION_NOT_SUPPORTED;
    gl_info->vs_glsl_constantsF = 0;
    gl_info->ps_glsl_constantsF = 0;
    gl_info->vs_arb_constantsF = 0;
    gl_info->ps_arb_constantsF = 0;

    /* Now work out what GL support this card really has */
#define USE_GL_FUNC(type, pfn) gl_info->pfn = (type) glXGetProcAddressARB( (const GLubyte *) #pfn);
    GL_EXT_FUNCS_GEN;
    GLX_EXT_FUNCS_GEN;
#undef USE_GL_FUNC

    /* Retrieve opengl defaults */
    glGetIntegerv(GL_MAX_CLIP_PLANES, &gl_max);
    gl_info->max_clipplanes = min(D3DMAXUSERCLIPPLANES, gl_max);
    TRACE_(d3d_caps)("ClipPlanes support - num Planes=%d\n", gl_max);

    glGetIntegerv(GL_MAX_LIGHTS, &gl_max);
    gl_info->max_lights = gl_max;
    TRACE_(d3d_caps)("Lights support - max lights=%d\n", gl_max);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max);
    gl_info->max_texture_size = gl_max;
    TRACE_(d3d_caps)("Maximum texture size support - max texture size=%d\n", gl_max);

    glGetFloatv(GL_POINT_SIZE_RANGE, gl_floatv);
    gl_info->max_pointsize = gl_floatv[1];
    TRACE_(d3d_caps)("Maximum point size support - max point size=%f\n", gl_floatv[1]);

    /* Parse the gl supported features, in theory enabling parts of our code appropriately */
    GL_Extensions = (const char *) glGetString(GL_EXTENSIONS);
    TRACE_(d3d_caps)("GL_Extensions reported:\n");

    if (NULL == GL_Extensions) {
        ERR("   GL_Extensions returns NULL\n");
    } else {
        while (*GL_Extensions != 0x00) {
            const char *Start = GL_Extensions;
            char        ThisExtn[256];

            memset(ThisExtn, 0x00, sizeof(ThisExtn));
            while (*GL_Extensions != ' ' && *GL_Extensions != 0x00) {
                GL_Extensions++;
            }
            memcpy(ThisExtn, Start, (GL_Extensions - Start));
            TRACE_(d3d_caps)("- %s\n", ThisExtn);

            /**
             * ARB
             */
            if (strcmp(ThisExtn, "GL_ARB_draw_buffers") == 0) {
                glGetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &gl_max);
                TRACE_(d3d_caps)(" FOUND: ARB_draw_buffers support - max buffers=%u\n", gl_max);            
                gl_info->supported[ARB_DRAW_BUFFERS] = TRUE;
                gl_info->max_buffers = gl_max;
            } else if (strcmp(ThisExtn, "GL_ARB_fragment_program") == 0) {
                gl_info->ps_arb_version = PS_VERSION_11;
                TRACE_(d3d_caps)(" FOUND: ARB Pixel Shader support - version=%02x\n", gl_info->ps_arb_version);
                gl_info->supported[ARB_FRAGMENT_PROGRAM] = TRUE;
                glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &gl_max);
                TRACE_(d3d_caps)(" FOUND: ARB Pixel Shader support - GL_MAX_TEXTURE_IMAGE_UNITS_ARB=%u\n", gl_max);
                gl_info->max_samplers = min(MAX_SAMPLERS, gl_max);
                GL_EXTCALL(glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &gl_max));
                TRACE_(d3d_caps)(" FOUND: ARB Pixel Shader support - max float constants=%u\n", gl_max);
                gl_info->ps_arb_constantsF = gl_max;
                GL_EXTCALL(glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_TEMPORARIES_ARB, &gl_max));
                TRACE_(d3d_caps)(" FOUND: ARB Pixel Shader support - max temporaries=%u\n", gl_max);
                gl_info->ps_arb_max_temps = gl_max;
                GL_EXTCALL(glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_INSTRUCTIONS_ARB, &gl_max));
                TRACE_(d3d_caps)(" FOUND: ARB Pixel Shader support - max instructions=%u\n", gl_max);
                gl_info->ps_arb_max_instructions = gl_max;                
            } else if (strcmp(ThisExtn, "GL_ARB_fragment_shader") == 0) {
                gl_info->supported[ARB_FRAGMENT_SHADER] = TRUE;
                glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &gl_max);
                TRACE_(d3d_caps)(" FOUND: ARB_fragment_shader (GLSL) support - max float ps constants=%u\n", gl_max);
                gl_info->ps_glsl_constantsF = gl_max;
            } else if (strcmp(ThisExtn, "GL_ARB_imaging") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB imaging support\n");
                gl_info->supported[ARB_IMAGING] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_multisample") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Multisample support\n");
                gl_info->supported[ARB_MULTISAMPLE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_multitexture") == 0) {
                glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_max);
                TRACE_(d3d_caps)(" FOUND: ARB Multitexture support - GL_MAX_TEXTURE_UNITS_ARB=%u\n", gl_max);
                gl_info->supported[ARB_MULTITEXTURE] = TRUE;
                gl_info->max_textures = min(MAX_TEXTURES, gl_max);
                gl_info->max_texture_stages = min(MAX_TEXTURES, gl_max);
                gl_info->max_samplers = max(gl_info->max_samplers, gl_max);
            } else if (strcmp(ThisExtn, "GL_ARB_texture_cube_map") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Texture Cube Map support\n");
                gl_info->supported[ARB_TEXTURE_CUBE_MAP] = TRUE;
                TRACE_(d3d_caps)(" IMPLIED: NVIDIA (NV) Texture Gen Reflection support\n");
                gl_info->supported[NV_TEXGEN_REFLECTION] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_texture_compression") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Texture Compression support\n");
                gl_info->supported[ARB_TEXTURE_COMPRESSION] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_texture_env_add") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Texture Env Add support\n");
                gl_info->supported[ARB_TEXTURE_ENV_ADD] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_texture_env_combine") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Texture Env combine support\n");
                gl_info->supported[ARB_TEXTURE_ENV_COMBINE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_texture_env_dot3") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Dot3 support\n");
                gl_info->supported[ARB_TEXTURE_ENV_DOT3] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_texture_border_clamp") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Texture border clamp support\n");
                gl_info->supported[ARB_TEXTURE_BORDER_CLAMP] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_texture_mirrored_repeat") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Texture mirrored repeat support\n");
                gl_info->supported[ARB_TEXTURE_MIRRORED_REPEAT] = TRUE;
            } else if (strcmp(ThisExtn, "GLX_ARB_multisample") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB multisample support\n");
                gl_info->supported[ARB_MULTISAMPLE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_pixel_buffer_object") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Pixel Buffer support\n");
                gl_info->supported[ARB_PIXEL_BUFFER_OBJECT] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_point_sprite") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB point sprite support\n");
                gl_info->supported[ARB_POINT_SPRITE] = TRUE;
            } else if (strstr(ThisExtn, "GL_ARB_vertex_program")) {
                gl_info->vs_arb_version = VS_VERSION_11;
                TRACE_(d3d_caps)(" FOUND: ARB Vertex Shader support - version=%02x\n", gl_info->vs_arb_version);
                gl_info->supported[ARB_VERTEX_PROGRAM] = TRUE;
                GL_EXTCALL(glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &gl_max));
                TRACE_(d3d_caps)(" FOUND: ARB Vertex Shader support - max float constants=%u\n", gl_max);
                gl_info->vs_arb_constantsF = gl_max;
                GL_EXTCALL(glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_TEMPORARIES_ARB, &gl_max));
                TRACE_(d3d_caps)(" FOUND: ARB Vertex Shader support - max temporaries=%u\n", gl_max);
                gl_info->vs_arb_max_temps = gl_max;
                GL_EXTCALL(glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_INSTRUCTIONS_ARB, &gl_max));
                TRACE_(d3d_caps)(" FOUND: ARB Vertex Shader support - max instructions=%u\n", gl_max);
                gl_info->vs_arb_max_instructions = gl_max;
            } else if (strcmp(ThisExtn, "GL_ARB_vertex_shader") == 0) {
                gl_info->supported[ARB_VERTEX_SHADER] = TRUE;
                glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &gl_max);
                TRACE_(d3d_caps)(" FOUND: ARB_vertex_shader (GLSL) support - max float vs constants=%u\n", gl_max);
                gl_info->vs_glsl_constantsF = gl_max;
            } else if (strcmp(ThisExtn, "GL_ARB_vertex_blend") == 0) {
                glGetIntegerv(GL_MAX_VERTEX_UNITS_ARB, &gl_max);
                TRACE_(d3d_caps)(" FOUND: ARB Vertex Blend support GL_MAX_VERTEX_UNITS_ARB %d\n", gl_max);
                gl_info->max_blends = gl_max;
                gl_info->supported[ARB_VERTEX_BLEND] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_vertex_buffer_object") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Vertex Buffer support\n");
                gl_info->supported[ARB_VERTEX_BUFFER_OBJECT] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_occlusion_query") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Occlusion Query support\n");
                gl_info->supported[ARB_OCCLUSION_QUERY] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ARB_point_parameters") == 0) {
                TRACE_(d3d_caps)(" FOUND: ARB Point parameters support\n");
                gl_info->supported[ARB_POINT_PARAMETERS] = TRUE;
            /**
             * EXT
             */
            } else if (strcmp(ThisExtn, "GL_EXT_fog_coord") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Fog coord support\n");
                gl_info->supported[EXT_FOG_COORD] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_framebuffer_object") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Frame Buffer Object support\n");
                gl_info->supported[EXT_FRAMEBUFFER_OBJECT] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_paletted_texture") == 0) { /* handle paletted texture extensions */
                TRACE_(d3d_caps)(" FOUND: EXT Paletted texture support\n");
                gl_info->supported[EXT_PALETTED_TEXTURE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_point_parameters") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Point parameters support\n");
                gl_info->supported[EXT_POINT_PARAMETERS] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_secondary_color") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Secondary coord support\n");
                gl_info->supported[EXT_SECONDARY_COLOR] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_stencil_two_side") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Stencil two side support\n");
                gl_info->supported[EXT_STENCIL_TWO_SIDE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_stencil_wrap") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Stencil wrap support\n");
                gl_info->supported[EXT_STENCIL_WRAP] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_texture3D") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT_texture3D support\n");
                gl_info->supported[EXT_TEXTURE3D] = TRUE;
                glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE_EXT, &gl_max);
                TRACE_(d3d_caps)("Max texture3D size: %d\n", gl_max);
                gl_info->max_texture3d_size = gl_max;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_compression_s3tc") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Texture S3TC compression support\n");
                gl_info->supported[EXT_TEXTURE_COMPRESSION_S3TC] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_env_add") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Texture Env Add support\n");
                gl_info->supported[EXT_TEXTURE_ENV_ADD] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_env_combine") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Texture Env combine support\n");
                gl_info->supported[EXT_TEXTURE_ENV_COMBINE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_env_dot3") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Dot3 support\n");
                gl_info->supported[EXT_TEXTURE_ENV_DOT3] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_filter_anisotropic") == 0) {
                gl_info->supported[EXT_TEXTURE_FILTER_ANISOTROPIC] = TRUE;
                glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max);
                TRACE_(d3d_caps)(" FOUND: EXT Texture Anisotropic filter support. GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT %d\n", gl_max);
                gl_info->max_anisotropy = gl_max;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_lod") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Texture LOD support\n");
                gl_info->supported[EXT_TEXTURE_LOD] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_texture_lod_bias") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Texture LOD bias support\n");
                gl_info->supported[EXT_TEXTURE_LOD_BIAS] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_vertex_weighting") == 0) {
                TRACE_(d3d_caps)(" FOUND: EXT Vertex weighting support\n");
                gl_info->supported[EXT_VERTEX_WEIGHTING] = TRUE;

            /**
             * NVIDIA
             */
            } else if (strstr(ThisExtn, "GL_NV_fog_distance")) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Fog Distance support\n");
                gl_info->supported[NV_FOG_DISTANCE] = TRUE;
            } else if (strstr(ThisExtn, "GL_NV_fragment_program")) {
                gl_info->ps_nv_version = (strcmp(ThisExtn, "GL_NV_fragment_program2") == 0) ? PS_VERSION_30 : PS_VERSION_20;
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Pixel Shader support - version=%02x\n", gl_info->ps_nv_version);
            } else if (strcmp(ThisExtn, "GL_NV_register_combiners") == 0) {
                glGetIntegerv(GL_MAX_GENERAL_COMBINERS_NV, &gl_max);
                gl_info->max_texture_stages = min(MAX_TEXTURES, gl_max);
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Register combiners (1) support - GL_MAX_GENERAL_COMBINERS_NV=%d\n", gl_max);
                gl_info->supported[NV_REGISTER_COMBINERS] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_register_combiners2") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Register combiners (2) support\n");
                gl_info->supported[NV_REGISTER_COMBINERS2] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_texgen_reflection") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Texture Gen Reflection support\n");
                gl_info->supported[NV_TEXGEN_REFLECTION] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_texture_env_combine4") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Texture Env combine (4) support\n");
                gl_info->supported[NV_TEXTURE_ENV_COMBINE4] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_texture_shader") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Texture Shader (1) support\n");
                gl_info->supported[NV_TEXTURE_SHADER] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_texture_shader2") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Texture Shader (2) support\n");
                gl_info->supported[NV_TEXTURE_SHADER2] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_texture_shader3") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Texture Shader (3) support\n");
                gl_info->supported[NV_TEXTURE_SHADER3] = TRUE;
            } else if (strcmp(ThisExtn, "GL_NV_occlusion_query") == 0) {
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Occlusion Query (3) support\n");
                gl_info->supported[NV_OCCLUSION_QUERY] = TRUE;
            } else if (strstr(ThisExtn, "GL_NV_vertex_program")) {
                if(strcmp(ThisExtn, "GL_NV_vertex_program3") == 0)
                    gl_info->vs_nv_version = VS_VERSION_30;
                else if(strcmp(ThisExtn, "GL_NV_vertex_program2") == 0)
                    gl_info->vs_nv_version = VS_VERSION_20;
                else if(strcmp(ThisExtn, "GL_NV_vertex_program1_1") == 0)
                    gl_info->vs_nv_version = VS_VERSION_11;
                else
                    gl_info->vs_nv_version = VS_VERSION_10;
                TRACE_(d3d_caps)(" FOUND: NVIDIA (NV) Vertex Shader support - version=%02x\n", gl_info->vs_nv_version);
                gl_info->supported[NV_VERTEX_PROGRAM] = TRUE;

            /**
             * ATI
             */
            /** TODO */
            } else if (strcmp(ThisExtn, "GL_ATI_separate_stencil") == 0) {
                TRACE_(d3d_caps)(" FOUND: ATI Separate stencil support\n");
                gl_info->supported[ATI_SEPARATE_STENCIL] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ATI_texture_env_combine3") == 0) {
                TRACE_(d3d_caps)(" FOUND: ATI Texture Env combine (3) support\n");
                gl_info->supported[ATI_TEXTURE_ENV_COMBINE3] = TRUE;
            } else if (strcmp(ThisExtn, "GL_ATI_texture_mirror_once") == 0) {
                TRACE_(d3d_caps)(" FOUND: ATI Texture Mirror Once support\n");
                gl_info->supported[ATI_TEXTURE_MIRROR_ONCE] = TRUE;
            } else if (strcmp(ThisExtn, "GL_EXT_vertex_shader") == 0) {
                gl_info->vs_ati_version = VS_VERSION_11;
                TRACE_(d3d_caps)(" FOUND: ATI (EXT) Vertex Shader support - version=%02x\n", gl_info->vs_ati_version);
                gl_info->supported[EXT_VERTEX_SHADER] = TRUE;
            }


            if (*GL_Extensions == ' ') GL_Extensions++;
        }
    }
    checkGLcall("extension detection\n");

    gl_info->max_sampler_stages = max(gl_info->max_samplers, gl_info->max_texture_stages);

    /* Load all the lookup tables
    TODO: It may be a good idea to make minLookup and maxLookup const and populate them in wined3d_private.h where they are declared */
    minLookup[WINELOOKUP_WARPPARAM] = D3DTADDRESS_WRAP;
    maxLookup[WINELOOKUP_WARPPARAM] = D3DTADDRESS_MIRRORONCE;

    minLookup[WINELOOKUP_MAGFILTER] = WINED3DTEXF_NONE;
    maxLookup[WINELOOKUP_MAGFILTER] = WINED3DTEXF_ANISOTROPIC;


    for (i = 0; i < MAX_LOOKUPS; i++) {
        stateLookup[i] = HeapAlloc(GetProcessHeap(), 0, sizeof(*stateLookup[i]) * (1 + maxLookup[i] - minLookup[i]) );
    }

    stateLookup[WINELOOKUP_WARPPARAM][D3DTADDRESS_WRAP   - minLookup[WINELOOKUP_WARPPARAM]] = GL_REPEAT;
    stateLookup[WINELOOKUP_WARPPARAM][D3DTADDRESS_CLAMP  - minLookup[WINELOOKUP_WARPPARAM]] = GL_CLAMP_TO_EDGE;
    stateLookup[WINELOOKUP_WARPPARAM][D3DTADDRESS_BORDER - minLookup[WINELOOKUP_WARPPARAM]] =
             gl_info->supported[ARB_TEXTURE_BORDER_CLAMP] ? GL_CLAMP_TO_BORDER_ARB : GL_REPEAT;
    stateLookup[WINELOOKUP_WARPPARAM][D3DTADDRESS_BORDER - minLookup[WINELOOKUP_WARPPARAM]] =
             gl_info->supported[ARB_TEXTURE_BORDER_CLAMP] ? GL_CLAMP_TO_BORDER_ARB : GL_REPEAT;
    stateLookup[WINELOOKUP_WARPPARAM][D3DTADDRESS_MIRROR - minLookup[WINELOOKUP_WARPPARAM]] =
             gl_info->supported[ARB_TEXTURE_MIRRORED_REPEAT] ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT;
    stateLookup[WINELOOKUP_WARPPARAM][D3DTADDRESS_MIRRORONCE - minLookup[WINELOOKUP_WARPPARAM]] =
             gl_info->supported[ATI_TEXTURE_MIRROR_ONCE] ? GL_MIRROR_CLAMP_TO_EDGE_ATI : GL_REPEAT;

    stateLookup[WINELOOKUP_MAGFILTER][WINED3DTEXF_NONE        - minLookup[WINELOOKUP_MAGFILTER]]  = GL_NEAREST;
    stateLookup[WINELOOKUP_MAGFILTER][WINED3DTEXF_POINT       - minLookup[WINELOOKUP_MAGFILTER]] = GL_NEAREST;
    stateLookup[WINELOOKUP_MAGFILTER][WINED3DTEXF_LINEAR      - minLookup[WINELOOKUP_MAGFILTER]] = GL_LINEAR;
    stateLookup[WINELOOKUP_MAGFILTER][WINED3DTEXF_ANISOTROPIC - minLookup[WINELOOKUP_MAGFILTER]] =
             gl_info->supported[EXT_TEXTURE_FILTER_ANISOTROPIC] ? GL_LINEAR : GL_NEAREST;


    minMipLookup[WINED3DTEXF_NONE][WINED3DTEXF_NONE]     = GL_LINEAR;
    minMipLookup[WINED3DTEXF_NONE][WINED3DTEXF_POINT]    = GL_LINEAR;
    minMipLookup[WINED3DTEXF_NONE][WINED3DTEXF_LINEAR]   = GL_LINEAR;
    minMipLookup[WINED3DTEXF_POINT][WINED3DTEXF_NONE]    = GL_NEAREST;
    minMipLookup[WINED3DTEXF_POINT][WINED3DTEXF_POINT]   = GL_NEAREST_MIPMAP_NEAREST;
    minMipLookup[WINED3DTEXF_POINT][WINED3DTEXF_LINEAR]  = GL_NEAREST_MIPMAP_LINEAR;
    minMipLookup[WINED3DTEXF_LINEAR][WINED3DTEXF_NONE]   = GL_LINEAR;
    minMipLookup[WINED3DTEXF_LINEAR][WINED3DTEXF_POINT]  = GL_LINEAR_MIPMAP_NEAREST;
    minMipLookup[WINED3DTEXF_LINEAR][WINED3DTEXF_LINEAR] = GL_LINEAR_MIPMAP_LINEAR;
    minMipLookup[WINED3DTEXF_ANISOTROPIC][WINED3DTEXF_NONE]   = gl_info->supported[EXT_TEXTURE_FILTER_ANISOTROPIC] ?
    GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    minMipLookup[WINED3DTEXF_ANISOTROPIC][WINED3DTEXF_POINT]  = gl_info->supported[EXT_TEXTURE_FILTER_ANISOTROPIC] ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;
    minMipLookup[WINED3DTEXF_ANISOTROPIC][WINED3DTEXF_LINEAR] = gl_info->supported[EXT_TEXTURE_FILTER_ANISOTROPIC] ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;

/* TODO: config lookups */

    if (display != NULL) {
        GLX_Extensions = glXQueryExtensionsString(display, DefaultScreen(display));
        TRACE_(d3d_caps)("GLX_Extensions reported:\n");

        if (NULL == GLX_Extensions) {
            ERR("   GLX_Extensions returns NULL\n");
        } else {
            while (*GLX_Extensions != 0x00) {
                const char *Start = GLX_Extensions;
                char ThisExtn[256];

                memset(ThisExtn, 0x00, sizeof(ThisExtn));
                while (*GLX_Extensions != ' ' && *GLX_Extensions != 0x00) {
                    GLX_Extensions++;
                }
                memcpy(ThisExtn, Start, (GLX_Extensions - Start));
                TRACE_(d3d_caps)("- %s\n", ThisExtn);
                if (*GLX_Extensions == ' ') GLX_Extensions++;
            }
        }
    }

    /* If we created a dummy context, throw it away */
    if (NULL != fake_ctx) WineD3D_ReleaseFakeGLContext(fake_ctx);

    /* Only save the values obtained when a display is provided */
    if (fake_ctx == NULL) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/**********************************************************
 * IWineD3D implementation follows
 **********************************************************/

static UINT     WINAPI IWineD3DImpl_GetAdapterCount (IWineD3D *iface) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;

    /* FIXME: Set to one for now to imply the display */
    TRACE_(d3d_caps)("(%p): Mostly stub, only returns primary display\n", This);
    return 1;
}

static HRESULT  WINAPI IWineD3DImpl_RegisterSoftwareDevice(IWineD3D *iface, void* pInitializeFunction) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    FIXME("(%p)->(%p): stub\n", This, pInitializeFunction);
    return WINED3D_OK;
}

static HMONITOR WINAPI IWineD3DImpl_GetAdapterMonitor(IWineD3D *iface, UINT Adapter) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    POINT pt = { -1, -1 };

    if (Adapter >= IWineD3DImpl_GetAdapterCount(iface)) {
        return NULL;
    }

    FIXME_(d3d_caps)("(%p): returning the primary monitor for adapter %d\n", This, Adapter);
    return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
}

/* FIXME: GetAdapterModeCount and EnumAdapterModes currently only returns modes
     of the same bpp but different resolutions                                  */

/* Note: dx9 supplies a format. Calls from d3d8 supply D3DFMT_UNKNOWN */
static UINT     WINAPI IWineD3DImpl_GetAdapterModeCount(IWineD3D *iface, UINT Adapter, WINED3DFORMAT Format) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    TRACE_(d3d_caps)("(%p}->(Adapter: %d, Format: %s)\n", This, Adapter, debug_d3dformat(Format));

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return 0;
    }

    if (Adapter == 0) { /* Display */
        int i = 0;
        int j = 0;
#if !defined( DEBUG_SINGLE_MODE )
        DEVMODEW DevModeW;

        /* Work out the current screen bpp */
        HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
        int bpp = GetDeviceCaps(hdc, BITSPIXEL);
        DeleteDC(hdc);

        while (EnumDisplaySettingsExW(NULL, j, &DevModeW, 0)) {
            j++;
            switch (Format)
            {
            case D3DFMT_UNKNOWN:
                   i++;
                   break;
            case D3DFMT_X8R8G8B8:
            case D3DFMT_A8R8G8B8:
                   if (min(DevModeW.dmBitsPerPel, bpp) == 32) i++;
                   if (min(DevModeW.dmBitsPerPel, bpp) == 24) i++;
                   break;
            case D3DFMT_X1R5G5B5:
            case D3DFMT_A1R5G5B5:
            case D3DFMT_R5G6B5:
                   if (min(DevModeW.dmBitsPerPel, bpp) == 16) i++;
                   break;
            default:
                   /* Skip other modes as they do not match requested format */
                   break;
            }
        }
#else
        i = 1;
        j = 1;
#endif
        TRACE_(d3d_caps)("(%p}->(Adapter: %d) => %d (out of %d)\n", This, Adapter, i, j);
        return i;
    } else {
        FIXME_(d3d_caps)("Adapter not primary display\n");
    }
    return 0;
}

/* Note: dx9 supplies a format. Calls from d3d8 supply D3DFMT_UNKNOWN */
static HRESULT WINAPI IWineD3DImpl_EnumAdapterModes(IWineD3D *iface, UINT Adapter, WINED3DFORMAT Format, UINT Mode, WINED3DDISPLAYMODE* pMode) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    TRACE_(d3d_caps)("(%p}->(Adapter:%d, mode:%d, pMode:%p, format:%s)\n", This, Adapter, Mode, pMode, debug_d3dformat(Format));

    /* Validate the parameters as much as possible */
    if (NULL == pMode ||
        Adapter >= IWineD3DImpl_GetAdapterCount(iface) ||
        Mode    >= IWineD3DImpl_GetAdapterModeCount(iface, Adapter, Format)) {
        return WINED3DERR_INVALIDCALL;
    }

    if (Adapter == 0) { /* Display */
#if !defined( DEBUG_SINGLE_MODE )
        DEVMODEW DevModeW;
        int ModeIdx = 0;

        /* Work out the current screen bpp */
        HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
        int bpp = GetDeviceCaps(hdc, BITSPIXEL);
        DeleteDC(hdc);

        /* If we are filtering to a specific format, then need to skip all unrelated
           modes, but if mode is irrelevant, then we can use the index directly      */
        if (Format == D3DFMT_UNKNOWN)
        {
            ModeIdx = Mode;
        } else {
            int i = 0;
            int j = 0;
            DEVMODEW DevModeWtmp;


            while (i<(Mode) && EnumDisplaySettingsExW(NULL, j, &DevModeWtmp, 0)) {
                j++;
                switch (Format)
                {
                case D3DFMT_UNKNOWN:
                       i++;
                       break;
                case D3DFMT_X8R8G8B8:
                case D3DFMT_A8R8G8B8:
                       if (min(DevModeWtmp.dmBitsPerPel, bpp) == 32) i++;
                       if (min(DevModeWtmp.dmBitsPerPel, bpp) == 24) i++;
                       break;
                case D3DFMT_X1R5G5B5:
                case D3DFMT_A1R5G5B5:
                case D3DFMT_R5G6B5:
                       if (min(DevModeWtmp.dmBitsPerPel, bpp) == 16) i++;
                       break;
                default:
                       /* Skip other modes as they do not match requested format */
                       break;
                }
            }
            ModeIdx = j;
        }

        /* Now get the display mode via the calculated index */
        if (EnumDisplaySettingsExW(NULL, ModeIdx, &DevModeW, 0))
        {
            pMode->Width        = DevModeW.dmPelsWidth;
            pMode->Height       = DevModeW.dmPelsHeight;
            bpp                 = min(DevModeW.dmBitsPerPel, bpp);
            pMode->RefreshRate  = D3DADAPTER_DEFAULT;
            if (DevModeW.dmFields & DM_DISPLAYFREQUENCY)
            {
                pMode->RefreshRate = DevModeW.dmDisplayFrequency;
            }

            if (Format == D3DFMT_UNKNOWN)
            {
                switch (bpp) {
                case  8: pMode->Format = D3DFMT_R3G3B2;   break;
                case 16: pMode->Format = D3DFMT_R5G6B5;   break;
                case 24: /* Robots and EVE Online need 24 and 32 bit as A8R8G8B8 to start */
                case 32: pMode->Format = D3DFMT_A8R8G8B8; break;
                default: pMode->Format = D3DFMT_UNKNOWN;
                }
            } else {
                pMode->Format = Format;
            }
        }
        else
        {
            TRACE_(d3d_caps)("Requested mode out of range %d\n", Mode);
            return WINED3DERR_INVALIDCALL;
        }

#else
        /* Return one setting of the format requested */
        if (Mode > 0) return WINED3DERR_INVALIDCALL;
        pMode->Width        = 800;
        pMode->Height       = 600;
        pMode->RefreshRate  = D3DADAPTER_DEFAULT;
        pMode->Format       = (Format == D3DFMT_UNKNOWN) ? D3DFMT_A8R8G8B8 : Format;
        bpp = 32;
#endif
        TRACE_(d3d_caps)("W %d H %d rr %d fmt (%x - %s) bpp %u\n", pMode->Width, pMode->Height,
                 pMode->RefreshRate, pMode->Format, debug_d3dformat(pMode->Format), bpp);

    } else {
        FIXME_(d3d_caps)("Adapter not primary display\n");
    }

    return WINED3D_OK;
}

static HRESULT WINAPI IWineD3DImpl_GetAdapterDisplayMode(IWineD3D *iface, UINT Adapter, WINED3DDISPLAYMODE* pMode) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    TRACE_(d3d_caps)("(%p}->(Adapter: %d, pMode: %p)\n", This, Adapter, pMode);

    if (NULL == pMode ||
        Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    if (Adapter == 0) { /* Display */
        int bpp = 0;
        DEVMODEW DevModeW;

        EnumDisplaySettingsExW(NULL, (DWORD)-1, &DevModeW, 0);
        pMode->Width        = DevModeW.dmPelsWidth;
        pMode->Height       = DevModeW.dmPelsHeight;
        bpp                 = DevModeW.dmBitsPerPel;
        pMode->RefreshRate  = D3DADAPTER_DEFAULT;
        if (DevModeW.dmFields&DM_DISPLAYFREQUENCY)
        {
            pMode->RefreshRate = DevModeW.dmDisplayFrequency;
        }

        switch (bpp) {
        case  8: pMode->Format       = D3DFMT_R3G3B2;   break;
        case 16: pMode->Format       = D3DFMT_R5G6B5;   break;
        case 24: pMode->Format       = D3DFMT_X8R8G8B8; break; /* Robots needs 24bit to be X8R8G8B8 */
        case 32: pMode->Format       = D3DFMT_X8R8G8B8; break; /* EVE online and the Fur demo need 32bit AdapterDisplatMode to return X8R8G8B8 */
        default: pMode->Format       = D3DFMT_UNKNOWN;
        }

    } else {
        FIXME_(d3d_caps)("Adapter not primary display\n");
    }

    TRACE_(d3d_caps)("returning w:%d, h:%d, ref:%d, fmt:%s\n", pMode->Width,
          pMode->Height, pMode->RefreshRate, debug_d3dformat(pMode->Format));
    return WINED3D_OK;
}

static Display * WINAPI IWineD3DImpl_GetAdapterDisplay(IWineD3D *iface, UINT Adapter) {
    Display *display;
    HDC     device_context;
    /* only works with one adapter at the moment... */

    /* Get the display */
    device_context = GetDC(0);
    display = get_display(device_context);
    ReleaseDC(0, device_context);
    return display;
}

/* NOTE: due to structure differences between dx8 and dx9 D3DADAPTER_IDENTIFIER,
   and fields being inserted in the middle, a new structure is used in place    */
static HRESULT WINAPI IWineD3DImpl_GetAdapterIdentifier(IWineD3D *iface, UINT Adapter, DWORD Flags,
                                                   WINED3DADAPTER_IDENTIFIER* pIdentifier) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;

    TRACE_(d3d_caps)("(%p}->(Adapter: %d, Flags: %lx, pId=%p)\n", This, Adapter, Flags, pIdentifier);

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    if (Adapter == 0) { /* Display - only device supported for now */

        BOOL isGLInfoValid = This->isGLInfoValid;

        /* FillGLCaps updates gl_info, but we only want to store and
           reuse the values once we have a context which is valid. Values from
           a temporary context may differ from the final ones                 */
        if (isGLInfoValid == FALSE) {
            WineD3D_Context *fake_ctx = NULL;
            if (glXGetCurrentContext() == NULL) fake_ctx = WineD3D_CreateFakeGLContext();
            /* If we don't know the device settings, go query them now */
            isGLInfoValid = IWineD3DImpl_FillGLCaps(iface, IWineD3DImpl_GetAdapterDisplay(iface, Adapter));
            if (fake_ctx != NULL) WineD3D_ReleaseFakeGLContext(fake_ctx);
        }

        /* If it worked, return the information requested */
        if (isGLInfoValid) {
          TRACE_(d3d_caps)("device/Vendor Name and Version detection using FillGLCaps\n");
          strcpy(pIdentifier->Driver, "Display");
          strcpy(pIdentifier->Description, "Direct3D HAL");

          /* Note dx8 doesn't supply a DeviceName */
          if (NULL != pIdentifier->DeviceName) strcpy(pIdentifier->DeviceName, "\\\\.\\DISPLAY"); /* FIXME: May depend on desktop? */
          /* Current Windows drivers have versions like 6.14.... (some older have an earlier version) */
          pIdentifier->DriverVersion->u.HighPart = MAKEDWORD_VERSION(6, 14);
          pIdentifier->DriverVersion->u.LowPart = This->gl_info.gl_driver_version;
          *(pIdentifier->VendorId) = This->gl_info.gl_vendor;
          *(pIdentifier->DeviceId) = This->gl_info.gl_card;
          *(pIdentifier->SubSysId) = 0;
          *(pIdentifier->Revision) = 0;

        } else {

          /* If it failed, return dummy values from an NVidia driver */
          WARN_(d3d_caps)("Cannot get GLCaps for device/Vendor Name and Version detection using FillGLCaps, currently using NVIDIA identifiers\n");
          strcpy(pIdentifier->Driver, "Display");
          strcpy(pIdentifier->Description, "Direct3D HAL");
          if (NULL != pIdentifier->DeviceName) strcpy(pIdentifier->DeviceName, "\\\\.\\DISPLAY"); /* FIXME: May depend on desktop? */
          /* Current Windows Nvidia drivers have versions like e.g. 6.14.10.5672 */
          pIdentifier->DriverVersion->u.HighPart = MAKEDWORD_VERSION(6, 14);
          /* 71.74 is a current Linux Nvidia driver version */
          pIdentifier->DriverVersion->u.LowPart = MAKEDWORD_VERSION(10, (71*100+74));
          *(pIdentifier->VendorId) = VENDOR_NVIDIA;
          *(pIdentifier->DeviceId) = CARD_NVIDIA_GEFORCE4_TI4600;
          *(pIdentifier->SubSysId) = 0;
          *(pIdentifier->Revision) = 0;
        }

        /*FIXME: memcpy(&pIdentifier->DeviceIdentifier, ??, sizeof(??GUID)); */
        if (Flags & D3DENUM_NO_WHQL_LEVEL) {
            *(pIdentifier->WHQLLevel) = 0;
        } else {
            *(pIdentifier->WHQLLevel) = 1;
        }

    } else {
        FIXME_(d3d_caps)("Adapter not primary display\n");
    }

    return WINED3D_OK;
}

static BOOL IWineD3DImpl_IsGLXFBConfigCompatibleWithRenderFmt(WineD3D_Context* ctx, GLXFBConfig cfgs, WINED3DFORMAT Format) {
#if 0 /* This code performs a strict test between the format and the current X11  buffer depth, which may give the best performance */
  int gl_test;
  int rb, gb, bb, ab, type, buf_sz;

  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_RED_SIZE,   &rb);
  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_GREEN_SIZE, &gb);
  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_BLUE_SIZE,  &bb);
  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_ALPHA_SIZE, &ab);
  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_RENDER_TYPE, &type);
  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_BUFFER_SIZE, &buf_sz);

  switch (Format) {
  case WINED3DFMT_X8R8G8B8:
  case WINED3DFMT_R8G8B8:
    if (8 == rb && 8 == gb && 8 == bb) return TRUE;
    break;
  case WINED3DFMT_A8R8G8B8:
    if (8 == rb && 8 == gb && 8 == bb && 8 == ab) return TRUE;
    break;
  case WINED3DFMT_A2R10G10B10:
    if (10 == rb && 10 == gb && 10 == bb && 2 == ab) return TRUE;
    break;
  case WINED3DFMT_X1R5G5B5:
    if (5 == rb && 5 == gb && 5 == bb) return TRUE;
    break;
  case WINED3DFMT_A1R5G5B5:
    if (5 == rb && 5 == gb && 5 == bb && 1 == ab) return TRUE;
    break;
  case WINED3DFMT_X4R4G4B4:
    if (16 == buf_sz && 4 == rb && 4 == gb && 4 == bb) return TRUE;
    break;
  case WINED3DFMT_R5G6B5:
    if (5 == rb && 6 == gb && 5 == bb) return TRUE;
    break;
  case WINED3DFMT_R3G3B2:
    if (3 == rb && 3 == gb && 2 == bb) return TRUE;
    break;
  case WINED3DFMT_A8P8:
    if (type & GLX_COLOR_INDEX_BIT && 8 == buf_sz && 8 == ab) return TRUE;
    break;
  case WINED3DFMT_P8:
    if (type & GLX_COLOR_INDEX_BIT && 8 == buf_sz) return TRUE;
    break;
  default:
    ERR("unsupported format %s\n", debug_d3dformat(Format));
    break;
  }
  return FALSE;
#else /* Most of the time performance is less of an issue than compatibility, this code allows for most common opengl/d3d formats */
switch (Format) {
  case WINED3DFMT_X8R8G8B8:
  case WINED3DFMT_R8G8B8:
  case WINED3DFMT_A8R8G8B8:
  case WINED3DFMT_A2R10G10B10:
  case WINED3DFMT_X1R5G5B5:
  case WINED3DFMT_A1R5G5B5:
  case WINED3DFMT_R5G6B5:
  case WINED3DFMT_R3G3B2:
  case WINED3DFMT_A8P8:
  case WINED3DFMT_P8:
return TRUE;
  default:
    ERR("unsupported format %s\n", debug_d3dformat(Format));
    break;
  }
return FALSE;
#endif
}

static BOOL IWineD3DImpl_IsGLXFBConfigCompatibleWithDepthFmt(WineD3D_Context* ctx, GLXFBConfig cfgs, WINED3DFORMAT Format) {
#if 0/* This code performs a strict test between the format and the current X11  buffer depth, which may give the best performance */
  int gl_test;
  int db, sb;

  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_DEPTH_SIZE, &db);
  gl_test = glXGetFBConfigAttrib(ctx->display, cfgs, GLX_STENCIL_SIZE, &sb);

  switch (Format) {
  case WINED3DFMT_D16:
  case WINED3DFMT_D16_LOCKABLE:
    if (16 == db) return TRUE;
    break;
  case WINED3DFMT_D32:
    if (32 == db) return TRUE;
    break;
  case WINED3DFMT_D15S1:
    if (15 == db) return TRUE;
    break;
  case WINED3DFMT_D24S8:
    if (24 == db && 8 == sb) return TRUE;
    break;
  case WINED3DFMT_D24FS8:
    if (24 == db && 8 == sb) return TRUE;
    break;
  case WINED3DFMT_D24X8:
    if (24 == db) return TRUE;
    break;
  case WINED3DFMT_D24X4S4:
    if (24 == db && 4 == sb) return TRUE;
    break;
  case WINED3DFMT_D32F_LOCKABLE:
    if (32 == db) return TRUE;
    break;
  default:
    ERR("unsupported format %s\n", debug_d3dformat(Format));
    break;
  }
  return FALSE;
#else /* Most of the time performance is less of an issue than compatibility, this code allows for most common opengl/d3d formats */
  switch (Format) {
  case WINED3DFMT_D16:
  case WINED3DFMT_D16_LOCKABLE:
  case WINED3DFMT_D32:
  case WINED3DFMT_D15S1:
  case WINED3DFMT_D24S8:
  case WINED3DFMT_D24FS8:
  case WINED3DFMT_D24X8:
  case WINED3DFMT_D24X4S4:
  case WINED3DFMT_D32F_LOCKABLE:
    return TRUE;
  default:
    ERR("unsupported format %s\n", debug_d3dformat(Format));
    break;
  }
  return FALSE;
#endif
}

static HRESULT WINAPI IWineD3DImpl_CheckDepthStencilMatch(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE DeviceType,
                                                   WINED3DFORMAT AdapterFormat,
                                                   WINED3DFORMAT RenderTargetFormat,
                                                   WINED3DFORMAT DepthStencilFormat) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    HRESULT hr = WINED3DERR_NOTAVAILABLE;
    WineD3D_Context* ctx = NULL;
    GLXFBConfig* cfgs = NULL;
    int nCfgs = 0;
    int it;

    WARN_(d3d_caps)("(%p)-> (STUB) (Adptr:%d, DevType:(%x,%s), AdptFmt:(%x,%s), RendrTgtFmt:(%x,%s), DepthStencilFmt:(%x,%s))\n",
           This, Adapter,
           DeviceType, debug_d3ddevicetype(DeviceType),
           AdapterFormat, debug_d3dformat(AdapterFormat),
           RenderTargetFormat, debug_d3dformat(RenderTargetFormat),
           DepthStencilFormat, debug_d3dformat(DepthStencilFormat));

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        TRACE("(%p) Failed: Atapter (%u) higher than supported adapters (%u) returning WINED3DERR_INVALIDCALL\n", This, Adapter, IWineD3D_GetAdapterCount(iface));
        return WINED3DERR_INVALIDCALL;
    }
    /* TODO: use the real context if it's available */
    ctx = WineD3D_CreateFakeGLContext();
    if(NULL !=  ctx) {
        cfgs = glXGetFBConfigs(ctx->display, DefaultScreen(ctx->display), &nCfgs);
    } else {
        TRACE_(d3d_caps)("(%p) : Unable to create a fake context at this time (there may already be an active context)\n", This);
    }

    if (NULL != cfgs) {
        for (it = 0; it < nCfgs; ++it) {
            if (IWineD3DImpl_IsGLXFBConfigCompatibleWithRenderFmt(ctx, cfgs[it], RenderTargetFormat)) {
                if (IWineD3DImpl_IsGLXFBConfigCompatibleWithDepthFmt(ctx, cfgs[it], DepthStencilFormat)) {
                    hr = WINED3D_OK;
                    break ;
                }
            }
        }
        XFree(cfgs);
        cfgs = NULL;
    } else {
        /* If there's a corrent context then we cannot create a fake one so pass everything */
        hr = WINED3D_OK;
    }

    if (ctx != NULL)
        WineD3D_ReleaseFakeGLContext(ctx);

    if (hr != WINED3D_OK)
        TRACE_(d3d_caps)("Failed to match stencil format to device\b");

    TRACE_(d3d_caps)("(%p) : Returning %lx\n", This, hr);
    return hr;
}

static HRESULT WINAPI IWineD3DImpl_CheckDeviceMultiSampleType(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE DeviceType, 
                                                       WINED3DFORMAT SurfaceFormat,
                                                       BOOL Windowed, WINED3DMULTISAMPLE_TYPE MultiSampleType, DWORD*   pQualityLevels) {

    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    TRACE_(d3d_caps)("(%p)-> (STUB) (Adptr:%d, DevType:(%x,%s), SurfFmt:(%x,%s), Win?%d, MultiSamp:%x, pQual:%p)\n",
          This,
          Adapter,
          DeviceType, debug_d3ddevicetype(DeviceType),
          SurfaceFormat, debug_d3dformat(SurfaceFormat),
          Windowed,
          MultiSampleType,
          pQualityLevels);

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    if (pQualityLevels != NULL) {
        static int s_single_shot = 0;
        if (!s_single_shot) {
            FIXME("Quality levels unsupported at present\n");
            s_single_shot = 1;
        }
        *pQualityLevels = 1; /* Guess at a value! */
    }

    if (WINED3DMULTISAMPLE_NONE == MultiSampleType) return WINED3D_OK;
    return WINED3DERR_NOTAVAILABLE;
}

static HRESULT WINAPI IWineD3DImpl_CheckDeviceType(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE CheckType,
                                            WINED3DFORMAT DisplayFormat, WINED3DFORMAT BackBufferFormat, BOOL Windowed) {

    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    TRACE_(d3d_caps)("(%p)-> (STUB) (Adptr:%d, CheckType:(%x,%s), DispFmt:(%x,%s), BackBuf:(%x,%s), Win?%d): stub\n",
          This,
          Adapter,
          CheckType, debug_d3ddevicetype(CheckType),
          DisplayFormat, debug_d3dformat(DisplayFormat),
          BackBufferFormat, debug_d3dformat(BackBufferFormat),
          Windowed);

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    {
      GLXFBConfig* cfgs = NULL;
      int nCfgs = 0;
      int it;
      HRESULT hr = WINED3DERR_NOTAVAILABLE;

      WineD3D_Context* ctx = WineD3D_CreateFakeGLContext();
      if (NULL != ctx) {
        cfgs = glXGetFBConfigs(ctx->display, DefaultScreen(ctx->display), &nCfgs);
        for (it = 0; it < nCfgs; ++it) {
            if (IWineD3DImpl_IsGLXFBConfigCompatibleWithRenderFmt(ctx, cfgs[it], DisplayFormat)) {
                hr = WINED3D_OK;
                break ;
            }
        }
        XFree(cfgs);

        WineD3D_ReleaseFakeGLContext(ctx);
        return hr;
      }
    }

    return WINED3DERR_NOTAVAILABLE;
}

static HRESULT WINAPI IWineD3DImpl_CheckDeviceFormat(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE DeviceType, 
                                              WINED3DFORMAT AdapterFormat, DWORD Usage, WINED3DRESOURCETYPE RType, WINED3DFORMAT CheckFormat) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    TRACE_(d3d_caps)("(%p)-> (STUB) (Adptr:%d, DevType:(%u,%s), AdptFmt:(%u,%s), Use:(%lu,%s,%s), ResTyp:(%x,%s), CheckFmt:(%u,%s)) ",
          This,
          Adapter,
          DeviceType, debug_d3ddevicetype(DeviceType),
          AdapterFormat, debug_d3dformat(AdapterFormat),
          Usage, debug_d3dusage(Usage), debug_d3dusagequery(Usage),
          RType, debug_d3dresourcetype(RType),
          CheckFormat, debug_d3dformat(CheckFormat));

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    /* TODO: Check support against more of the WINED3DUSAGE_QUERY_* constants
     * See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directx9_c/IDirect3D9__CheckDeviceFormat.asp
     * and http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directx9_c/D3DUSAGE_QUERY.asp */
    if (Usage & WINED3DUSAGE_QUERY_VERTEXTEXTURE) {
        TRACE_(d3d_caps)("[FAILED]\n");
        return WINED3DERR_NOTAVAILABLE;     /* Enable when fully supported */
    }
    
    if(Usage & WINED3DUSAGE_DEPTHSTENCIL) {
        switch (CheckFormat) {
            case WINED3DFMT_D16_LOCKABLE:
            case WINED3DFMT_D32:
            case WINED3DFMT_D15S1:
            case WINED3DFMT_D24S8:
            case WINED3DFMT_D24X8:
            case WINED3DFMT_D24X4S4:
            case WINED3DFMT_D16:
            case WINED3DFMT_L16:
            case WINED3DFMT_D32F_LOCKABLE:
            case WINED3DFMT_D24FS8:
                TRACE_(d3d_caps)("[OK]\n");
                return WINED3D_OK;
            default:
                TRACE_(d3d_caps)("[FAILED]\n");
                return WINED3DERR_NOTAVAILABLE;
        }
    } else if(Usage & WINED3DUSAGE_RENDERTARGET) {
        switch (CheckFormat) {
            case WINED3DFMT_R8G8B8:
            case WINED3DFMT_A8R8G8B8:
            case WINED3DFMT_X8R8G8B8:
            case WINED3DFMT_R5G6B5:
            case WINED3DFMT_X1R5G5B5:
            case WINED3DFMT_A1R5G5B5:
            case WINED3DFMT_A4R4G4B4:
            case WINED3DFMT_R3G3B2:
            case WINED3DFMT_X4R4G4B4:
            case WINED3DFMT_A8B8G8R8:
            case WINED3DFMT_X8B8G8R8:
            case WINED3DFMT_P8:
                TRACE_(d3d_caps)("[OK]\n");
                return WINED3D_OK;
            default:
                TRACE_(d3d_caps)("[FAILED]\n");
                return WINED3DERR_NOTAVAILABLE;
        }
    }

    if (GL_SUPPORT(EXT_TEXTURE_COMPRESSION_S3TC)) {
        switch (CheckFormat) {
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
          TRACE_(d3d_caps)("[OK]\n");
          return WINED3D_OK;
        default:
            break; /* Avoid compiler warnings */
        }
    }

    /* This format is nothing special and it is supported perfectly.
     * However, ati and nvidia driver on windows do not mark this format
     * supported(tested with the dxCapsViewer) and pretending to
     * support this format uncovers a bug in Battlefield 1942(fonts are missing)
     * So do the same as Windows drivers and pretend not to support it on dx8 and 9
     * Enable it on dx7. It will need additional checking on dx10 when we support it.
     */
    if(This->dxVersion > 7 && CheckFormat == WINED3DFMT_R8G8B8) {
        TRACE_(d3d_caps)("[FAILED]\n");
        return WINED3DERR_NOTAVAILABLE;
    }

    switch (CheckFormat) {

        /*****
         *  supported: RGB(A) formats
         */
        case WINED3DFMT_R8G8B8: /* Enable for dx7, blacklisted for 8 and 9 above */
        case WINED3DFMT_A8R8G8B8:
        case WINED3DFMT_X8R8G8B8:
        case WINED3DFMT_R5G6B5:
        case WINED3DFMT_X1R5G5B5:
        case WINED3DFMT_A1R5G5B5:
        case WINED3DFMT_A4R4G4B4:
        case WINED3DFMT_R3G3B2:
        case WINED3DFMT_A8:
        case WINED3DFMT_A8R3G3B2:
        case WINED3DFMT_X4R4G4B4:
        case WINED3DFMT_A8B8G8R8:
        case WINED3DFMT_X8B8G8R8:
        case WINED3DFMT_A2R10G10B10:
        case WINED3DFMT_A2B10G10R10:
            TRACE_(d3d_caps)("[OK]\n");
            return WINED3D_OK;

        /*****
         *  supported: Palettized
         */
        case WINED3DFMT_P8:
            TRACE_(d3d_caps)("[OK]\n");
            return WINED3D_OK;

        /*****
         *  Supported: (Alpha)-Luminance
         */
        case WINED3DFMT_L8:
        case WINED3DFMT_A8L8:
        case WINED3DFMT_A4L4:
            TRACE_(d3d_caps)("[OK]\n");
            return WINED3D_OK;

        /*****
         *  Not supported for now: Bump mapping formats
         *  Enable some because games often fail when they are not available
         *  and are still playable even without bump mapping
         */
        case WINED3DFMT_V8U8:
        case WINED3DFMT_V16U16:
        case WINED3DFMT_L6V5U5:
        case WINED3DFMT_X8L8V8U8:
        case WINED3DFMT_Q8W8V8U8:
        case WINED3DFMT_W11V11U10:
        case WINED3DFMT_A2W10V10U10:
            WARN_(d3d_caps)("[Not supported, but pretended to do]\n");
            return WINED3D_OK;

        /*****
         *  DXTN Formats: Handled above
         * WINED3DFMT_DXT1
         * WINED3DFMT_DXT2
         * WINED3DFMT_DXT3
         * WINED3DFMT_DXT4
         * WINED3DFMT_DXT5
         */

        /*****
         *  Odd formats - not supported
         */
        case WINED3DFMT_VERTEXDATA:
        case WINED3DFMT_INDEX16:
        case WINED3DFMT_INDEX32:
        case WINED3DFMT_Q16W16V16U16:
            TRACE_(d3d_caps)("[FAILED]\n"); /* Enable when implemented */
            return WINED3DERR_NOTAVAILABLE;

        /*****
         *  Float formats: Not supported right now
         */
        case WINED3DFMT_R16F:
        case WINED3DFMT_G16R16F:
        case WINED3DFMT_A16B16G16R16F:
        case WINED3DFMT_R32F:
        case WINED3DFMT_G32R32F:
        case WINED3DFMT_A32B32G32R32F:
        case WINED3DFMT_CxV8U8:
            TRACE_(d3d_caps)("[FAILED]\n"); /* Enable when implemented */
            return WINED3DERR_NOTAVAILABLE;

            /* Not supported */
        case WINED3DFMT_G16R16:
        case WINED3DFMT_A16B16G16R16:
            TRACE_(d3d_caps)("[FAILED]\n"); /* Enable when implemented */
            return WINED3DERR_NOTAVAILABLE;

        default:
            break;
    }

    TRACE_(d3d_caps)("[FAILED]\n");
    return WINED3DERR_NOTAVAILABLE;
}

static HRESULT  WINAPI IWineD3DImpl_CheckDeviceFormatConversion(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE DeviceType,
                                                          WINED3DFORMAT SourceFormat, WINED3DFORMAT TargetFormat) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;

    FIXME_(d3d_caps)("(%p)-> (STUB) (Adptr:%d, DevType:(%u,%s), SrcFmt:(%u,%s), TgtFmt:(%u,%s))\n",
          This,
          Adapter,
          DeviceType, debug_d3ddevicetype(DeviceType),
          SourceFormat, debug_d3dformat(SourceFormat),
          TargetFormat, debug_d3dformat(TargetFormat));
    return WINED3D_OK;
}

/* Note: d3d8 passes in a pointer to a D3DCAPS8 structure, which is a true
      subset of a D3DCAPS9 structure. However, it has to come via a void *
      as the d3d8 interface cannot import the d3d9 header                  */
static HRESULT WINAPI IWineD3DImpl_GetDeviceCaps(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE DeviceType, WINED3DCAPS* pCaps) {

    IWineD3DImpl    *This = (IWineD3DImpl *)iface;

    TRACE_(d3d_caps)("(%p)->(Adptr:%d, DevType: %x, pCaps: %p)\n", This, Adapter, DeviceType, pCaps);

    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    /* FIXME: both the gl_info and the shader_mode should be made per adapter */

    /* If we don't know the device settings, go query them now */
    if (This->isGLInfoValid == FALSE) {
        /* use the desktop window to fill gl caps */
        BOOL rc = IWineD3DImpl_FillGLCaps(iface, IWineD3DImpl_GetAdapterDisplay(iface, Adapter));

        /* We are running off a real context, save the values */
        if (rc) This->isGLInfoValid = TRUE;
    }
    select_shader_mode(&This->gl_info, DeviceType,
        &wined3d_settings.ps_selected_mode, &wined3d_settings.vs_selected_mode);
    select_shader_max_constants(&This->gl_info);

    /* ------------------------------------------------
       The following fields apply to both d3d8 and d3d9
       ------------------------------------------------ */
    *pCaps->DeviceType              = (DeviceType == WINED3DDEVTYPE_HAL) ? WINED3DDEVTYPE_HAL : WINED3DDEVTYPE_REF;  /* Not quite true, but use h/w supported by opengl I suppose */
    *pCaps->AdapterOrdinal          = Adapter;

    *pCaps->Caps                    = 0;
    *pCaps->Caps2                   = D3DCAPS2_CANRENDERWINDOWED;
    *pCaps->Caps3                   = WINED3DDEVCAPS_HWTRANSFORMANDLIGHT;
    *pCaps->PresentationIntervals   = D3DPRESENT_INTERVAL_IMMEDIATE;

    *pCaps->CursorCaps              = 0;


    *pCaps->DevCaps                 = WINED3DDEVCAPS_FLOATTLVERTEX       |
                                      WINED3DDEVCAPS_EXECUTESYSTEMMEMORY |
                                      WINED3DDEVCAPS_TLVERTEXSYSTEMMEMORY|
                                      WINED3DDEVCAPS_TLVERTEXVIDEOMEMORY |
                                      WINED3DDEVCAPS_DRAWPRIMTLVERTEX    |
                                      WINED3DDEVCAPS_HWTRANSFORMANDLIGHT |
                                      WINED3DDEVCAPS_EXECUTEVIDEOMEMORY  |
                                      WINED3DDEVCAPS_PUREDEVICE          |
                                      WINED3DDEVCAPS_HWRASTERIZATION     |
                                      WINED3DDEVCAPS_TEXTUREVIDEOMEMORY  |
                                      WINED3DDEVCAPS_TEXTURESYSTEMMEMORY |
                                      WINED3DDEVCAPS_CANRENDERAFTERFLIP  |
                                      WINED3DDEVCAPS_DRAWPRIMITIVES2     |
                                      WINED3DDEVCAPS_DRAWPRIMITIVES2EX;

    *pCaps->PrimitiveMiscCaps       = D3DPMISCCAPS_CULLNONE              |
                                      D3DPMISCCAPS_CULLCCW               |
                                      D3DPMISCCAPS_CULLCW                |
                                      D3DPMISCCAPS_COLORWRITEENABLE      |
                                      D3DPMISCCAPS_CLIPTLVERTS           |
                                      D3DPMISCCAPS_CLIPPLANESCALEDPOINTS |
                                      D3DPMISCCAPS_MASKZ                 |
                                      D3DPMISCCAPS_BLENDOP;
                                    /* TODO:
                                        D3DPMISCCAPS_NULLREFERENCE
                                        D3DPMISCCAPS_INDEPENDENTWRITEMASKS
                                        D3DPMISCCAPS_FOGANDSPECULARALPHA
                                        D3DPMISCCAPS_SEPARATEALPHABLEND
                                        D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS
                                        D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING
                                        D3DPMISCCAPS_FOGVERTEXCLAMPED */

/* The caps below can be supported but aren't handled yet in utils.c 'd3dta_to_combiner_input', disable them until support is fixed */
#if 0
    if (GL_SUPPORT(NV_REGISTER_COMBINERS))
        *pCaps->PrimitiveMiscCaps |=  D3DPMISCCAPS_TSSARGTEMP;
    if (GL_SUPPORT(NV_REGISTER_COMBINERS2))
        *pCaps->PrimitiveMiscCaps |=  D3DPMISCCAPS_PERSTAGECONSTANT;
#endif

    *pCaps->RasterCaps              = WINED3DPRASTERCAPS_DITHER    |
                                      WINED3DPRASTERCAPS_PAT       |
                                      WINED3DPRASTERCAPS_WFOG      |
                                      WINED3DPRASTERCAPS_ZFOG      |
                                      WINED3DPRASTERCAPS_FOGVERTEX |
                                      WINED3DPRASTERCAPS_FOGTABLE  |
                                      WINED3DPRASTERCAPS_FOGRANGE  |
                                      WINED3DPRASTERCAPS_STIPPLE   |
                                      WINED3DPRASTERCAPS_SUBPIXEL  |
                                      WINED3DPRASTERCAPS_ZTEST     |
                                      WINED3DPRASTERCAPS_SCISSORTEST   |
                                      WINED3DPRASTERCAPS_SLOPESCALEDEPTHBIAS |
                                      WINED3DPRASTERCAPS_DEPTHBIAS;

    if (GL_SUPPORT(EXT_TEXTURE_FILTER_ANISOTROPIC)) {
      *pCaps->RasterCaps |= WINED3DPRASTERCAPS_ANISOTROPY    |
                            WINED3DPRASTERCAPS_ZBIAS         |
                            WINED3DPRASTERCAPS_MIPMAPLODBIAS;
    }
                        /* FIXME Add:
			   WINED3DPRASTERCAPS_COLORPERSPECTIVE
			   WINED3DPRASTERCAPS_STRETCHBLTMULTISAMPLE
			   WINED3DPRASTERCAPS_ANTIALIASEDGES
			   WINED3DPRASTERCAPS_ZBUFFERLESSHSR
			   WINED3DPRASTERCAPS_WBUFFER */

    *pCaps->ZCmpCaps = D3DPCMPCAPS_ALWAYS       |
                       D3DPCMPCAPS_EQUAL        |
                       D3DPCMPCAPS_GREATER      |
                       D3DPCMPCAPS_GREATEREQUAL |
                       D3DPCMPCAPS_LESS         |
                       D3DPCMPCAPS_LESSEQUAL    |
                       D3DPCMPCAPS_NEVER        |
                       D3DPCMPCAPS_NOTEQUAL;

    *pCaps->SrcBlendCaps  = D3DPBLENDCAPS_BLENDFACTOR     |
                            D3DPBLENDCAPS_BOTHINVSRCALPHA |
                            D3DPBLENDCAPS_BOTHSRCALPHA    |
                            D3DPBLENDCAPS_DESTALPHA       |
                            D3DPBLENDCAPS_DESTCOLOR       |
                            D3DPBLENDCAPS_INVDESTALPHA    |
                            D3DPBLENDCAPS_INVDESTCOLOR    |
                            D3DPBLENDCAPS_INVSRCALPHA     |
                            D3DPBLENDCAPS_INVSRCCOLOR     |
                            D3DPBLENDCAPS_ONE             |
                            D3DPBLENDCAPS_SRCALPHA        |
                            D3DPBLENDCAPS_SRCALPHASAT     |
                            D3DPBLENDCAPS_SRCCOLOR        |
                            D3DPBLENDCAPS_ZERO;
                            
    *pCaps->DestBlendCaps = D3DPBLENDCAPS_BLENDFACTOR     |
                            D3DPBLENDCAPS_BOTHINVSRCALPHA |
                            D3DPBLENDCAPS_BOTHSRCALPHA    |
                            D3DPBLENDCAPS_DESTALPHA       |
                            D3DPBLENDCAPS_DESTCOLOR       |
                            D3DPBLENDCAPS_INVDESTALPHA    |
                            D3DPBLENDCAPS_INVDESTCOLOR    |
                            D3DPBLENDCAPS_INVSRCALPHA     |
                            D3DPBLENDCAPS_INVSRCCOLOR     |
                            D3DPBLENDCAPS_ONE             |
                            D3DPBLENDCAPS_SRCALPHA        |
                            D3DPBLENDCAPS_SRCALPHASAT     |
                            D3DPBLENDCAPS_SRCCOLOR        |
                            D3DPBLENDCAPS_ZERO;

    *pCaps->AlphaCmpCaps = D3DPCMPCAPS_ALWAYS       |
                           D3DPCMPCAPS_EQUAL        |
                           D3DPCMPCAPS_GREATER      |
                           D3DPCMPCAPS_GREATEREQUAL |
                           D3DPCMPCAPS_LESS         |
                           D3DPCMPCAPS_LESSEQUAL    |
                           D3DPCMPCAPS_NEVER        |
                           D3DPCMPCAPS_NOTEQUAL;

    *pCaps->ShadeCaps     = WINED3DPSHADECAPS_SPECULARGOURAUDRGB |
                            WINED3DPSHADECAPS_COLORGOURAUDRGB    |
                            WINED3DPSHADECAPS_ALPHAFLATBLEND     |
                            WINED3DPSHADECAPS_ALPHAGOURAUDBLEND  |
                            WINED3DPSHADECAPS_COLORFLATRGB       |
                            WINED3DPSHADECAPS_FOGFLAT            |
                            WINED3DPSHADECAPS_FOGGOURAUD         |
                            WINED3DPSHADECAPS_SPECULARFLATRGB;

    *pCaps->TextureCaps =  WINED3DPTEXTURECAPS_ALPHA              |
                           WINED3DPTEXTURECAPS_ALPHAPALETTE       |
                           WINED3DPTEXTURECAPS_BORDER             |
                           WINED3DPTEXTURECAPS_MIPMAP             |
                           WINED3DPTEXTURECAPS_PROJECTED          |
                           WINED3DPTEXTURECAPS_PERSPECTIVE        |
                           WINED3DPTEXTURECAPS_NONPOW2CONDITIONAL;

    if( GL_SUPPORT(EXT_TEXTURE3D)) {
        *pCaps->TextureCaps |=  WINED3DPTEXTURECAPS_VOLUMEMAP      |
                                WINED3DPTEXTURECAPS_MIPVOLUMEMAP   |
                                WINED3DPTEXTURECAPS_VOLUMEMAP_POW2;
    }

    if (GL_SUPPORT(ARB_TEXTURE_CUBE_MAP)) {
        *pCaps->TextureCaps |= WINED3DPTEXTURECAPS_CUBEMAP     |
                             WINED3DPTEXTURECAPS_MIPCUBEMAP    |
                             WINED3DPTEXTURECAPS_CUBEMAP_POW2;

    }

    *pCaps->TextureFilterCaps = WINED3DPTFILTERCAPS_MAGFLINEAR       |
                                WINED3DPTFILTERCAPS_MAGFPOINT        |
                                WINED3DPTFILTERCAPS_MINFLINEAR       |
                                WINED3DPTFILTERCAPS_MINFPOINT        |
                                WINED3DPTFILTERCAPS_MIPFLINEAR       |
                                WINED3DPTFILTERCAPS_MIPFPOINT        |
                                WINED3DPTFILTERCAPS_LINEAR           |
                                WINED3DPTFILTERCAPS_LINEARMIPLINEAR  |
                                WINED3DPTFILTERCAPS_LINEARMIPNEAREST |
                                WINED3DPTFILTERCAPS_MIPLINEAR        |
                                WINED3DPTFILTERCAPS_MIPNEAREST       |
                                WINED3DPTFILTERCAPS_NEAREST;

    if (GL_SUPPORT(EXT_TEXTURE_FILTER_ANISOTROPIC)) {
        *pCaps->TextureFilterCaps |= WINED3DPTFILTERCAPS_MAGFANISOTROPIC |
                                     WINED3DPTFILTERCAPS_MINFANISOTROPIC;
    }

    if (GL_SUPPORT(ARB_TEXTURE_CUBE_MAP)) {
        *pCaps->CubeTextureFilterCaps = WINED3DPTFILTERCAPS_MAGFLINEAR       |
                                        WINED3DPTFILTERCAPS_MAGFPOINT        |
                                        WINED3DPTFILTERCAPS_MINFLINEAR       |
                                        WINED3DPTFILTERCAPS_MINFPOINT        |
                                        WINED3DPTFILTERCAPS_MIPFLINEAR       |
                                        WINED3DPTFILTERCAPS_MIPFPOINT        |
                                        WINED3DPTFILTERCAPS_LINEAR           |
                                        WINED3DPTFILTERCAPS_LINEARMIPLINEAR  |
                                        WINED3DPTFILTERCAPS_LINEARMIPNEAREST |
                                        WINED3DPTFILTERCAPS_MIPLINEAR        |
                                        WINED3DPTFILTERCAPS_MIPNEAREST       |
                                        WINED3DPTFILTERCAPS_NEAREST;

        if (GL_SUPPORT(EXT_TEXTURE_FILTER_ANISOTROPIC)) {
            *pCaps->CubeTextureFilterCaps |= WINED3DPTFILTERCAPS_MAGFANISOTROPIC |
                                            WINED3DPTFILTERCAPS_MINFANISOTROPIC;
        }
    } else
        *pCaps->CubeTextureFilterCaps = 0;

    if (GL_SUPPORT(EXT_TEXTURE3D)) {
        *pCaps->VolumeTextureFilterCaps = WINED3DPTFILTERCAPS_MAGFLINEAR       |
                                          WINED3DPTFILTERCAPS_MAGFPOINT        |
                                          WINED3DPTFILTERCAPS_MINFLINEAR       |
                                          WINED3DPTFILTERCAPS_MINFPOINT        |
                                          WINED3DPTFILTERCAPS_MIPFLINEAR       |
                                          WINED3DPTFILTERCAPS_MIPFPOINT        |
                                          WINED3DPTFILTERCAPS_LINEAR           |
                                          WINED3DPTFILTERCAPS_LINEARMIPLINEAR  |
                                          WINED3DPTFILTERCAPS_LINEARMIPNEAREST |
                                          WINED3DPTFILTERCAPS_MIPLINEAR        |
                                          WINED3DPTFILTERCAPS_MIPNEAREST       |
                                          WINED3DPTFILTERCAPS_NEAREST;
    } else
        *pCaps->VolumeTextureFilterCaps = 0;

    *pCaps->TextureAddressCaps =  D3DPTADDRESSCAPS_INDEPENDENTUV |
                                  D3DPTADDRESSCAPS_CLAMP  |
                                  D3DPTADDRESSCAPS_WRAP;

    if (GL_SUPPORT(ARB_TEXTURE_BORDER_CLAMP)) {
        *pCaps->TextureAddressCaps |= D3DPTADDRESSCAPS_BORDER;
    }
    if (GL_SUPPORT(ARB_TEXTURE_MIRRORED_REPEAT)) {
        *pCaps->TextureAddressCaps |= D3DPTADDRESSCAPS_MIRROR;
    }
    if (GL_SUPPORT(ATI_TEXTURE_MIRROR_ONCE)) {
        *pCaps->TextureAddressCaps |= D3DPTADDRESSCAPS_MIRRORONCE;
    }

    if (GL_SUPPORT(EXT_TEXTURE3D)) {
        *pCaps->VolumeTextureAddressCaps =  D3DPTADDRESSCAPS_INDEPENDENTUV |
                                            D3DPTADDRESSCAPS_CLAMP  |
                                            D3DPTADDRESSCAPS_WRAP;
        if (GL_SUPPORT(ARB_TEXTURE_BORDER_CLAMP)) {
            *pCaps->VolumeTextureAddressCaps |= D3DPTADDRESSCAPS_BORDER;
        }
        if (GL_SUPPORT(ARB_TEXTURE_MIRRORED_REPEAT)) {
            *pCaps->VolumeTextureAddressCaps |= D3DPTADDRESSCAPS_MIRROR;
        }
        if (GL_SUPPORT(ATI_TEXTURE_MIRROR_ONCE)) {
            *pCaps->VolumeTextureAddressCaps |= D3DPTADDRESSCAPS_MIRRORONCE;
        }
    } else
        *pCaps->VolumeTextureAddressCaps = 0;

    *pCaps->LineCaps = D3DLINECAPS_TEXTURE |
                       D3DLINECAPS_ZTEST;
                      /* FIXME: Add
			 D3DLINECAPS_BLEND
			 D3DLINECAPS_ALPHACMP
			 D3DLINECAPS_FOG */

    *pCaps->MaxTextureWidth  = GL_LIMITS(texture_size);
    *pCaps->MaxTextureHeight = GL_LIMITS(texture_size);

    if(GL_SUPPORT(EXT_TEXTURE3D))
        *pCaps->MaxVolumeExtent = GL_LIMITS(texture3d_size);
    else
        *pCaps->MaxVolumeExtent = 0;

    *pCaps->MaxTextureRepeat = 32768;
    *pCaps->MaxTextureAspectRatio = GL_LIMITS(texture_size);
    *pCaps->MaxVertexW = 1.0;

    *pCaps->GuardBandLeft = 0;
    *pCaps->GuardBandTop = 0;
    *pCaps->GuardBandRight = 0;
    *pCaps->GuardBandBottom = 0;

    *pCaps->ExtentsAdjust = 0;

    *pCaps->StencilCaps =  D3DSTENCILCAPS_DECRSAT |
                           D3DSTENCILCAPS_INCRSAT |
                           D3DSTENCILCAPS_INVERT  |
                           D3DSTENCILCAPS_KEEP    |
                           D3DSTENCILCAPS_REPLACE |
                           D3DSTENCILCAPS_ZERO;
    if (GL_SUPPORT(EXT_STENCIL_WRAP)) {
      *pCaps->StencilCaps |= D3DSTENCILCAPS_DECR  |
                             D3DSTENCILCAPS_INCR;
    }
    if ( This->dxVersion > 8 &&
        ( GL_SUPPORT(EXT_STENCIL_TWO_SIDE) ||
            GL_SUPPORT(ATI_SEPARATE_STENCIL) ) ) {
        *pCaps->StencilCaps |= D3DSTENCILCAPS_TWOSIDED;
    }

    *pCaps->FVFCaps = D3DFVFCAPS_PSIZE | 0x0008; /* 8 texture coords */

    *pCaps->TextureOpCaps =  D3DTEXOPCAPS_ADD         |
                             D3DTEXOPCAPS_ADDSIGNED   |
                             D3DTEXOPCAPS_ADDSIGNED2X |
                             D3DTEXOPCAPS_MODULATE    |
                             D3DTEXOPCAPS_MODULATE2X  |
                             D3DTEXOPCAPS_MODULATE4X  |
                             D3DTEXOPCAPS_SELECTARG1  |
                             D3DTEXOPCAPS_SELECTARG2  |
                             D3DTEXOPCAPS_DISABLE;

    if (GL_SUPPORT(ARB_TEXTURE_ENV_COMBINE) ||
        GL_SUPPORT(EXT_TEXTURE_ENV_COMBINE) ||
        GL_SUPPORT(NV_TEXTURE_ENV_COMBINE4)) {
        *pCaps->TextureOpCaps |= D3DTEXOPCAPS_BLENDDIFFUSEALPHA |
                                D3DTEXOPCAPS_BLENDTEXTUREALPHA  |
                                D3DTEXOPCAPS_BLENDFACTORALPHA   |
                                D3DTEXOPCAPS_BLENDCURRENTALPHA  |
                                D3DTEXOPCAPS_LERP               |
                                D3DTEXOPCAPS_SUBTRACT;
    }
    if (GL_SUPPORT(NV_TEXTURE_ENV_COMBINE4)) {
        *pCaps->TextureOpCaps |= D3DTEXOPCAPS_ADDSMOOTH             |
                                D3DTEXOPCAPS_MULTIPLYADD            |
                                D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR |
                                D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA |
                                D3DTEXOPCAPS_BLENDTEXTUREALPHAPM;
    }
    if (GL_SUPPORT(ARB_TEXTURE_ENV_DOT3))
        *pCaps->TextureOpCaps |= D3DTEXOPCAPS_DOTPRODUCT3;

    if (GL_SUPPORT(NV_REGISTER_COMBINERS)) {
        *pCaps->TextureOpCaps |= D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR |
                                 D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA;
    }
    
    
#if 0
    *pCaps->TextureOpCaps |= D3DTEXOPCAPS_BUMPENVMAP;
                            /* FIXME: Add
                            D3DTEXOPCAPS_BUMPENVMAPLUMINANCE 
                            D3DTEXOPCAPS_PREMODULATE */
#endif

    *pCaps->MaxTextureBlendStages   = GL_LIMITS(texture_stages);
    *pCaps->MaxSimultaneousTextures = GL_LIMITS(textures);
    *pCaps->MaxUserClipPlanes       = GL_LIMITS(clipplanes);
    *pCaps->MaxActiveLights         = GL_LIMITS(lights);



#if 0 /* TODO: Blends support in drawprim */
    *pCaps->MaxVertexBlendMatrices      = GL_LIMITS(blends);
#else
    *pCaps->MaxVertexBlendMatrices      = 0;
#endif
    *pCaps->MaxVertexBlendMatrixIndex   = 1;

    *pCaps->MaxAnisotropy   = GL_LIMITS(anisotropy);
    *pCaps->MaxPointSize    = GL_LIMITS(pointsize);


    *pCaps->VertexProcessingCaps = WINED3DVTXPCAPS_DIRECTIONALLIGHTS |
                                   WINED3DVTXPCAPS_MATERIALSOURCE7   |
                                   WINED3DVTXPCAPS_POSITIONALLIGHTS  |
                                   WINED3DVTXPCAPS_LOCALVIEWER       |
                                   WINED3DVTXPCAPS_VERTEXFOG         |
                                   WINED3DVTXPCAPS_TEXGEN;
                                  /* FIXME: Add 
                                     D3DVTXPCAPS_TWEENING, D3DVTXPCAPS_TEXGEN_SPHEREMAP */

    *pCaps->MaxPrimitiveCount   = 0xFFFFF; /* For now set 2^20-1 which is used by most >=Geforce3/Radeon8500 cards */
    *pCaps->MaxVertexIndex      = 0xFFFFF;
    *pCaps->MaxStreams          = MAX_STREAMS;
    *pCaps->MaxStreamStride     = 1024;

    /* FIXME: the shader mode should be per adapter */
    if (wined3d_settings.vs_selected_mode == SHADER_GLSL) {
        /* Nvidia Geforce6/7 or Ati R4xx/R5xx cards with GLSL support, support VS 3.0 but older Nvidia/Ati
           models with GLSL support only support 2.0. In case of nvidia we can detect VS 2.0 support using
           vs_nv_version which is based on NV_vertex_program. For Ati cards there's no easy way, so for
           now only support 2.0/3.0 detection on Nvidia GeforceFX cards and default to 3.0 for everything else */
        if(This->gl_info.vs_nv_version == VS_VERSION_20)
            *pCaps->VertexShaderVersion = D3DVS_VERSION(2,0);
        else
            *pCaps->VertexShaderVersion = D3DVS_VERSION(3,0);
        TRACE_(d3d_caps)("Hardware vertex shader version 3.0 enabled (GLSL)\n");
    } else if (wined3d_settings.vs_selected_mode == SHADER_ARB) {
        *pCaps->VertexShaderVersion = D3DVS_VERSION(1,1);
        TRACE_(d3d_caps)("Hardware vertex shader version 1.1 enabled (ARB_PROGRAM)\n");
    } else if (wined3d_settings.vs_selected_mode == SHADER_SW) {
        *pCaps->VertexShaderVersion = D3DVS_VERSION(3,0);
        TRACE_(d3d_caps)("Software vertex shader version 3.0 enabled\n");
    } else {
        *pCaps->VertexShaderVersion  = 0;
        TRACE_(d3d_caps)("Vertex shader functionality not available\n");
    }

    *pCaps->MaxVertexShaderConst = GL_LIMITS(vshader_constantsF);

    /* FIXME: the shader mode should be per adapter */
    if (wined3d_settings.ps_selected_mode == SHADER_GLSL) {
        /* See the comment about VS2.0/VS3.0 detection as we do the same here but then based on NV_fragment_program
           in case of GeforceFX cards. */
        if(This->gl_info.ps_nv_version == PS_VERSION_20)
            *pCaps->PixelShaderVersion = D3DPS_VERSION(2,0);
        else
            *pCaps->PixelShaderVersion = D3DPS_VERSION(3,0);
        /* FIXME: The following line is card dependent. -1.0 to 1.0 is a safe default clamp range for now */
        *pCaps->PixelShader1xMaxValue = 1.0;
        TRACE_(d3d_caps)("Hardware pixel shader version 3.0 enabled (GLSL)\n");
    } else if (wined3d_settings.ps_selected_mode == SHADER_ARB) {
        *pCaps->PixelShaderVersion    = D3DPS_VERSION(1,4);
        *pCaps->PixelShader1xMaxValue = 1.0;
        TRACE_(d3d_caps)("Hardware pixel shader version 1.4 enabled (ARB_PROGRAM)\n");
    /* FIXME: Uncomment this when there is support for software Pixel Shader 3.0 and PS_SW is defined
    } else if (wined3d_settings.ps_selected_mode = SHADER_SW) {
        *pCaps->PixelShaderVersion    = D3DPS_VERSION(3,0);
        *pCaps->PixelShader1xMaxValue = 1.0;
        TRACE_(d3d_caps)("Software pixel shader version 3.0 enabled\n"); */
    } else {
        *pCaps->PixelShaderVersion    = 0;
        *pCaps->PixelShader1xMaxValue = 0.0;
        TRACE_(d3d_caps)("Pixel shader functionality not available\n");
    }

    /* ------------------------------------------------
       The following fields apply to d3d9 only
       ------------------------------------------------ */
    if (This->dxVersion > 8) {
        FIXME("Caps support for directx9 is nonexistent at the moment!\n");
        *pCaps->DevCaps2                          = 0;
        /* TODO: D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES and VS3.0 needs atleast D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET */
        *pCaps->MaxNpatchTessellationLevel        = 0;
        *pCaps->MasterAdapterOrdinal              = 0;
        *pCaps->AdapterOrdinalInGroup             = 0;
        *pCaps->NumberOfAdaptersInGroup           = 1;

        if(*pCaps->VertexShaderVersion >= D3DVS_VERSION(2,0)) {
            /* OpenGL supports all formats below perhaps not allways without conversion but it supports them.
               Further GLSL doesn't seem to have an official unsigned type as I'm not sure how we handle it
               don't advertise iet yet. We might need to add some clamping in the shader engine to support it.       
               TODO: D3DDTCAPS_USHORT2N, D3DDTCAPS_USHORT4N, D3DDTCAPS_UDEC3, D3DDTCAPS_DEC3N */
            *pCaps->DeclTypes = D3DDTCAPS_UBYTE4    |
                                D3DDTCAPS_UBYTE4N   |
                                D3DDTCAPS_SHORT2N   |
                                D3DDTCAPS_SHORT4N   |
                                D3DDTCAPS_FLOAT16_2 |
                                D3DDTCAPS_FLOAT16_4;
                                                                                                                                
        } else
            *pCaps->DeclTypes                         = 0;

#if 0 /* We don't properly support multiple render targets yet, so disable this for now */
        if (GL_SUPPORT(ARB_DRAWBUFFERS)) {
            *pCaps->NumSimultaneousRTs = GL_LIMITS(buffers);
        } else    
#endif
            *pCaps->NumSimultaneousRTs = 1;
            
        *pCaps->StretchRectFilterCaps             = 0;
        *pCaps->VertexTextureFilterCaps           = 0;
        
        if(*pCaps->VertexShaderVersion == D3DVS_VERSION(3,0)) {
            /* Where possible set the caps based on OpenGL extensions and if they aren't set (in case of software rendering)
               use the VS 3.0 from MSDN or else if there's OpenGL spec use a hardcoded value minimum VS3.0 value. */
            *pCaps->VS20Caps.Caps                     = D3DVS20CAPS_PREDICATION;
            *pCaps->VS20Caps.DynamicFlowControlDepth  = D3DVS20_MAX_DYNAMICFLOWCONTROLDEPTH; /* VS 3.0 requires MAX_DYNAMICFLOWCONTROLDEPTH (24) */
            *pCaps->VS20Caps.NumTemps                 = max(32, This->gl_info.vs_arb_max_temps);
            *pCaps->VS20Caps.StaticFlowControlDepth   = D3DVS20_MAX_STATICFLOWCONTROLDEPTH ; /* level of nesting in loops / if-statements; VS 3.0 requires MAX (4) */

            *pCaps->MaxVShaderInstructionsExecuted    = 65535; /* VS 3.0 needs atleast 65535, some cards even use 2^32-1 */
            *pCaps->MaxVertexShader30InstructionSlots = max(512, This->gl_info.vs_arb_max_instructions);
        } else if(*pCaps->VertexShaderVersion == D3DVS_VERSION(2,0)) {
            *pCaps->VS20Caps.Caps                     = 0;
            *pCaps->VS20Caps.DynamicFlowControlDepth  = D3DVS20_MIN_DYNAMICFLOWCONTROLDEPTH;
            *pCaps->VS20Caps.NumTemps                 = max(12, This->gl_info.vs_arb_max_temps);
            *pCaps->VS20Caps.StaticFlowControlDepth   = 1;    

            *pCaps->MaxVShaderInstructionsExecuted    = 65535;
            *pCaps->MaxVertexShader30InstructionSlots = 0;
        } else { /* VS 1.x */
            *pCaps->VS20Caps.Caps                     = 0;
            *pCaps->VS20Caps.DynamicFlowControlDepth  = 0;
            *pCaps->VS20Caps.NumTemps                 = 0;
            *pCaps->VS20Caps.StaticFlowControlDepth   = 0;    

            *pCaps->MaxVShaderInstructionsExecuted    = 0;
            *pCaps->MaxVertexShader30InstructionSlots = 0;        
        }

        if(*pCaps->PixelShaderVersion == D3DPS_VERSION(3,0)) {
            /* Where possible set the caps based on OpenGL extensions and if they aren't set (in case of software rendering)
               use the PS 3.0 from MSDN or else if there's OpenGL spec use a hardcoded value minimum PS 3.0 value. */
            
            /* Caps is more or less undocumented on MSDN but it appears to be used for PS20Caps based on results from R9600/FX5900/Geforce6800 cards from Windows */
            *pCaps->PS20Caps.Caps                     = D3DPS20CAPS_ARBITRARYSWIZZLE     |
                                                        D3DPS20CAPS_GRADIENTINSTRUCTIONS |
                                                        D3DPS20CAPS_PREDICATION          |
                                                        D3DPS20CAPS_NODEPENDENTREADLIMIT |
                                                        D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT;
            *pCaps->PS20Caps.DynamicFlowControlDepth  = D3DPS20_MAX_DYNAMICFLOWCONTROLDEPTH; /* PS 3.0 requires MAX_DYNAMICFLOWCONTROLDEPTH (24) */
            *pCaps->PS20Caps.NumTemps                 = max(32, This->gl_info.ps_arb_max_temps);
            *pCaps->PS20Caps.StaticFlowControlDepth   = D3DPS20_MAX_STATICFLOWCONTROLDEPTH; /* PS 3.0 requires MAX_STATICFLOWCONTROLDEPTH (4) */
            *pCaps->PS20Caps.NumInstructionSlots      = D3DPS20_MAX_NUMINSTRUCTIONSLOTS; /* PS 3.0 requires MAX_NUMINSTRUCTIONSLOTS (512) */

            *pCaps->MaxPShaderInstructionsExecuted    = 65535;
            *pCaps->MaxPixelShader30InstructionSlots  = max(D3DMIN30SHADERINSTRUCTIONS, This->gl_info.ps_arb_max_instructions);
        } else if(*pCaps->PixelShaderVersion == D3DPS_VERSION(2,0)) {
            /* Below we assume PS2.0 specs, not extended 2.0a(GeforceFX)/2.0b(Radeon R3xx) ones */
            *pCaps->PS20Caps.Caps                     = 0;
            *pCaps->PS20Caps.DynamicFlowControlDepth  = 0; /* D3DVS20_MIN_DYNAMICFLOWCONTROLDEPTH = 0 */
            *pCaps->PS20Caps.NumTemps                 = max(12, This->gl_info.ps_arb_max_temps);
            *pCaps->PS20Caps.StaticFlowControlDepth   = D3DPS20_MIN_STATICFLOWCONTROLDEPTH; /* Minumum: 1 */
            *pCaps->PS20Caps.NumInstructionSlots      = D3DPS20_MIN_NUMINSTRUCTIONSLOTS; /* Minimum number (64 ALU + 32 Texture), a GeforceFX uses 512 */

            *pCaps->MaxPShaderInstructionsExecuted    = 512; /* Minimum value, a GeforceFX uses 1024 */
            *pCaps->MaxPixelShader30InstructionSlots  = 0;
        } else { /* PS 1.x */
            *pCaps->PS20Caps.Caps                     = 0;
            *pCaps->PS20Caps.DynamicFlowControlDepth  = 0;
            *pCaps->PS20Caps.NumTemps                 = 0;
            *pCaps->PS20Caps.StaticFlowControlDepth   = 0;
            *pCaps->PS20Caps.NumInstructionSlots      = 0;

            *pCaps->MaxPShaderInstructionsExecuted    = 0;
            *pCaps->MaxPixelShader30InstructionSlots  = 0;
        }
    }

    return WINED3D_OK;
}


/* Note due to structure differences between dx8 and dx9 D3DPRESENT_PARAMETERS,
   and fields being inserted in the middle, a new structure is used in place    */
static HRESULT  WINAPI IWineD3DImpl_CreateDevice(IWineD3D *iface, UINT Adapter, WINED3DDEVTYPE DeviceType, HWND hFocusWindow,
                                           DWORD BehaviourFlags, IWineD3DDevice** ppReturnedDeviceInterface,
                                           IUnknown *parent) {

    IWineD3DDeviceImpl *object  = NULL;
    IWineD3DImpl       *This    = (IWineD3DImpl *)iface;
    HDC hDC;
    HRESULT temp_result;

    /* Validate the adapter number */
    if (Adapter >= IWineD3D_GetAdapterCount(iface)) {
        return WINED3DERR_INVALIDCALL;
    }

    /* Create a WineD3DDevice object */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IWineD3DDeviceImpl));
    *ppReturnedDeviceInterface = (IWineD3DDevice *)object;
    TRACE("Created WineD3DDevice object @ %p\n", object);
    if (NULL == object) {
      return WINED3DERR_OUTOFVIDEOMEMORY;
    }

    /* Set up initial COM information */
    object->lpVtbl  = &IWineD3DDevice_Vtbl;
    object->ref     = 1;
    object->wineD3D = iface;
    IWineD3D_AddRef(object->wineD3D);
    object->parent  = parent;

    /* Set the state up as invalid until the device is fully created */
    object->state   = WINED3DERR_DRIVERINTERNALERROR;

    TRACE("(%p)->(Adptr:%d, DevType: %x, FocusHwnd: %p, BehFlags: %lx, RetDevInt: %p)\n", This, Adapter, DeviceType,
          hFocusWindow, BehaviourFlags, ppReturnedDeviceInterface);

    /* Save the creation parameters */
    object->createParms.AdapterOrdinal = Adapter;
    object->createParms.DeviceType     = DeviceType;
    object->createParms.hFocusWindow   = hFocusWindow;
    object->createParms.BehaviorFlags  = BehaviourFlags;

    /* Initialize other useful values */
    object->adapterNo                    = Adapter;
    object->devType                      = DeviceType;

    TRACE("(%p) : Creating stateblock\n", This);
    /* Creating the startup stateBlock - Note Special Case: 0 => Don't fill in yet! */
    if (WINED3D_OK != IWineD3DDevice_CreateStateBlock((IWineD3DDevice *)object,
                                      WINED3DSBT_INIT,
                                    (IWineD3DStateBlock **)&object->stateBlock,
                                    NULL)  || NULL == object->stateBlock) {   /* Note: No parent needed for initial internal stateblock */
        WARN("Failed to create stateblock\n");
        goto create_device_error;
    }
    TRACE("(%p) : Created stateblock (%p)\n", This, object->stateBlock);
    object->updateStateBlock = object->stateBlock;
    IWineD3DStateBlock_AddRef((IWineD3DStateBlock*)object->updateStateBlock);
    /* Setup surfaces for the backbuffer, frontbuffer and depthstencil buffer */

    /* Setup some defaults for creating the implicit swapchain */
    ENTER_GL();
    /* FIXME: both of those should be made per adapter */
    IWineD3DImpl_FillGLCaps(iface, IWineD3DImpl_GetAdapterDisplay(iface, Adapter));
    LEAVE_GL();
    select_shader_mode(&This->gl_info, DeviceType,
        &wined3d_settings.ps_selected_mode, &wined3d_settings.vs_selected_mode);
    select_shader_max_constants(&This->gl_info);

    temp_result = allocate_shader_constants(object->updateStateBlock);
    if (WINED3D_OK != temp_result)
        return temp_result;

    /* set the state of the device to valid */
    object->state = WINED3D_OK;

    /* Get the initial screen setup for ddraw */
    object->ddraw_width = GetSystemMetrics(SM_CXSCREEN);
    object->ddraw_height = GetSystemMetrics(SM_CYSCREEN);
    hDC = CreateDCA("DISPLAY", NULL, NULL, NULL);
    object->ddraw_format = pixelformat_for_depth(GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES));
    DeleteDC(hDC);

    return WINED3D_OK;
create_device_error:

    /* Set the device state to error */
    object->state = WINED3DERR_DRIVERINTERNALERROR;

    if (object->updateStateBlock != NULL) {
        IWineD3DStateBlock_Release((IWineD3DStateBlock *)object->updateStateBlock);
        object->updateStateBlock = NULL;
    }
    if (object->stateBlock != NULL) {
        IWineD3DStateBlock_Release((IWineD3DStateBlock *)object->stateBlock);
        object->stateBlock = NULL;
    }
    if (object->renderTarget != NULL) {
        IWineD3DSurface_Release(object->renderTarget);
        object->renderTarget = NULL;
    }
    if (object->stencilBufferTarget != NULL) {
        IWineD3DSurface_Release(object->stencilBufferTarget);
        object->stencilBufferTarget = NULL;
    }
    if (object->stencilBufferTarget != NULL) {
        IWineD3DSurface_Release(object->stencilBufferTarget);
        object->stencilBufferTarget = NULL;
    }
    HeapFree(GetProcessHeap(), 0, object);
    *ppReturnedDeviceInterface = NULL;
    return WINED3DERR_INVALIDCALL;

}

static HRESULT WINAPI IWineD3DImpl_GetParent(IWineD3D *iface, IUnknown **pParent) {
    IWineD3DImpl *This = (IWineD3DImpl *)iface;
    IUnknown_AddRef(This->parent);
    *pParent = This->parent;
    return WINED3D_OK;
}

/**********************************************************
 * IWineD3D VTbl follows
 **********************************************************/

const IWineD3DVtbl IWineD3D_Vtbl =
{
    /* IUnknown */
    IWineD3DImpl_QueryInterface,
    IWineD3DImpl_AddRef,
    IWineD3DImpl_Release,
    /* IWineD3D */
    IWineD3DImpl_GetParent,
    IWineD3DImpl_GetAdapterCount,
    IWineD3DImpl_RegisterSoftwareDevice,
    IWineD3DImpl_GetAdapterMonitor,
    IWineD3DImpl_GetAdapterModeCount,
    IWineD3DImpl_EnumAdapterModes,
    IWineD3DImpl_GetAdapterDisplayMode,
    IWineD3DImpl_GetAdapterIdentifier,
    IWineD3DImpl_CheckDeviceMultiSampleType,
    IWineD3DImpl_CheckDepthStencilMatch,
    IWineD3DImpl_CheckDeviceType,
    IWineD3DImpl_CheckDeviceFormat,
    IWineD3DImpl_CheckDeviceFormatConversion,
    IWineD3DImpl_GetDeviceCaps,
    IWineD3DImpl_CreateDevice
};
