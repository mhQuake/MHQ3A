/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "tr_program.h"

backEndData_t	backEndData;
backEndState_t	backEnd;


static float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


/*
** GL_Bind
*/
void GL_BindTexture (int tmu, image_t *image)
{
	if (!image && tr.defaultImage)
	{
		ri.Printf (PRINT_WARNING, "GL_Bind: NULL image\n");
		GL_BindTexture (tmu, tr.defaultImage);
		return;
	}
	else if (!image)
		return;

	IDirect3DTexture9 *texnum = image->texnum;

	if (r_nobind->integer && tr.dlightImage)
	{
		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if (glState.currenttextures[tmu] != texnum)
	{
		image->frameUsed = tr.frameCount;
		d3d_Device->lpVtbl->SetTexture (d3d_Device, tmu, (IDirect3DBaseTexture9 *) texnum);
		glState.currenttextures[tmu] = texnum;
	}

	if (glState.currentWrapClampMode[tmu] != image->wrapClampMode)
	{
		d3d_Device->lpVtbl->SetSamplerState (d3d_Device, tmu, D3DSAMP_ADDRESSU, image->wrapClampMode);
		d3d_Device->lpVtbl->SetSamplerState (d3d_Device, tmu, D3DSAMP_ADDRESSV, image->wrapClampMode);
		glState.currentWrapClampMode[tmu] = image->wrapClampMode;
	}
}


/*
** GL_Cull
*/
void GL_Cull (int cullType)
{
	if (glState.faceCulling == cullType)
		return;

	glState.faceCulling = cullType;

	if (cullType == CT_TWO_SIDED)
		d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_CULLMODE, D3DCULL_NONE);
	else
	{
		// thse may be the wrong way around...
		if (cullType == CT_BACK_SIDED)
		{
			if (backEnd.viewParms.isMirror)
				d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_CULLMODE, D3DCULL_CCW);
			else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_CULLMODE, D3DCULL_CW);
		}
		else
		{
			if (backEnd.viewParms.isMirror)
				d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_CULLMODE, D3DCULL_CW);
			else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_CULLMODE, D3DCULL_CCW);
		}
	}
}


