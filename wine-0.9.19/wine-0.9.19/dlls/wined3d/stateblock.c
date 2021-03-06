/*
 * state block implementation
 *
 * Copyright 2002 Raphael Junqueira
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

#include "config.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);
#define GLINFO_LOCATION ((IWineD3DImpl *)(((IWineD3DDeviceImpl *)This->wineD3DDevice)->wineD3D))->gl_info

/***************************************
 * Stateblock helper functions follow
 **************************************/

/** Allocates the correct amount of space for pixel and vertex shader constants, 
 * along with their set/changed flags on the given stateblock object
 */
HRESULT allocate_shader_constants(IWineD3DStateBlockImpl* object) {
    
    IWineD3DStateBlockImpl *This = object;

#define WINED3D_MEMCHECK(_object) if (NULL == _object) { FIXME("Out of memory!\n"); return E_OUTOFMEMORY; }

    /* Allocate space for floating point constants */
    object->pixelShaderConstantF = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(float) * GL_LIMITS(pshader_constantsF) * 4);
    WINED3D_MEMCHECK(object->pixelShaderConstantF);
    object->set.pixelShaderConstantsF = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BOOL) * GL_LIMITS(pshader_constantsF) );
    WINED3D_MEMCHECK(object->set.pixelShaderConstantsF);
    object->changed.pixelShaderConstantsF = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BOOL) * GL_LIMITS(pshader_constantsF));
    WINED3D_MEMCHECK(object->changed.pixelShaderConstantsF);
    object->vertexShaderConstantF = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(float) * GL_LIMITS(vshader_constantsF) * 4);
    WINED3D_MEMCHECK(object->vertexShaderConstantF);
    object->set.vertexShaderConstantsF = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BOOL) * GL_LIMITS(vshader_constantsF));
    WINED3D_MEMCHECK(object->set.vertexShaderConstantsF);
    object->changed.vertexShaderConstantsF = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BOOL) * GL_LIMITS(vshader_constantsF));
    WINED3D_MEMCHECK(object->changed.vertexShaderConstantsF);

#undef WINED3D_MEMCHECK

    return WINED3D_OK;
}

/** Copy all members of one stateblock to another */
void stateblock_savedstates_copy(
    IWineD3DStateBlock* iface,
    SAVEDSTATES* dest,
    SAVEDSTATES* source) {
    
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    unsigned bsize = sizeof(BOOL);

    /* Single values */
    dest->indices = source->indices;
    dest->material = source->material;
    dest->fvf = source->fvf;
    dest->viewport = source->viewport;
    dest->vertexDecl = source->vertexDecl;
    dest->pixelShader = source->pixelShader;
    dest->vertexShader = source->vertexShader;

    /* Fixed size arrays */
    memcpy(dest->streamSource, source->streamSource, bsize * MAX_STREAMS);
    memcpy(dest->streamFreq, source->streamFreq, bsize * MAX_STREAMS);
    memcpy(dest->textures, source->textures, bsize * MAX_SAMPLERS);
    memcpy(dest->transform, source->transform, bsize * (HIGHEST_TRANSFORMSTATE + 1));
    memcpy(dest->renderState, source->renderState, bsize * (WINEHIGHEST_RENDER_STATE + 1));
    memcpy(dest->textureState, source->textureState, bsize * MAX_TEXTURES * (WINED3D_HIGHEST_TEXTURE_STATE + 1));
    memcpy(dest->samplerState, source->samplerState, bsize * MAX_SAMPLERS * (WINED3D_HIGHEST_SAMPLER_STATE + 1));
    memcpy(dest->clipplane, source->clipplane, bsize * MAX_CLIPPLANES);
    memcpy(dest->pixelShaderConstantsB, source->pixelShaderConstantsB, bsize * MAX_CONST_B);
    memcpy(dest->pixelShaderConstantsI, source->pixelShaderConstantsI, bsize * MAX_CONST_I);
    memcpy(dest->vertexShaderConstantsB, source->vertexShaderConstantsB, bsize * MAX_CONST_B);
    memcpy(dest->vertexShaderConstantsI, source->vertexShaderConstantsI, bsize * MAX_CONST_I);

    /* Dynamically sized arrays */
    memcpy(dest->pixelShaderConstantsF, source->pixelShaderConstantsF, bsize * GL_LIMITS(pshader_constantsF));
    memcpy(dest->vertexShaderConstantsF, source->vertexShaderConstantsF, bsize * GL_LIMITS(vshader_constantsF));
}

/** Set all members of a stateblock savedstate to the given value */
void stateblock_savedstates_set(
    IWineD3DStateBlock* iface,
    SAVEDSTATES* states,
    BOOL value) {
    
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    unsigned bsize = sizeof(BOOL);

    /* Single values */
    states->indices = value;
    states->material = value;
    states->fvf = value;
    states->viewport = value;
    states->vertexDecl = value;
    states->pixelShader = value;
    states->vertexShader = value;

    /* Fixed size arrays */
    memset(states->streamSource, value, bsize * MAX_STREAMS);
    memset(states->streamFreq, value, bsize * MAX_STREAMS);
    memset(states->textures, value, bsize * MAX_SAMPLERS);
    memset(states->transform, value, bsize * (HIGHEST_TRANSFORMSTATE + 1));
    memset(states->renderState, value, bsize * (WINEHIGHEST_RENDER_STATE + 1));
    memset(states->textureState, value, bsize * MAX_TEXTURES * (WINED3D_HIGHEST_TEXTURE_STATE + 1));
    memset(states->samplerState, value, bsize * MAX_SAMPLERS * (WINED3D_HIGHEST_SAMPLER_STATE + 1));
    memset(states->clipplane, value, bsize * MAX_CLIPPLANES);
    memset(states->pixelShaderConstantsB, value, bsize * MAX_CONST_B);
    memset(states->pixelShaderConstantsI, value, bsize * MAX_CONST_I);
    memset(states->vertexShaderConstantsB, value, bsize * MAX_CONST_B);
    memset(states->vertexShaderConstantsI, value, bsize * MAX_CONST_I);

    /* Dynamically sized arrays */
    memset(states->pixelShaderConstantsF, value, bsize * GL_LIMITS(pshader_constantsF));
    memset(states->vertexShaderConstantsF, value, bsize * GL_LIMITS(vshader_constantsF));
}