/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State (unsigned long stateBits)
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if (!diff)
		return;

	// check depthFunc bits
	if (diff & GLS_DEPTHFUNC_EQUAL)
	{
		if (stateBits & GLS_DEPTHFUNC_EQUAL)
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ZFUNC, D3DCMP_EQUAL);
		else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}

	// check blend bits
	if (diff & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
	{
		D3DBLEND srcFactor, dstFactor;

		if (stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
		{
			switch (stateBits & GLS_SRCBLEND_BITS)
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = D3DBLEND_ZERO;
				break;

			case GLS_SRCBLEND_ONE:
				srcFactor = D3DBLEND_ONE;
				break;

			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = D3DBLEND_DESTCOLOR;
				break;

			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = D3DBLEND_INVDESTCOLOR;
				break;

			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = D3DBLEND_SRCALPHA;
				break;

			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = D3DBLEND_INVSRCALPHA;
				break;

			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = D3DBLEND_DESTALPHA;
				break;

			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = D3DBLEND_INVDESTALPHA;
				break;

			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = D3DBLEND_SRCALPHASAT;
				break;

			default:
				srcFactor = D3DBLEND_ONE;
				break;
			}

			switch (stateBits & GLS_DSTBLEND_BITS)
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = D3DBLEND_ZERO;
				break;

			case GLS_DSTBLEND_ONE:
				dstFactor = D3DBLEND_ONE;
				break;

			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = D3DBLEND_SRCCOLOR;
				break;

			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = D3DBLEND_INVSRCCOLOR;
				break;

			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = D3DBLEND_SRCALPHA;
				break;

			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = D3DBLEND_INVSRCALPHA;
				break;

			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = D3DBLEND_DESTALPHA;
				break;

			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = D3DBLEND_INVDESTALPHA;
				break;

			default:
				dstFactor = D3DBLEND_ZERO;
				break;
			}

			if (srcFactor != D3DBLEND_ONE || dstFactor != D3DBLEND_ZERO)
			{
				d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHABLENDENABLE, TRUE);
				d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_SRCBLEND, srcFactor);
				d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_DESTBLEND, dstFactor);
			}
			else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHABLENDENABLE, FALSE);
		}
		else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHABLENDENABLE, FALSE);
	}

	// check depthmask
	if (diff & GLS_DEPTHMASK_TRUE)
	{
		if (stateBits & GLS_DEPTHMASK_TRUE)
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ZWRITEENABLE, TRUE);
		else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ZWRITEENABLE, FALSE);
	}

	// fill/line mode
	if (diff & GLS_POLYMODE_LINE)
	{
		if (stateBits & GLS_POLYMODE_LINE)
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_FILLMODE, D3DFILL_SOLID);
	}

	// depthtest
	if (diff & GLS_DEPTHTEST_DISABLE)
	{
		if (stateBits & GLS_DEPTHTEST_DISABLE)
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ZENABLE, D3DZB_FALSE);
		else d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ZENABLE, D3DZB_TRUE);
	}

	// alpha test
	if (diff & GLS_ATEST_BITS)
	{
		switch (stateBits & GLS_ATEST_BITS)
		{
		case GLS_ATEST_GT_0:
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHATESTENABLE, TRUE);
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHAFUNC, D3DCMP_GREATER);
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHAREF, 0x0);
			break;

		case GLS_ATEST_LT_80:
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHATESTENABLE, TRUE);
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHAFUNC, D3DCMP_LESS);
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHAREF, 0x80);
			break;

		case GLS_ATEST_GE_80:
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHATESTENABLE, TRUE);
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHAREF, 0x80);
			break;

		default:
			d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_ALPHATESTENABLE, FALSE);
			break;
		}
	}

	glState.glStateBits = stateBits;
}


void RB_SetViewportAndHalfPixelCorrection (D3DVIEWPORT9 *vp)
{
	// this is the only place that SetViewport should be called so that we can also do our half-pixel correction here
	float inverseRT[4] = {-1.0f / glConfig.vidWidth, 1.0f / glConfig.vidHeight, 0, 0};
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, VSREG_INVERSERT, inverseRT, 1);

	// and now set the viewport
	d3d_Device->lpVtbl->SetViewport (d3d_Device, vp);
}


void RB_SetViewport (float depthmin, float depthmax)
{
	D3DVIEWPORT9 vp = {
		backEnd.viewParms.viewportX,
		backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportHeight,
		depthmin,
		depthmax
	};

	RB_SetViewportAndHalfPixelCorrection (&vp);
}


// Lengyel, Eric. “Oblique View Frustum Depth Projection and Clipping”.
// Journal of Game Development, Vol. 1, No. 2 (2005), Charles River Media, pp. 5–16.
float sgn (float a)
{
	if (a > 0.0f) return (1.0f);
	if (a < 0.0f) return (-1.0f);
	return (0.0f);
}


// Lengyel, Eric.“Oblique View Frustum Depth Projection and Clipping”.
// Journal of Game Development, Vol. 1, No. 2 (2005), Charles River Media, pp. 5–16.
// http://www.terathon.com/code/oblique.html
void RB_SetupObliqueNearPlane (float *matrix, float *clipPlane)
{
	// Calculate the clip-space corner point opposite the clipping plane
	// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
	// transform it into camera space by multiplying it
	// by the inverse of the projection matrix
	float q[4] = {
		(sgn (clipPlane[0]) + matrix[8]) / matrix[0],
		(sgn (clipPlane[1]) + matrix[9]) / matrix[5],
		-1.0f,
		(1.0f + matrix[10]) / matrix[14]
	};

	// Calculate the scaled plane vector
	float d = 1.0f / Vector4Dot (clipPlane, q);

	// Replace the third row of the projection matrix
	matrix[2] = clipPlane[0] * d;
	matrix[6] = clipPlane[1] * d;
	matrix[10] = clipPlane[2] * d;
	matrix[14] = clipPlane[3] * d;
}