void stateblock_copy(
    IWineD3DStateBlock* destination,
    IWineD3DStateBlock* source) {

    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)source;
    IWineD3DStateBlockImpl *Dest = (IWineD3DStateBlockImpl *)destination;

    /* IUnknown fields */
    Dest->lpVtbl                = This->lpVtbl;
    Dest->ref                   = This->ref;

    /* IWineD3DStateBlock information */
    Dest->parent                = This->parent;
    Dest->wineD3DDevice         = This->wineD3DDevice;
    Dest->blockType             = This->blockType;

    /* Saved states */
    stateblock_savedstates_copy(source, &Dest->set, &This->set);
    stateblock_savedstates_copy(source, &Dest->changed, &This->changed);

    /* Single items */
    Dest->fvf = This->fvf;
    Dest->vertexDecl = This->vertexDecl;
    Dest->vertexShader = This->vertexShader;
    Dest->streamIsUP = This->streamIsUP;
    Dest->pIndexData = This->pIndexData;
    Dest->baseVertexIndex = This->baseVertexIndex;
    Dest->lights = This->lights;
    Dest->clip_status = This->clip_status;
    Dest->viewport = This->viewport;
    Dest->material = This->material;
    Dest->pixelShader = This->pixelShader;
    Dest->vertex_blend = This->vertex_blend;
    Dest->tween_factor = This->tween_factor;
    Dest->shaderPrgId = This->shaderPrgId;

    /* Fixed size arrays */
    memcpy(Dest->vertexShaderConstantB, This->vertexShaderConstantB, sizeof(BOOL) * MAX_CONST_B);
    memcpy(Dest->vertexShaderConstantI, This->vertexShaderConstantI, sizeof(INT) * MAX_CONST_I * 4);
    memcpy(Dest->pixelShaderConstantB, This->pixelShaderConstantB, sizeof(BOOL) * MAX_CONST_B);
    memcpy(Dest->pixelShaderConstantI, This->pixelShaderConstantI, sizeof(INT) * MAX_CONST_I * 4);
    
    memcpy(Dest->streamStride, This->streamStride, sizeof(UINT) * MAX_STREAMS);
    memcpy(Dest->streamOffset, This->streamOffset, sizeof(UINT) * MAX_STREAMS);
    memcpy(Dest->streamSource, This->streamSource, sizeof(IWineD3DVertexBuffer*) * MAX_STREAMS);
    memcpy(Dest->streamFreq,   This->streamFreq,   sizeof(UINT) * MAX_STREAMS);
    memcpy(Dest->streamFlags,  This->streamFlags,  sizeof(UINT) * MAX_STREAMS);
    memcpy(Dest->transforms,   This->transforms,   sizeof(D3DMATRIX) * (HIGHEST_TRANSFORMSTATE + 1));
    memcpy(Dest->clipplane,    This->clipplane,    sizeof(double) * MAX_CLIPPLANES * 4);
    memcpy(Dest->renderState,  This->renderState,  sizeof(DWORD) * (WINEHIGHEST_RENDER_STATE + 1));
    memcpy(Dest->textures,     This->textures,     sizeof(IWineD3DBaseTexture*) * MAX_SAMPLERS);
    memcpy(Dest->textureDimensions, This->textureDimensions, sizeof(int) * MAX_SAMPLERS);
    memcpy(Dest->textureState, This->textureState, sizeof(DWORD) * MAX_TEXTURES * (WINED3D_HIGHEST_TEXTURE_STATE + 1));
    memcpy(Dest->samplerState, This->samplerState, sizeof(DWORD) * MAX_SAMPLERS * (WINED3D_HIGHEST_SAMPLER_STATE + 1));

    /* Dynamically sized arrays */
    memcpy(Dest->vertexShaderConstantF, This->vertexShaderConstantF, sizeof(float) * GL_LIMITS(vshader_constantsF) * 4);
    memcpy(Dest->pixelShaderConstantF,  This->pixelShaderConstantF,  sizeof(float) * GL_LIMITS(pshader_constantsF) * 4);
}

/**********************************************************
 * IWineD3DStateBlockImpl IUnknown parts follows
 **********************************************************/
static HRESULT  WINAPI IWineD3DStateBlockImpl_QueryInterface(IWineD3DStateBlock *iface,REFIID riid,LPVOID *ppobj)
{
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    TRACE("(%p)->(%s,%p)\n",This,debugstr_guid(riid),ppobj);
    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IWineD3DBase)
        || IsEqualGUID(riid, &IID_IWineD3DStateBlock)){
        IUnknown_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG  WINAPI IWineD3DStateBlockImpl_AddRef(IWineD3DStateBlock *iface) {
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef increasing from %ld\n", This, refCount - 1);
    return refCount;
}

static ULONG  WINAPI IWineD3DStateBlockImpl_Release(IWineD3DStateBlock *iface) {
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p) : Releasing from %ld\n", This, refCount + 1);

    if (!refCount) {
        /* type 0 represents the primary stateblock, so free all the resources */
        if (This->blockType == WINED3DSBT_INIT) {
            int counter;
            FIXME("Releasing primary stateblock\n");
            /* Free any streams still bound */
            for (counter = 0 ; counter < MAX_STREAMS ; counter++) {
                if (This->streamSource[counter] != NULL) {
                    IWineD3DVertexBuffer_Release(This->streamSource[counter]);
                    This->streamSource[counter] = NULL;
                }
            }

            /* free any index data */
            if (This->pIndexData) {
                IWineD3DIndexBuffer_Release(This->pIndexData);
                This->pIndexData = NULL;
            }

            if (NULL != This->pixelShader) {
                IWineD3DPixelShader_Release(This->pixelShader);
            }

            if (NULL != This->vertexShader) {
                IWineD3DVertexShader_Release(This->vertexShader);
            }

            if (NULL != This->vertexDecl) {
                IWineD3DVertexDeclaration_Release(This->vertexDecl);
            }
            
            HeapFree(GetProcessHeap(), 0, This->vertexShaderConstantF);
            HeapFree(GetProcessHeap(), 0, This->set.vertexShaderConstantsF);
            HeapFree(GetProcessHeap(), 0, This->changed.vertexShaderConstantsF);
            HeapFree(GetProcessHeap(), 0, This->pixelShaderConstantF);
            HeapFree(GetProcessHeap(), 0, This->set.pixelShaderConstantsF);
            HeapFree(GetProcessHeap(), 0, This->changed.pixelShaderConstantsF);
 
            /* NOTE: according to MSDN: The application is responsible for making sure the texture references are cleared down */
            for (counter = 0; counter < GL_LIMITS(sampler_stages); counter++) {
                if (This->textures[counter]) {
                    /* release our 'internal' hold on the texture */
                    if(0 != IWineD3DBaseTexture_Release(This->textures[counter])) {
                        TRACE("Texture still referenced by stateblock, applications has leaked Stage = %u Texture = %p\n", counter, This->textures[counter]);
                    }
                }
            }

        }
        HeapFree(GetProcessHeap(), 0, This);
    }
    return refCount;
}

/**********************************************************
 * IWineD3DStateBlockImpl parts follows
 **********************************************************/
static HRESULT  WINAPI IWineD3DStateBlockImpl_GetParent(IWineD3DStateBlock *iface, IUnknown **pParent) {
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    IUnknown_AddRef(This->parent);
    *pParent = This->parent;
    return WINED3D_OK;
}

static HRESULT  WINAPI IWineD3DStateBlockImpl_GetDevice(IWineD3DStateBlock *iface, IWineD3DDevice** ppDevice){

    IWineD3DStateBlockImpl *This   = (IWineD3DStateBlockImpl *)iface;

    *ppDevice = (IWineD3DDevice*)This->wineD3DDevice;
    IWineD3DDevice_AddRef(*ppDevice);
    return WINED3D_OK;

}

static HRESULT  WINAPI IWineD3DStateBlockImpl_Capture(IWineD3DStateBlock *iface){

    IWineD3DStateBlockImpl *This             = (IWineD3DStateBlockImpl *)iface;
    IWineD3DStateBlockImpl *targetStateBlock = This->wineD3DDevice->stateBlock;

    TRACE("(%p) : Updating state block %p ------------------v\n", targetStateBlock, This);

    /* If not recorded, then update can just recapture */
    if (/*TODO: 'magic' statetype, replace with BOOL This->blockType == D3DSBT_RECORDED  */ 0) {
        IWineD3DStateBlockImpl* tmpBlock;
        PLIGHTINFOEL *tmp = This->lights;

        IWineD3DDevice_CreateStateBlock((IWineD3DDevice *)This->wineD3DDevice, This->blockType, (IWineD3DStateBlock**) &tmpBlock, NULL/*parent*/);

        /* Note just swap the light chains over so when deleting, the old one goes */
        memcpy(This, tmpBlock, sizeof(IWineD3DStateBlockImpl));
        tmpBlock->lights = tmp;

        /* Delete the temporary one (which points to the old light chain though */
        IWineD3DStateBlock_Release((IWineD3DStateBlock *)tmpBlock);
        /*IDirect3DDevice_DeleteStateBlock(pDevice, tmpBlock);*/

    } else {
        unsigned int i, j;

        PLIGHTINFOEL *src;

        /* Recorded => Only update 'changed' values */
        if (This->vertexShader != targetStateBlock->vertexShader) {
            TRACE("Updating vertex shader from %p to %p\n", This->vertexShader, targetStateBlock->vertexShader);

            if (targetStateBlock->vertexShader) {
                IWineD3DVertexShader_AddRef(targetStateBlock->vertexShader);
            }
            if (This->vertexShader) {
                IWineD3DVertexShader_Release(This->vertexShader);
            }

            This->vertexShader = targetStateBlock->vertexShader;
        }

        /* Vertex Shader Float Constants */
        for (i = 0; i < GL_LIMITS(vshader_constantsF); ++i) {
            if (This->set.vertexShaderConstantsF[i]) {
                TRACE("Setting %p from %p %d to { %f, %f, %f, %f }\n", This, targetStateBlock, i,
                    targetStateBlock->vertexShaderConstantF[i * 4],
                    targetStateBlock->vertexShaderConstantF[i * 4 + 1],
                    targetStateBlock->vertexShaderConstantF[i * 4 + 2],
                    targetStateBlock->vertexShaderConstantF[i * 4 + 3]);

                This->vertexShaderConstantF[i * 4]      = targetStateBlock->vertexShaderConstantF[i * 4];
                This->vertexShaderConstantF[i * 4 + 1]  = targetStateBlock->vertexShaderConstantF[i * 4 + 1];
                This->vertexShaderConstantF[i * 4 + 2]  = targetStateBlock->vertexShaderConstantF[i * 4 + 2];
                This->vertexShaderConstantF[i * 4 + 3]  = targetStateBlock->vertexShaderConstantF[i * 4 + 3];
            }
        }
        
        /* Vertex Shader Integer Constants */
        for (i = 0; i < MAX_CONST_I; ++i) {
            if (This->set.vertexShaderConstantsI[i]) {
                TRACE("Setting %p from %p %d to { %d, %d, %d, %d }\n", This, targetStateBlock, i,
                    targetStateBlock->vertexShaderConstantI[i * 4],
                    targetStateBlock->vertexShaderConstantI[i * 4 + 1],
                    targetStateBlock->vertexShaderConstantI[i * 4 + 2],
                    targetStateBlock->vertexShaderConstantI[i * 4 + 3]);

                This->vertexShaderConstantI[i * 4]      = targetStateBlock->vertexShaderConstantI[i * 4];
                This->vertexShaderConstantI[i * 4 + 1]  = targetStateBlock->vertexShaderConstantI[i * 4 + 1];
                This->vertexShaderConstantI[i * 4 + 2]  = targetStateBlock->vertexShaderConstantI[i * 4 + 2];
                This->vertexShaderConstantI[i * 4 + 3]  = targetStateBlock->vertexShaderConstantI[i * 4 + 3];
            }
        }
        
        /* Vertex Shader Boolean Constants */
        for (i = 0; i < MAX_CONST_B; ++i) {
            if (This->set.vertexShaderConstantsB[i]) {
                TRACE("Setting %p from %p %d to %s\n", This, targetStateBlock, i,
                    targetStateBlock->vertexShaderConstantB[i]? "TRUE":"FALSE");

                This->vertexShaderConstantB[i] =  targetStateBlock->vertexShaderConstantB[i];
            }
        }
        
        /* Lights... For a recorded state block, we just had a chain of actions to perform,
             so we need to walk that chain and update any actions which differ */
        src = This->lights;
        while (src != NULL) {
            PLIGHTINFOEL *realLight = NULL;

            /* Locate the light in the live lights */
            realLight = targetStateBlock->lights;
            while (realLight != NULL && realLight->OriginalIndex != src->OriginalIndex) realLight = realLight->next;

            /* If 'changed' then its a SetLight command. Rather than comparing to see
                 if the OriginalParms have changed and then copy them (twice through
                 memory) just do the copy                                              */
            if (src->changed) {

                /* If the light exists, copy its parameters, otherwise copy the default parameters */
                const WINED3DLIGHT* params = realLight? &realLight->OriginalParms: &WINED3D_default_light;
                TRACE("Updating lights for light %ld\n", src->OriginalIndex);
                memcpy(&src->OriginalParms, params, sizeof(*params));
            }

            /* If 'enabledchanged' then its a LightEnable command */
            if (src->enabledChanged) {

                /* If the light exists, check if it's enabled, otherwise default is disabled state */
                TRACE("Updating lightEnabled for light %ld\n", src->OriginalIndex);
                src->lightEnabled = realLight? realLight->lightEnabled: FALSE;
            }

            src = src->next;
        }

        /* Recorded => Only update 'changed' values */
        if (This->pixelShader != targetStateBlock->pixelShader) {
            TRACE("Updating pixel shader from %p to %p\n", This->pixelShader, targetStateBlock->pixelShader);

            if (targetStateBlock->pixelShader) {
                IWineD3DPixelShader_AddRef(targetStateBlock->pixelShader);
            }
            if (This->pixelShader) {
                IWineD3DPixelShader_Release(This->pixelShader);
            }

            This->pixelShader = targetStateBlock->pixelShader;
        }

        /* Pixel Shader Float Constants */
        for (i = 0; i < GL_LIMITS(pshader_constantsF); ++i) {
            if (This->set.pixelShaderConstantsF[i]) {
                TRACE("Setting %p from %p %d to { %f, %f, %f, %f }\n", This, targetStateBlock, i,
                    targetStateBlock->pixelShaderConstantF[i * 4],
                    targetStateBlock->pixelShaderConstantF[i * 4 + 1],
                    targetStateBlock->pixelShaderConstantF[i * 4 + 2],
                    targetStateBlock->pixelShaderConstantF[i * 4 + 3]);

                This->pixelShaderConstantF[i * 4]      = targetStateBlock->pixelShaderConstantF[i * 4];
                This->pixelShaderConstantF[i * 4 + 1]  = targetStateBlock->pixelShaderConstantF[i * 4 + 1];
                This->pixelShaderConstantF[i * 4 + 2]  = targetStateBlock->pixelShaderConstantF[i * 4 + 2];
                This->pixelShaderConstantF[i * 4 + 3]  = targetStateBlock->pixelShaderConstantF[i * 4 + 3];
            }
        }
        
        /* Pixel Shader Integer Constants */
        for (i = 0; i < MAX_CONST_I; ++i) {
            if (This->set.pixelShaderConstantsI[i]) {
                TRACE("Setting %p from %p %d to { %d, %d, %d, %d }\n", This, targetStateBlock, i,
                    targetStateBlock->pixelShaderConstantI[i * 4],
                    targetStateBlock->pixelShaderConstantI[i * 4 + 1],
                    targetStateBlock->pixelShaderConstantI[i * 4 + 2],
                    targetStateBlock->pixelShaderConstantI[i * 4 + 3]);

                This->pixelShaderConstantI[i * 4]      = targetStateBlock->pixelShaderConstantI[i * 4];
                This->pixelShaderConstantI[i * 4 + 1]  = targetStateBlock->pixelShaderConstantI[i * 4 + 1];
                This->pixelShaderConstantI[i * 4 + 2]  = targetStateBlock->pixelShaderConstantI[i * 4 + 2];
                This->pixelShaderConstantI[i * 4 + 3]  = targetStateBlock->pixelShaderConstantI[i * 4 + 3];
            }
        }
        
        /* Pixel Shader Boolean Constants */
        for (i = 0; i < MAX_CONST_B; ++i) {
            if (This->set.pixelShaderConstantsB[i]) {
                TRACE("Setting %p from %p %d to %s\n", This, targetStateBlock, i,
                    targetStateBlock->pixelShaderConstantB[i]? "TRUE":"FALSE");

                This->pixelShaderConstantB[i] =  targetStateBlock->pixelShaderConstantB[i];
            }
        }

        /* Others + Render & Texture */
        for (i = 1; i <= HIGHEST_TRANSFORMSTATE; i++) {
            if (This->set.transform[i] && memcmp(&targetStateBlock->transforms[i],
                                    &This->transforms[i],
                                    sizeof(D3DMATRIX)) != 0) {
                TRACE("Updating transform %d\n", i);
                memcpy(&This->transforms[i], &targetStateBlock->transforms[i], sizeof(D3DMATRIX));
            }
        }

        if (This->set.indices && ((This->pIndexData != targetStateBlock->pIndexData)
                        || (This->baseVertexIndex != targetStateBlock->baseVertexIndex))) {
            TRACE("Updating pindexData to %p, baseVertexIndex to %d\n",
            targetStateBlock->pIndexData, targetStateBlock->baseVertexIndex);
            This->pIndexData = targetStateBlock->pIndexData;
            This->baseVertexIndex = targetStateBlock->baseVertexIndex;
        }

        if(This->set.vertexDecl && This->vertexDecl != targetStateBlock->vertexDecl){
            TRACE("Updating vertex declaration from %p to %p\n", This->vertexDecl, targetStateBlock->vertexDecl);

            if (targetStateBlock->vertexDecl) {
                IWineD3DVertexDeclaration_AddRef(targetStateBlock->vertexDecl);
            }
            if (This->vertexDecl) {
                IWineD3DVertexDeclaration_Release(This->vertexDecl);
            }

            This->vertexDecl = targetStateBlock->vertexDecl;
        }

        if(This->set.fvf && This->fvf != targetStateBlock->fvf){
            This->fvf = targetStateBlock->fvf;
        }

        if (This->set.material && memcmp(&targetStateBlock->material,
                                                    &This->material,
                                                    sizeof(D3DMATERIAL9)) != 0) {
            TRACE("Updating material\n");
            memcpy(&This->material, &targetStateBlock->material, sizeof(D3DMATERIAL9));
        }

        if (This->set.viewport && memcmp(&targetStateBlock->viewport,
                                                    &This->viewport,
                                                    sizeof(D3DVIEWPORT9)) != 0) {
            TRACE("Updating viewport\n");
            memcpy(&This->viewport, &targetStateBlock->viewport, sizeof(D3DVIEWPORT9));
        }

        for (i = 0; i < MAX_STREAMS; i++) {
            if (This->set.streamSource[i] &&
                            ((This->streamStride[i] != targetStateBlock->streamStride[i]) ||
                            (This->streamSource[i] != targetStateBlock->streamSource[i]))) {
                TRACE("Updating stream source %d to %p, stride to %d\n", i, targetStateBlock->streamSource[i],
                                                                            targetStateBlock->streamStride[i]);
                This->streamStride[i] = targetStateBlock->streamStride[i];
                This->streamSource[i] = targetStateBlock->streamSource[i];
            }

            if (This->set.streamFreq[i] &&
            (This->streamFreq[i] != targetStateBlock->streamFreq[i]
            || This->streamFlags[i] != targetStateBlock->streamFlags[i])){
                    TRACE("Updating stream frequency %d to %d flags to %d\n", i ,  targetStateBlock->streamFreq[i] ,
                                                                                   targetStateBlock->streamFlags[i]);
                    This->streamFreq[i]  =  targetStateBlock->streamFreq[i];
                    This->streamFlags[i] =  targetStateBlock->streamFlags[i];
            }
        }

        for (i = 0; i < GL_LIMITS(clipplanes); i++) {
            if (This->set.clipplane[i] && memcmp(&targetStateBlock->clipplane[i],
                                                        &This->clipplane[i],
                                                        sizeof(This->clipplane)) != 0) {

                TRACE("Updating clipplane %d\n", i);
                memcpy(&This->clipplane[i], &targetStateBlock->clipplane[i],
                                        sizeof(This->clipplane));
            }
        }

        /* Render */
        for (i = 1; i <= WINEHIGHEST_RENDER_STATE; i++) {

            if (This->set.renderState[i] && (This->renderState[i] != targetStateBlock->renderState[i])) {
                TRACE("Updating renderState %d to %ld\n", i, targetStateBlock->renderState[i]);
                This->renderState[i] = targetStateBlock->renderState[i];
            }
        }

        /* Texture states */
        for (j = 0; j < GL_LIMITS(texture_stages); j++) {
            /* TODO: move over to using memcpy */
            for (i = 1; i <= WINED3D_HIGHEST_TEXTURE_STATE ; i++) {
                if (This->set.textureState[j][i]) {
                    TRACE("Updating texturestagestate %d,%d to %ld (was %ld)\n", j,i, targetStateBlock->textureState[j][i],
                    This->textureState[j][i]);
                    This->textureState[j][i]         =  targetStateBlock->textureState[j][i];
                }
            }
        }

        /* Samplers */
        /* TODO: move over to using memcpy */
        for (j = 0; j < GL_LIMITS(sampler_stages); j++) {
            if (This->set.textures[j]) {
                TRACE("Updating texture %d to %p (was %p)\n", j, targetStateBlock->textures[j],  This->textures[j]);
                This->textures[j] = targetStateBlock->textures[j];
            }
            for (i = 1; i <= WINED3D_HIGHEST_SAMPLER_STATE ; i++){ /* States are 1 based */
                if (This->set.samplerState[j][i]) {
                    TRACE("Updating sampler state %d,%d to %ld (was %ld)\n",
                    j, i, targetStateBlock->samplerState[j][i],
                    This->samplerState[j][i]);
                    This->samplerState[j][i]         = targetStateBlock->samplerState[j][i];
                }
            }
        }
    }

    TRACE("(%p) : Updated state block %p ------------------^\n", targetStateBlock, This);

    return WINED3D_OK;
}