void RB_SetGammaAndBrightness (void)
{
	// rather than doing a separate brightpass we'll setup gamma and brightness in our shaders
	float gamma[4] = {r_gamma->value, r_gamma->value, r_gamma->value, r_gamma->value};
	float brightness[4] = {r_brightness->value, r_brightness->value, r_brightness->value, r_brightness->value};

	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_GAMMA, gamma, 1);
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_BRIGHTNESS, brightness, 1);
}


/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void)
{
	RB_SetGammaAndBrightness ();

	// sync with gl if needed
	if (r_finish->integer == 1 && !glState.finishCalled)
	{
		RB_EmulateGLFinish ();
		glState.finishCalled = qtrue;
	}

	if (r_finish->integer == 0)
		glState.finishCalled = qtrue;

	// we will need to change the projection matrix before drawing 2D images again
	backEnd.projection2D = qfalse;

	// set up the viewport at default depth range
	RB_SetViewport (0, 1);

	// evaluate the rect that will be used by clear operations
	D3DRECT clearrect = {
		backEnd.viewParms.viewportX,
		backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight
	};

	// clear relevant buffers
	// clearing z and stencil because they're going to be interleaved so this will give a faster clear
	// color is always cleared too but that happens at the beginning of the frame, not of the scene
	if (backEnd.viewParms.viewportX == 0 && backEnd.viewParms.viewportWidth == glConfig.vidWidth &&
		backEnd.viewParms.viewportY == 0 && backEnd.viewParms.viewportHeight == glConfig.vidHeight)
	{
		// ???? this might give us a faster clear ????
		d3d_Device->lpVtbl->Clear (d3d_Device, 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
	}
	else d3d_Device->lpVtbl->Clear (d3d_Device, 1, &clearrect, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

	if (backEnd.refdef.rdflags & RDF_HYPERSPACE)
	{
		if (!backEnd.isHyperspace)
		{
			// do initialization shit
		}

		byte c = (backEnd.refdef.time & 255);

		if (backEnd.viewParms.viewportX == 0 && backEnd.viewParms.viewportWidth == glConfig.vidWidth &&
			backEnd.viewParms.viewportY == 0 && backEnd.viewParms.viewportHeight == glConfig.vidHeight)
		{
			// ???? this might give us a faster clear ????
			d3d_Device->lpVtbl->Clear (d3d_Device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB (255, c, c, c), 1.0f, 0);
		}
		else d3d_Device->lpVtbl->Clear (d3d_Device, 1, &clearrect, D3DCLEAR_TARGET, D3DCOLOR_ARGB (255, c, c, c), 1.0f, 0);

		backEnd.isHyperspace = qtrue;
		return;
	}
	else backEnd.isHyperspace = qfalse;

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if (backEnd.viewParms.isPortal)
	{
		float plane[] = {
			backEnd.viewParms.portalPlane.normal[0],
			backEnd.viewParms.portalPlane.normal[1],
			backEnd.viewParms.portalPlane.normal[2],
			backEnd.viewParms.portalPlane.dist
		};

		float plane2[] = {
			DotProduct (backEnd.viewParms.or.axis[0], plane),
			DotProduct (backEnd.viewParms.or.axis[1], plane),
			DotProduct (backEnd.viewParms.or.axis[2], plane),
			DotProduct (plane, backEnd.viewParms.or.origin) - plane[3]
		};

		R_Transform4x4 ((glmatrix *) s_flipMatrix, plane, plane2);
		RB_SetupObliqueNearPlane (backEnd.viewParms.projection.m16, plane);
	}

	// set the projection matrix for the viewer
	RB_UpdateProjectionMatrix (&backEnd.viewParms.projection);
}


#define	MAC_EVENT_PUMP_MSEC		5

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList (drawSurf_t *drawSurfs, int numDrawSurfs)
{
	shader_t		*shader, *oldShader;
	int				fogNum, oldFogNum;
	int				entityNum, oldEntityNum;
	int				dlighted, oldDlighted;
	qboolean		depthRange, oldDepthRange;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
	float			originalTime;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++)
	{
		if (drawSurf->sort == oldSort)
		{
			// fast path, same as previous sort
			rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
			continue;
		}

		oldSort = drawSurf->sort;
		R_DecomposeSort (drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted);

		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted || (entityNum != oldEntityNum && !shader->entityMergable))
		{
			if (oldShader != NULL)
			{
				RB_EndSurface ();
			}

			RB_BeginSurface (shader, fogNum);
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		// change the modelview matrix if needed
		if (entityNum != oldEntityNum)
		{
			depthRange = qfalse;

			if (entityNum != ENTITYNUM_WORLD)
			{
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity (backEnd.currentEntity, &backEnd.viewParms, &backEnd.or);

				// set up the dynamic lighting if needed
				if (backEnd.currentEntity->needDlights)
				{
					R_TransformDlights (backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or);
				}

				if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;

				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights (backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or);
			}

			RB_UpdateModelviewMatrix ((glmatrix *) backEnd.or.modelMatrix);

			// change depthrange if needed
			if (oldDepthRange != depthRange)
			{
				if (depthRange)
					RB_SetViewport (0, 0.3f);
				else RB_SetViewport (0, 1);

				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (oldShader != NULL)
	{
		RB_EndSurface ();
	}

	// go back to the world modelview matrix
	RB_UpdateModelviewMatrix ((glmatrix *) backEnd.viewParms.world.modelMatrix);

	if (depthRange)
		RB_SetViewport (0, 1);

	// darken down any stencil shadows
	RB_ShadowFinish ();
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void RB_SetGL2D (void)
{
	backEnd.projection2D = qtrue;

	D3DVIEWPORT9 vp = {0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0};
	glmatrix orthomatrix;
	glmatrix modelmatrix;

	RB_SetViewportAndHalfPixelCorrection (&vp);

	R_IdentityMatrix (&orthomatrix);
	R_IdentityMatrix (&modelmatrix);
	R_OrthoMatrix (&orthomatrix, 0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1);

	RB_UpdateProjectionMatrix (&orthomatrix);
	RB_UpdateModelviewMatrix (&modelmatrix);

	GL_State (GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
	GL_Cull (CT_TWO_SIDED);

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds ();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
typedef struct stretchraw_s
{
	int x, y, w, h;
	int client;
} stretchraw_t;

stretchraw_t stretchRaw;

void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	if (!QGL_CheckScene ()) return;

	RB_SetGammaAndBrightness ();
	R_SyncRenderThread ();

	int start = 0;
	int end = 0;

	if (r_speeds->integer) start = ri.Milliseconds ();

	RE_UploadCinematic (cols, rows, data, client, dirty);

	if (r_speeds->integer)
	{
		end = ri.Milliseconds ();
		ri.Printf (PRINT_ALL, "RE_UploadCinematic %i, %i: %i msec\n", cols, rows, end - start);
	}

	// just save it out
	stretchRaw.x = x;
	stretchRaw.y = y;
	stretchRaw.w = w;
	stretchRaw.h = h;
	stretchRaw.client = client;

	tr.hasStretchRaw = qtrue;
}


void RE_UploadCinematic (int cols, int rows, const byte *data, int client, qboolean dirty)
{
	// if the scratchImage isn't in the format we want, specify it as a new texture
	if (cols != tr.scratchImage[client]->uploadWidth || rows != tr.scratchImage[client]->uploadHeight)
	{
		QGL_RemoveDirect3DResource ((void **) &tr.scratchImage[client]->texnum);

		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;

		R_Upload32 (tr.scratchImage[client], (const unsigned *) data);
	}
	else if (dirty)
	{
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
		R_FillSubTexture (tr.scratchImage[client]->texnum, 0, (const unsigned *) data, 0, 0, cols, rows);
	}
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor (const void *data)
{
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *) data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *) (cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic (const void *data)
{
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *) data;

	if (!backEnd.projection2D)
	{
		RB_SetGL2D ();
	}

	shader = cmd->shader;

	if (shader != tess.shader)
	{
		if (tess.numIndexes)
		{
			RB_EndSurface ();
		}

		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface (shader, 0);
	}

	RB_CHECKOVERFLOW (4, 6);
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	*(int *) tess.vertexColors[numVerts] =
		*(int *) tess.vertexColors[numVerts + 1] =
		*(int *) tess.vertexColors[numVerts + 2] =
		*(int *) tess.vertexColors[numVerts + 3] = *(int *) backEnd.color2D;

	r_stagestaticverts[numVerts].xyz[0] = cmd->x;
	r_stagestaticverts[numVerts].xyz[1] = cmd->y;
	r_stagestaticverts[numVerts].xyz[2] = 0;

	r_stagestaticverts[numVerts].st[0] = cmd->s1;
	r_stagestaticverts[numVerts].st[1] = cmd->t1;

	r_stagestaticverts[numVerts + 1].xyz[0] = cmd->x + cmd->w;
	r_stagestaticverts[numVerts + 1].xyz[1] = cmd->y;
	r_stagestaticverts[numVerts + 1].xyz[2] = 0;

	r_stagestaticverts[numVerts + 1].st[0] = cmd->s2;
	r_stagestaticverts[numVerts + 1].st[1] = cmd->t1;

	r_stagestaticverts[numVerts + 2].xyz[0] = cmd->x + cmd->w;
	r_stagestaticverts[numVerts + 2].xyz[1] = cmd->y + cmd->h;
	r_stagestaticverts[numVerts + 2].xyz[2] = 0;

	r_stagestaticverts[numVerts + 2].st[0] = cmd->s2;
	r_stagestaticverts[numVerts + 2].st[1] = cmd->t2;

	r_stagestaticverts[numVerts + 3].xyz[0] = cmd->x;
	r_stagestaticverts[numVerts + 3].xyz[1] = cmd->y + cmd->h;
	r_stagestaticverts[numVerts + 3].xyz[2] = 0;

	r_stagestaticverts[numVerts + 3].st[0] = cmd->s1;
	r_stagestaticverts[numVerts + 3].st[1] = cmd->t2;

	return (const void *) (cmd + 1);
}


/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs (const void *data)
{
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if (tess.numIndexes)
	{
		RB_EndSurface ();
	}

	cmd = (const drawSurfsCommand_t *) data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList (cmd->drawSurfs, cmd->numDrawSurfs);

	return (const void *) (cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer (const void *data)
{
	// this exists as a stub for anything that may call it from elsewhere
	const drawBufferCommand_t *cmd = (const drawBufferCommand_t *) data;
	return (const void *) (cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages (void)
{
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if (!backEnd.projection2D)
		RB_SetGL2D ();

	d3d_Device->lpVtbl->Clear (d3d_Device, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	RB_EmulateGLFinish ();

	start = ri.Milliseconds ();

	for (i = 0; i < tr.numImages; i++)
	{
		image = tr.images[i];

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if (r_showImages->integer == 2)
		{
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		DWORD color = D3DCOLOR_ARGB (255, tr.identityLightByte, tr.identityLightByte, tr.identityLightByte);

		genericvert_t verts[] = {
			{{x, y, 0}, color, {0, 0}},
			{{x + w, y, 0}, color, {1, 0}},
			{{x + w, y + h, 0}, color, {1, 1}},
			{{x, y + h, 0}, color, {0, 1}}
		};

		GL_BindTexture (0, image);
		RB_SetProgram (&r_genericProgram);
		d3d_Device->lpVtbl->DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLEFAN, 2, verts, sizeof (genericvert_t));
	}

	RB_EmulateGLFinish ();

	end = ri.Milliseconds ();
	ri.Printf (PRINT_ALL, "%i msec to draw all images\n", end - start);
}


/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers (const void *data)
{
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if (tess.numIndexes)
	{
		RB_EndSurface ();
	}

	// texture swapping test
	if (r_showImages->integer)
	{
		RB_ShowImages ();
	}

	// if a cinematic was requested draw it now
	if (tr.hasStretchRaw)
	{
		DWORD color = D3DCOLOR_ARGB (255, tr.identityLightByte, tr.identityLightByte, tr.identityLightByte);

		genericvert_t verts[] = {
			{{stretchRaw.x, stretchRaw.y, 0}, color, {0, 0}},
			{{stretchRaw.x + stretchRaw.w, stretchRaw.y, 0}, color, {1, 0}},
			{{stretchRaw.x + stretchRaw.w, stretchRaw.y + stretchRaw.h, 0}, color, {1, 1}},
			{{stretchRaw.x, stretchRaw.y + stretchRaw.h, 0}, color, {0, 1}}
		};

		RB_SetGL2D ();
		GL_BindTexture (0, tr.scratchImage[stretchRaw.client]);
		RB_SetProgram (&r_genericProgram);

		// we definately want to sync every frame for the cinematics
		RB_EmulateGLFinish ();
		glState.finishCalled = qtrue;

		// always clear if drawing cinematics
		d3d_Device->lpVtbl->Clear (d3d_Device, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
		d3d_Device->lpVtbl->DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLEFAN, 2, verts, sizeof (genericvert_t));

		tr.hasStretchRaw = qfalse;
	}

	if (tr.screenshotRequested)
	{
		RB_EmulateGLFinish ();
		glState.finishCalled = qtrue;

		IDirect3DSurface9 *surf = NULL;
		byte buffer[4] = {0, 0, 0, 0};

		// create path
		ri.FS_WriteFile (tr.screenshotName, buffer, 1);

		if (SUCCEEDED (d3d_Device->lpVtbl->GetRenderTarget (d3d_Device, 0, &surf)))
		{
			D3DXSaveSurfaceToFile (ri.FS_GetOSPath (tr.screenshotName), D3DXIFF_JPG, surf, NULL, NULL);
			surf->lpVtbl->Release (surf);
		}

		tr.screenshotRequested = qfalse;
	}

	if (!glState.finishCalled)
		RB_EmulateGLFinish ();

	cmd = (const swapBuffersCommand_t *) data;

	GLimp_EndFrame ();

	backEnd.projection2D = qfalse;

	return (const void *) (cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called synchronously if running without
smp extensions, or asynchronously by another thread.
====================
*/
void RB_ExecuteRenderCommands (const void *data)
{
	int t1 = ri.Milliseconds ();

	while (1)
	{
		switch (*(const int *) data)
		{
		case RC_SET_COLOR:
			data = RB_SetColor (data);
			break;

		case RC_STRETCH_PIC:
			data = RB_StretchPic (data);
			break;

		case RC_DRAW_SURFS:
			data = RB_DrawSurfs (data);
			break;

		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer (data);
			break;

		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers (data);
			break;

		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd (data);
			break;

		case RC_END_OF_LIST:
		default:
			// stop rendering on this thread
			backEnd.pc.msec = ri.Milliseconds () - t1;
			return;
		}
	}
}


void RB_EmulateGLFinish (void)
{
	if (d3d_Event)
	{
		d3d_Event->lpVtbl->Issue (d3d_Event, D3DISSUE_END);
		while (d3d_Event->lpVtbl->GetData (d3d_Event, NULL, 0, D3DGETDATA_FLUSH) == S_FALSE);
	}
}