static HRESULT  WINAPI IWineD3DStateBlockImpl_Apply(IWineD3DStateBlock *iface){
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    IWineD3DDevice*        pDevice     = (IWineD3DDevice*)This->wineD3DDevice;

/*Copy thing over to updateBlock is isRecording otherwise StateBlock,
should really perform a delta so that only the changes get updated*/


    UINT i;
    UINT j;

    TRACE("(%p) : Applying state block %p ------------------v\n", This, pDevice);

    /* FIXME: Only apply applicable states not all states */

    if (/*TODO: 'magic' statetype, replace with BOOL This->blockType == D3DSBT_RECORDED || */This->blockType == WINED3DSBT_INIT || This->blockType == WINED3DSBT_ALL || This->blockType == WINED3DSBT_VERTEXSTATE) {


        PLIGHTINFOEL *toDo = This->lights;
        while (toDo != NULL) {
            if (toDo->changed)
                  IWineD3DDevice_SetLight(pDevice, toDo->OriginalIndex, &toDo->OriginalParms);
            if (toDo->enabledChanged)
                  IWineD3DDevice_SetLightEnable(pDevice, toDo->OriginalIndex, toDo->lightEnabled);
            toDo = toDo->next;
        }

        /* Vertex Shader */
        if (This->set.vertexShader && This->changed.vertexShader) {
            IWineD3DDevice_SetVertexShader(pDevice, This->vertexShader);
        }

        /* Vertex Shader Constants */
        for (i = 0; i < GL_LIMITS(vshader_constantsF); ++i) {
            if (This->set.vertexShaderConstantsF[i] && This->changed.vertexShaderConstantsF[i])
                IWineD3DDevice_SetVertexShaderConstantF(pDevice, i, This->vertexShaderConstantF + i * 4, 1);
        }
        
        for (i = 0; i < MAX_CONST_I; i++) {
            if (This->set.vertexShaderConstantsI[i] && This->changed.vertexShaderConstantsI[i])
                IWineD3DDevice_SetVertexShaderConstantI(pDevice, i, This->vertexShaderConstantI + i * 4, 1);
        }
        
        for (i = 0; i < MAX_CONST_B; i++) {
            if (This->set.vertexShaderConstantsB[i] && This->changed.vertexShaderConstantsB[i])
                IWineD3DDevice_SetVertexShaderConstantB(pDevice, i, This->vertexShaderConstantB + i, 1);
        }
    }

    if (/*TODO: 'magic' statetype, replace with BOOL This->blockType == D3DSBT_RECORDED || */ This->blockType == D3DSBT_ALL || This->blockType == D3DSBT_PIXELSTATE) {

        /* Pixel Shader */
        if (This->set.pixelShader && This->changed.pixelShader) {
            IWineD3DDevice_SetPixelShader(pDevice, This->pixelShader);
        }

        /* Pixel Shader Constants */
        for (i = 0; i < GL_LIMITS(pshader_constantsF); ++i) {
            if (This->set.pixelShaderConstantsF[i] && This->changed.pixelShaderConstantsF[i])
                IWineD3DDevice_SetPixelShaderConstantF(pDevice, i, This->pixelShaderConstantF + i * 4, 1);
        }

        for (i = 0; i < MAX_CONST_I; ++i) {
            if (This->set.pixelShaderConstantsI[i] && This->changed.pixelShaderConstantsI[i])
                IWineD3DDevice_SetPixelShaderConstantI(pDevice, i, This->pixelShaderConstantI + i * 4, 1);
        }
        
        for (i = 0; i < MAX_CONST_B; ++i) {
            if (This->set.pixelShaderConstantsB[i] && This->changed.pixelShaderConstantsB[i])
                IWineD3DDevice_SetPixelShaderConstantB(pDevice, i, This->pixelShaderConstantB + i, 1);
        }
    }

    if (This->set.fvf && This->changed.fvf) {
        IWineD3DDevice_SetFVF(pDevice, This->fvf);
    }

    if (This->set.vertexDecl && This->changed.vertexDecl) {
        IWineD3DDevice_SetVertexDeclaration(pDevice, This->vertexDecl);
    }

    /* Others + Render & Texture */
    if (/*TODO: 'magic' statetype, replace with BOOL This->blockType == D3DSBT_RECORDED || */ This->blockType == WINED3DSBT_ALL || This->blockType == WINED3DSBT_INIT) {
        for (i = 1; i <= HIGHEST_TRANSFORMSTATE; i++) {
            if (This->set.transform[i] && This->changed.transform[i])
                IWineD3DDevice_SetTransform(pDevice, i, &This->transforms[i]);
        }

        if (This->set.indices && This->changed.indices)
            IWineD3DDevice_SetIndices(pDevice, This->pIndexData, This->baseVertexIndex);

        if (This->set.material && This->changed.material )
            IWineD3DDevice_SetMaterial(pDevice, &This->material);

        if (This->set.viewport && This->changed.viewport)
            IWineD3DDevice_SetViewport(pDevice, &This->viewport);

        /* TODO: Proper implementation using SetStreamSource offset (set to 0 for the moment)\n") */
        for (i=0; i<MAX_STREAMS; i++) {
            if (This->set.streamSource[i] && This->changed.streamSource[i])
                IWineD3DDevice_SetStreamSource(pDevice, i, This->streamSource[i], 0, This->streamStride[i]);

            if (This->set.streamFreq[i] && This->changed.streamFreq[i])
                IWineD3DDevice_SetStreamSourceFreq(pDevice, i, This->streamFreq[i] | This->streamFlags[i]);
        }

        for (i = 0; i < GL_LIMITS(clipplanes); i++) {
            if (This->set.clipplane[i] && This->changed.clipplane[i]) {
                float clip[4];

                clip[0] = This->clipplane[i][0];
                clip[1] = This->clipplane[i][1];
                clip[2] = This->clipplane[i][2];
                clip[3] = This->clipplane[i][3];
                IWineD3DDevice_SetClipPlane(pDevice, i, clip);
            }
        }

        /* Render */
        for (i = 1; i <= WINEHIGHEST_RENDER_STATE; i++) {
            if (This->set.renderState[i] && This->changed.renderState[i])
                IWineD3DDevice_SetRenderState(pDevice, i, This->renderState[i]);
        }

        /* Texture states */
        for (j = 0; j < GL_LIMITS(texture_stages); j++) { /* Set The texture first, just in case it resets the states? */
            /* TODO: move over to memcpy */
            for (i = 1; i <= WINED3D_HIGHEST_TEXTURE_STATE; i++) {
                if (This->set.textureState[j][i] && This->changed.textureState[j][i]) { /* tb_dx9_10 failes without this test */
                    ((IWineD3DDeviceImpl *)pDevice)->stateBlock->textureState[j][i]         = This->textureState[j][i];
                    ((IWineD3DDeviceImpl *)pDevice)->stateBlock->set.textureState[j][i]     = TRUE;
                    ((IWineD3DDeviceImpl *)pDevice)->stateBlock->changed.textureState[j][i] = TRUE;
                }
            }
        }

        /* Samplers */
        /* TODO: move over to memcpy */
        for (j = 0 ; j < GL_LIMITS(sampler_stages); j++){
            if (This->set.textures[j] && This->changed.textures[j]) {
                IWineD3DDevice_SetTexture(pDevice, j, This->textures[j]);
            }
            for (i = 1; i <= WINED3D_HIGHEST_SAMPLER_STATE; i++){
                if (This->set.samplerState[j][i] && This->changed.samplerState[j][i]) {
                    ((IWineD3DDeviceImpl *)pDevice)->stateBlock->samplerState[j][i]         = This->samplerState[j][i];
                    ((IWineD3DDeviceImpl *)pDevice)->stateBlock->set.samplerState[j][i]     = TRUE;
                    ((IWineD3DDeviceImpl *)pDevice)->stateBlock->changed.samplerState[j][i] = TRUE;
                }
            }

        }

    } else if (This->blockType == WINED3DSBT_PIXELSTATE) {

        for (i = 0; i < NUM_SAVEDPIXELSTATES_R; i++) {
            if (This->set.renderState[SavedPixelStates_R[i]] && This->changed.renderState[SavedPixelStates_R[i]])
                IWineD3DDevice_SetRenderState(pDevice, SavedPixelStates_R[i], This->renderState[SavedPixelStates_R[i]]);

        }

        for (j = 0; j < GL_LIMITS(texture_stages); j++) {
            for (i = 0; i < NUM_SAVEDPIXELSTATES_T; i++) {
                ((IWineD3DDeviceImpl *)pDevice)->stateBlock->textureState[j][SavedPixelStates_T[i]] = This->textureState[j][SavedPixelStates_T[i]];
            }
        }

        for (j = 0; j < GL_LIMITS(sampler_stages); j++) {
            for (i = 0; i < NUM_SAVEDPIXELSTATES_S; i++) {
                ((IWineD3DDeviceImpl *)pDevice)->stateBlock->samplerState[j][SavedPixelStates_S[i]] = This->samplerState[j][SavedPixelStates_S[i]];
            }
        }

    } else if (This->blockType == WINED3DSBT_VERTEXSTATE) {

        for (i = 0; i < NUM_SAVEDVERTEXSTATES_R; i++) {
            if ( This->set.renderState[SavedVertexStates_R[i]] && This->changed.renderState[SavedVertexStates_R[i]])
                IWineD3DDevice_SetRenderState(pDevice, SavedVertexStates_R[i], This->renderState[SavedVertexStates_R[i]]);
        }

        for (j = 0; j < GL_LIMITS(texture_stages); j++) {
            for (i = 0; i < NUM_SAVEDVERTEXSTATES_T; i++) {
                ((IWineD3DDeviceImpl *)pDevice)->stateBlock->textureState[j][SavedVertexStates_T[i]] = This->textureState[j][SavedVertexStates_T[i]];
            }
        }

        for (j = 0; j < GL_LIMITS(sampler_stages); j++) {
            for (i = 0; i < NUM_SAVEDVERTEXSTATES_S; i++) {
                ((IWineD3DDeviceImpl *)pDevice)->stateBlock->samplerState[j][SavedVertexStates_S[i]] = This->samplerState[j][SavedVertexStates_S[i]];
            }
        }


    } else {
        FIXME("Unrecognized state block type %d\n", This->blockType);
    }
    stateblock_savedstates_copy(iface, &((IWineD3DDeviceImpl*)pDevice)->stateBlock->changed, &This->changed);
    TRACE("(%p) : Applied state block %p ------------------^\n", This, pDevice);

    return WINED3D_OK;
}

static HRESULT  WINAPI IWineD3DStateBlockImpl_InitStartupStateBlock(IWineD3DStateBlock* iface) {
    IWineD3DStateBlockImpl *This = (IWineD3DStateBlockImpl *)iface;
    IWineD3DDevice         *device = (IWineD3DDevice *)This->wineD3DDevice;
    IWineD3DDeviceImpl     *ThisDevice = (IWineD3DDeviceImpl *)device;
    union {
        D3DLINEPATTERN lp;
        DWORD d;
    } lp;
    union {
        float f;
        DWORD d;
    } tmpfloat;
    unsigned int i;

    /* Note this may have a large overhead but it should only be executed
       once, in order to initialize the complete state of the device and
       all opengl equivalents                                            */
    TRACE("(%p) -----------------------> Setting up device defaults... %p\n", This, This->wineD3DDevice);
    /* TODO: make a special stateblock type for the primary stateblock (it never gets applied so it doesn't need a real type) */
    This->blockType = WINED3DSBT_INIT;

    /* Set some of the defaults for lights, transforms etc */
    memcpy(&This->transforms[WINED3DTS_PROJECTION], &identity, sizeof(identity));
    memcpy(&This->transforms[WINED3DTS_VIEW], &identity, sizeof(identity));
    for (i = 0; i < 256; ++i) {
      memcpy(&This->transforms[WINED3DTS_WORLDMATRIX(i)], &identity, sizeof(identity));
    }

    TRACE("Render states\n");
    /* Render states: */
    if (ThisDevice->depthStencilBuffer != NULL) {
       IWineD3DDevice_SetRenderState(device, WINED3DRS_ZENABLE,       D3DZB_TRUE);
    } else {
       IWineD3DDevice_SetRenderState(device, WINED3DRS_ZENABLE,       D3DZB_FALSE);
    }
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FILLMODE,         D3DFILL_SOLID);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SHADEMODE,        D3DSHADE_GOURAUD);
    lp.lp.wRepeatFactor = 0;
    lp.lp.wLinePattern  = 0;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_LINEPATTERN,      lp.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ZWRITEENABLE,     TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ALPHATESTENABLE,  FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_LASTPIXEL,        TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SRCBLEND,         D3DBLEND_ONE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_DESTBLEND,        D3DBLEND_ZERO);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CULLMODE,         D3DCULL_CCW);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ZFUNC,            D3DCMP_LESSEQUAL);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ALPHAFUNC,        D3DCMP_ALWAYS);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ALPHAREF,         0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_DITHERENABLE,     FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ALPHABLENDENABLE, FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGENABLE,        FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SPECULARENABLE,   FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ZVISIBLE,         0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGCOLOR,         0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGTABLEMODE,     D3DFOG_NONE);
    tmpfloat.f = 0.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGSTART,         tmpfloat.d);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGEND,           tmpfloat.d);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGDENSITY,       tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_EDGEANTIALIAS,    FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ZBIAS,            0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_RANGEFOGENABLE,   FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILENABLE,    FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILFAIL,      D3DSTENCILOP_KEEP);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILZFAIL,     D3DSTENCILOP_KEEP);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILPASS,      D3DSTENCILOP_KEEP);

    /* Setting stencil func also uses values for stencil ref/mask, so manually set defaults
     * so only a single call performed (and ensure defaults initialized before making that call)
     *
     * IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILREF, 0);
     * IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILMASK, 0xFFFFFFFF);
     */
    This->renderState[WINED3DRS_STENCILREF] = 0;
    This->renderState[WINED3DRS_STENCILMASK] = 0xFFFFFFFF;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILFUNC,      D3DCMP_ALWAYS);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_TEXTUREFACTOR,    0xFFFFFFFF);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP0, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP1, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP2, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP3, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP4, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP5, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP6, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP7, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CLIPPING,                 TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_LIGHTING,                 TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_AMBIENT,                  0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_FOGVERTEXMODE,            D3DFOG_NONE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_COLORVERTEX,              TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_LOCALVIEWER,              TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_NORMALIZENORMALS,         FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_DIFFUSEMATERIALSOURCE,    D3DMCS_COLOR1);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SPECULARMATERIALSOURCE,   D3DMCS_COLOR2);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_AMBIENTMATERIALSOURCE,    D3DMCS_MATERIAL);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_EMISSIVEMATERIALSOURCE,   D3DMCS_MATERIAL);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_VERTEXBLEND,              D3DVBF_DISABLE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CLIPPLANEENABLE,          0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SOFTWAREVERTEXPROCESSING, FALSE);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSIZE,                tmpfloat.d);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSIZE_MIN,            tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSPRITEENABLE,        FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSCALEENABLE,         FALSE);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSCALE_A,             tmpfloat.d);
    tmpfloat.f = 0.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSCALE_B,             tmpfloat.d);
    tmpfloat.f = 0.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSCALE_C,             tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_MULTISAMPLEANTIALIAS,     TRUE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_MULTISAMPLEMASK,          0xFFFFFFFF);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_PATCHEDGESTYLE,           D3DPATCHEDGE_DISCRETE);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_PATCHSEGMENTS,            tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_DEBUGMONITORTOKEN,        0xbaadcafe);
    tmpfloat.f = 64.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POINTSIZE_MAX,            tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_COLORWRITEENABLE,         0x0000000F);
    tmpfloat.f = 0.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_TWEENFACTOR,              tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_BLENDOP,                  D3DBLENDOP_ADD);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_POSITIONDEGREE,           WINED3DDEGREE_CUBIC);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_NORMALDEGREE,             WINED3DDEGREE_LINEAR);
    /* states new in d3d9 */
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SCISSORTESTENABLE,        FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SLOPESCALEDEPTHBIAS,      0);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_MINTESSELLATIONLEVEL,     tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_MAXTESSELLATIONLEVEL,     tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ANTIALIASEDLINEENABLE,    FALSE);
    tmpfloat.f = 0.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ADAPTIVETESS_X,           tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ADAPTIVETESS_Y,           tmpfloat.d);
    tmpfloat.f = 1.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ADAPTIVETESS_Z,           tmpfloat.d);
    tmpfloat.f = 0.0f;
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ADAPTIVETESS_W,           tmpfloat.d);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_ENABLEADAPTIVETESSELLATION, FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_TWOSIDEDSTENCILMODE,      FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CCW_STENCILFAIL,          D3DSTENCILOP_KEEP);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CCW_STENCILZFAIL,         D3DSTENCILOP_KEEP);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CCW_STENCILPASS,          D3DSTENCILOP_KEEP);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_CCW_STENCILFUNC,          D3DCMP_ALWAYS);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_COLORWRITEENABLE1,        0x0000000F);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_COLORWRITEENABLE2,        0x0000000F);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_COLORWRITEENABLE3,        0x0000000F);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_BLENDFACTOR,              0xFFFFFFFF);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SRGBWRITEENABLE,          0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_DEPTHBIAS,                0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP8,  0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP9,  0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP10, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP11, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP12, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP13, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP14, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_WRAP15, 0);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_SRCBLENDALPHA,            D3DBLEND_ONE);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_DESTBLENDALPHA,           D3DBLEND_ZERO);
    IWineD3DDevice_SetRenderState(device, WINED3DRS_BLENDOPALPHA,             D3DBLENDOP_ADD);

    /* clipping status */
    This->clip_status.ClipUnion = 0;
    This->clip_status.ClipIntersection = 0xFFFFFFFF;

    /* Texture Stage States - Put directly into state block, we will call function below */
    for (i = 0; i < GL_LIMITS(texture_stages); i++) {
        TRACE("Setting up default texture states for texture Stage %d\n", i);
        memcpy(&This->transforms[WINED3DTS_TEXTURE0 + i], &identity, sizeof(identity));
        This->textureState[i][D3DTSS_COLOROP               ] = (i==0)? D3DTOP_MODULATE :  D3DTOP_DISABLE;
        This->textureState[i][D3DTSS_COLORARG1             ] = D3DTA_TEXTURE;
        This->textureState[i][D3DTSS_COLORARG2             ] = D3DTA_CURRENT;
        This->textureState[i][D3DTSS_ALPHAOP               ] = (i==0)? D3DTOP_SELECTARG1 :  D3DTOP_DISABLE;
        This->textureState[i][D3DTSS_ALPHAARG1             ] = D3DTA_TEXTURE;
        This->textureState[i][D3DTSS_ALPHAARG2             ] = D3DTA_CURRENT;
        This->textureState[i][D3DTSS_BUMPENVMAT00          ] = (DWORD) 0.0;
        This->textureState[i][D3DTSS_BUMPENVMAT01          ] = (DWORD) 0.0;
        This->textureState[i][D3DTSS_BUMPENVMAT10          ] = (DWORD) 0.0;
        This->textureState[i][D3DTSS_BUMPENVMAT11          ] = (DWORD) 0.0;
        This->textureState[i][D3DTSS_TEXCOORDINDEX         ] = i;
        This->textureState[i][D3DTSS_BUMPENVLSCALE         ] = (DWORD) 0.0;
        This->textureState[i][D3DTSS_BUMPENVLOFFSET        ] = (DWORD) 0.0;
        This->textureState[i][D3DTSS_TEXTURETRANSFORMFLAGS ] = D3DTTFF_DISABLE;
        This->textureState[i][D3DTSS_ADDRESSW              ] = D3DTADDRESS_WRAP;
        This->textureState[i][D3DTSS_COLORARG0             ] = D3DTA_CURRENT;
        This->textureState[i][D3DTSS_ALPHAARG0             ] = D3DTA_CURRENT;
        This->textureState[i][D3DTSS_RESULTARG             ] = D3DTA_CURRENT;
    }

        /* Sampler states*/
    for (i = 0 ; i <  GL_LIMITS(sampler_stages); i++) {
        TRACE("Setting up default samplers states for sampler %d\n", i);
        This->samplerState[i][WINED3DSAMP_ADDRESSU         ] = D3DTADDRESS_WRAP;
        This->samplerState[i][WINED3DSAMP_ADDRESSV         ] = D3DTADDRESS_WRAP;
        This->samplerState[i][WINED3DSAMP_ADDRESSW         ] = D3DTADDRESS_WRAP;
        This->samplerState[i][WINED3DSAMP_BORDERCOLOR      ] = 0x00;
        This->samplerState[i][WINED3DSAMP_MAGFILTER        ] = WINED3DTEXF_POINT;
        This->samplerState[i][WINED3DSAMP_MINFILTER        ] = WINED3DTEXF_POINT;
        This->samplerState[i][WINED3DSAMP_MIPFILTER        ] = WINED3DTEXF_NONE;
        This->samplerState[i][WINED3DSAMP_MIPMAPLODBIAS    ] = 0;
        This->samplerState[i][WINED3DSAMP_MAXMIPLEVEL      ] = 0;
        This->samplerState[i][WINED3DSAMP_MAXANISOTROPY    ] = 1;
        This->samplerState[i][WINED3DSAMP_SRGBTEXTURE      ] = 0; /* TODO: Gamma correction value*/
        This->samplerState[i][WINED3DSAMP_ELEMENTINDEX     ] = 0; /* TODO: Indicates which element of a  multielement texture to use */
        This->samplerState[i][WINED3DSAMP_DMAPOFFSET       ] = 256; /* TODO: Vertex offset in the presampled displacement map */
    }

    /* Under DirectX you can have texture stage operations even if no texture is
       bound, whereas opengl will only do texture operations when a valid texture is
       bound. We emulate this by creating dummy textures and binding them to each
       texture stage, but disable all stages by default. Hence if a stage is enabled
       then the default texture will kick in until replaced by a SetTexture call     */
    if (!GL_SUPPORT(NV_REGISTER_COMBINERS)) {
        ENTER_GL();

        for (i = 0; i < GL_LIMITS(texture_stages); i++) {
            GLubyte white = 255;

            /* Note this avoids calling settexture, so pretend it has been called */
            This->set.textures[i]     = TRUE;
            This->changed.textures[i] = TRUE;
            This->textures[i]         = NULL;

            /* Make appropriate texture active */
            if (GL_SUPPORT(ARB_MULTITEXTURE)) {
                GL_EXTCALL(glActiveTextureARB(GL_TEXTURE0_ARB + i));
                checkGLcall("glActiveTextureARB");
            } else if (i > 0) {
                FIXME("Program using multiple concurrent textures which this opengl implementation doesn't support\n");
            }

            /* Generate an opengl texture name */
            glGenTextures(1, &ThisDevice->dummyTextureName[i]);
            checkGLcall("glGenTextures");
            TRACE("Dummy Texture %d given name %d\n", i, ThisDevice->dummyTextureName[i]);

            /* Generate a dummy 1d texture */
            This->textureDimensions[i] = GL_TEXTURE_1D;
            glBindTexture(GL_TEXTURE_1D, ThisDevice->dummyTextureName[i]);
            checkGLcall("glBindTexture");

            glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, &white);
            checkGLcall("glTexImage1D");
#if 1   /* TODO: move the setting texture states off to basetexture */
            /* Reapply all the texture state information to this texture */
            IWineD3DDevice_SetupTextureStates(device, i, i, REAPPLY_ALL);
#endif
        }

        LEAVE_GL();
    }

    /* Defaulting palettes - Note these are device wide but reinitialized here for convenience*/
    for (i = 0; i < MAX_PALETTES; ++i) {
      int j;
      for (j = 0; j < 256; ++j) {
        This->wineD3DDevice->palettes[i][j].peRed   = 0xFF;
        This->wineD3DDevice->palettes[i][j].peGreen = 0xFF;
        This->wineD3DDevice->palettes[i][j].peBlue  = 0xFF;
        This->wineD3DDevice->palettes[i][j].peFlags = 0xFF;
      }
    }
    This->wineD3DDevice->currentPalette = 0;

    /* Set default GLSL program ID to 0.  We won't actually create one
     * until the app sets a vertex or pixel shader */
    This->shaderPrgId = 0;

    TRACE("-----------------------> Device defaults now set up...\n");
    return WINED3D_OK;
}

/**********************************************************
 * IWineD3DStateBlock VTbl follows
 **********************************************************/

const IWineD3DStateBlockVtbl IWineD3DStateBlock_Vtbl =
{
    /* IUnknown */
    IWineD3DStateBlockImpl_QueryInterface,
    IWineD3DStateBlockImpl_AddRef,
    IWineD3DStateBlockImpl_Release,
    /* IWineD3DStateBlock */
    IWineD3DStateBlockImpl_GetParent,
    IWineD3DStateBlockImpl_GetDevice,
    IWineD3DStateBlockImpl_Capture,
    IWineD3DStateBlockImpl_Apply,
    IWineD3DStateBlockImpl_InitStartupStateBlock
};
