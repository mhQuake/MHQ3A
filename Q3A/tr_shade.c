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
// tr_shade.c

#include "tr_local.h"
#include "tr_program.h"

/*
THIS ENTIRE FILE IS BACK END

This file deals with applying shaders to surface data in the tess struct.
*/


/*
===================
R_DrawStripElements

===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;


stagestaticvert_t r_stagestaticverts[SHADER_MAX_VERTEXES];
stagedynamicvert_t r_stagedynamicverts[SHADER_MAX_VERTEXES];

static int r_firststaticvert = 0;
static int r_firstdynamicvert = 0;
static int r_firstindex = 0;


static void R_TransferStaticVertexes (shaderCommands_t *input)
{
	stagestaticvert_t *verts = NULL;
	int lockmode = D3DLOCK_NOOVERWRITE;

	if (r_firststaticvert + input->numVertexes >= BUFFER_MAX_VERTEXES)
	{
		lockmode = D3DLOCK_DISCARD;
		r_firststaticvert = 0;
	}

	int offset = r_firststaticvert * sizeof (stagestaticvert_t);
	int size = input->numVertexes * sizeof (stagestaticvert_t);

	if (SUCCEEDED (r_stageStaticVertexBuffer->lpVtbl->Lock (r_stageStaticVertexBuffer, offset, size, (void **) &verts, lockmode)))
	{
		memcpy (verts, r_stagestaticverts, input->numVertexes * sizeof (stagestaticvert_t));
		r_stageStaticVertexBuffer->lpVtbl->Unlock (r_stageStaticVertexBuffer);
	}

	// setup our input streams
	d3d_Device->lpVtbl->SetStreamSource (d3d_Device, 0, r_stageStaticVertexBuffer, offset, sizeof (stagestaticvert_t));
	r_firststaticvert += input->numVertexes;
}


static void R_TransferDynamicVertexes (stagedynamicvert_t *data, int numVertexes, int vertexStride)
{
	stagedynamicvert_t *verts = NULL;
	int lockmode = D3DLOCK_NOOVERWRITE;

	if (r_firstdynamicvert + numVertexes >= BUFFER_MAX_VERTEXES)
	{
		lockmode = D3DLOCK_DISCARD;
		r_firstdynamicvert = 0;
	}

	int offset = r_firstdynamicvert * sizeof (stagedynamicvert_t);
	int size = numVertexes * sizeof (stagedynamicvert_t);

	if (SUCCEEDED (r_stageDynamicVertexBuffer->lpVtbl->Lock (r_stageDynamicVertexBuffer, offset, size, (void **) &verts, lockmode)))
	{
		memcpy (verts, data, numVertexes * sizeof (stagedynamicvert_t));
		r_stageDynamicVertexBuffer->lpVtbl->Unlock (r_stageDynamicVertexBuffer);
	}

	// setup our input streams
	d3d_Device->lpVtbl->SetStreamSource (d3d_Device, 1, r_stageDynamicVertexBuffer, offset, vertexStride);
	r_firstdynamicvert += numVertexes;
}


static void R_TransferIndexes (shaderCommands_t *input)
{
	unsigned short *ndx = NULL;
	int lockmode = D3DLOCK_NOOVERWRITE;

	if (r_firstindex + input->numIndexes >= BUFFER_MAX_INDEXES)
	{
		lockmode = D3DLOCK_DISCARD;
		r_firstindex = 0;
	}

	int offset = r_firstindex * sizeof (unsigned short);
	int size = input->numIndexes * sizeof (unsigned short);

	if (SUCCEEDED (r_stageIndexBuffer->lpVtbl->Lock (r_stageIndexBuffer, offset, size, (void **) &ndx, lockmode)))
	{
		memcpy (ndx, input->indexes, input->numIndexes * sizeof (unsigned short));
		r_stageIndexBuffer->lpVtbl->Unlock (r_stageIndexBuffer);
	}

	d3d_Device->lpVtbl->SetIndices (d3d_Device, r_stageIndexBuffer);
}


/*
==================
R_DrawElements

==================
*/
static void R_DrawElements (shaderCommands_t *input)
{
	// issue the draw call
	d3d_Device->lpVtbl->DrawIndexedPrimitive (
		d3d_Device,
		D3DPT_TRIANGLELIST,
		0,
		0,
		input->numVertexes,
		r_firstindex,
		input->numIndexes / 3
	);
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;

/*
=================
R_BindAnimatedImage

=================
*/
static void R_BindAnimatedImage (int tmu, textureBundle_t *bundle)
{
	if (bundle->isVideoMap)
	{
		ri.CIN_RunCinematic (bundle->videoMapHandle);
		ri.CIN_UploadCinematic (bundle->videoMapHandle);
		return;
	}

	if (bundle->numImageAnimations <= 1)
	{
		GL_BindTexture (tmu, bundle->image[0]);
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up exactly with waveforms of the same frequency
	int index = (myftol (tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE)) >> FUNCTABLE_SIZE2;

	// may happen with shader time offsets
	if (index < 0) index = 0;

	GL_BindTexture (tmu, bundle->image[index % bundle->numImageAnimations]);
}


/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface (shader_t *shader, int fogNum)
{
	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime)
	{
		tess.shaderTime = tess.shader->clampTime;
	}
}


/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static void ProjectDlightTexture (void)
{
	for (int l = 0; l < backEnd.refdef.num_dlights; l++)
	{
		dlight_t *dl = &backEnd.refdef.dlights[l];

		// this surface definately doesn't have any of this light
		if (!(tess.dlightBits & (1 << l))) continue;

		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light where they aren't rendered
		if (dl->additive)
			GL_State (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);
		else GL_State (GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);

		d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_DLRADIUS, float4 (dl->radius, 0, 0, 0), 1);
		d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_DLORIGIN, dl->transformed, 1);
		d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_DLCOLOUR, dl->color, 1);

		RB_SetProgram (&r_dlightProgram);
		R_DrawElements (&tess);
	}
}


/*
===============
ComputeColors
===============
*/
static void ComputeColorsMTex (shaderStage_t *pStage, byte *color)
{
	switch (pStage->rgbGen)
	{
	case CGEN_CONST:
		color[0] = pStage->constantColor[0];
		color[1] = pStage->constantColor[1];
		color[2] = pStage->constantColor[2];
		color[3] = pStage->constantColor[3];
		break;

	case CGEN_IDENTITY:
		color[0] = color[1] = color[2] = color[3] = 0xff;
		break;

	case CGEN_WAVEFORM:
		RB_CalcWaveColor (&pStage->rgbWave, color);
		break;

	case CGEN_ENTITY:
		if (backEnd.currentEntity)
		{
			color[0] = backEnd.currentEntity->e.shaderRGBA[0];
			color[1] = backEnd.currentEntity->e.shaderRGBA[1];
			color[2] = backEnd.currentEntity->e.shaderRGBA[2];
			color[3] = backEnd.currentEntity->e.shaderRGBA[3];
		}
		break;

	case CGEN_ONE_MINUS_ENTITY:
		if (backEnd.currentEntity)
		{
			color[0] = 255 - backEnd.currentEntity->e.shaderRGBA[0];
			color[1] = 255 - backEnd.currentEntity->e.shaderRGBA[1];
			color[2] = 255 - backEnd.currentEntity->e.shaderRGBA[2];
			color[3] = 255 - backEnd.currentEntity->e.shaderRGBA[3];
		}
		break;

	case CGEN_FOG:
		if (tr.world && tr.world->fogs)
		{
			fog_t *fog = tr.world->fogs + tess.fogNum;

			color[0] = fog->colorBytes[0];
			color[1] = fog->colorBytes[1];
			color[2] = fog->colorBytes[2];
			color[3] = fog->colorBytes[3];
		}
		else color[0] = color[1] = color[2] = color[3] = tr.identityLightByte;
		break;

	case CGEN_IDENTITY_LIGHTING:
	default:
		color[0] = color[1] = color[2] = color[3] = tr.identityLightByte;
		break;
	}

	switch (pStage->alphaGen)
	{
	case AGEN_IDENTITY:
		color[3] = 0xff;
		break;

	case AGEN_CONST:
		color[3] = pStage->constantColor[3];
		break;

	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha (&pStage->alphaWave, color);
		break;

	case AGEN_ENTITY:
		if (backEnd.currentEntity)
			color[3] = backEnd.currentEntity->e.shaderRGBA[3];
		break;

	case AGEN_ONE_MINUS_ENTITY:
		if (backEnd.currentEntity)
			color[3] = 255 - backEnd.currentEntity->e.shaderRGBA[3];
		break;

	case AGEN_SKIP:
	default:
		break;
	}
}


static qboolean CanSendConstColor (shaderStage_t *pStage, qboolean hasConstColor, qboolean hasConstAlpha)
{
	if (hasConstColor && hasConstAlpha)
	{
		if (tess.fogNum)
		{
			switch (pStage->adjustColorsForFog)
			{
			case ACFF_MODULATE_RGB:
			case ACFF_MODULATE_ALPHA:
			case ACFF_MODULATE_RGBA:
				return qfalse;

			default:
				break;
			}
		}

		return qtrue;
	}

	return qfalse;
}


static void ComputeColors (shaderStage_t *pStage)
{
	stagedynamicvert_t constColor;

	qboolean hasConstColor = qfalse;
	qboolean hasConstAlpha = qfalse;

	// reuse the mtex code for cases where there is a single fixed color for the stage
	// this must be done outside as it will eval both rgbgen and agen which we then write in below
	ComputeColorsMTex (pStage, constColor.rgba);

	// rgbGen
	switch (pStage->rgbGen)
	{
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor ();
		break;

	case CGEN_EXACT_VERTEX:
		for (int i = 0; i < tess.numVertexes; i++)
		{
			r_stagedynamicverts[i].rgba[0] = tess.vertexColors[i][0];
			r_stagedynamicverts[i].rgba[1] = tess.vertexColors[i][1];
			r_stagedynamicverts[i].rgba[2] = tess.vertexColors[i][2];
			r_stagedynamicverts[i].rgba[3] = tess.vertexColors[i][3];
		}
		break;

	case CGEN_VERTEX:
		for (int i = 0; i < tess.numVertexes; i++)
		{
			r_stagedynamicverts[i].rgba[0] = tess.vertexColors[i][0] * tr.identityLight;
			r_stagedynamicverts[i].rgba[1] = tess.vertexColors[i][1] * tr.identityLight;
			r_stagedynamicverts[i].rgba[2] = tess.vertexColors[i][2] * tr.identityLight;
			r_stagedynamicverts[i].rgba[3] = tess.vertexColors[i][3];
		}
		break;

	case CGEN_ONE_MINUS_VERTEX:
		for (int i = 0; i < tess.numVertexes; i++)
		{
			r_stagedynamicverts[i].rgba[0] = (255 - tess.vertexColors[i][0]) * tr.identityLight;
			r_stagedynamicverts[i].rgba[1] = (255 - tess.vertexColors[i][1]) * tr.identityLight;
			r_stagedynamicverts[i].rgba[2] = (255 - tess.vertexColors[i][2]) * tr.identityLight;
		}
		break;

	default:
		hasConstColor = qtrue;
		break;
	}

	// alphaGen
	switch (pStage->alphaGen)
	{
	case AGEN_SKIP:
		hasConstAlpha = hasConstColor;
		break;

	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha ();
		break;

	case AGEN_VERTEX:
		if (pStage->rgbGen != CGEN_VERTEX)
			for (int i = 0; i < tess.numVertexes; i++)
				r_stagedynamicverts[i].rgba[3] = tess.vertexColors[i][3];
		break;

	case AGEN_ONE_MINUS_VERTEX:
		for (int i = 0; i < tess.numVertexes; i++)
			r_stagedynamicverts[i].rgba[3] = 255 - tess.vertexColors[i][3];
		break;

	case AGEN_PORTAL:
		RB_CalcPortalAlpha ();
		break;

	default:
		hasConstAlpha = qtrue;
		break;
	}

	// see if it can use a single vertex with stride 0
	if (CanSendConstColor (pStage, hasConstColor, hasConstAlpha))
	{
		// do a transfer of one vertex only; you can't do this in OpenGL because stride 0 has a special meaning there
		R_TransferDynamicVertexes (&constColor, 1, 0);
		return;
	}

	// expand out the const colour and alpha if needed
	if (hasConstColor && hasConstAlpha)
	{
		for (int i = 0; i < tess.numVertexes; i++)
		{
			r_stagedynamicverts[i].rgba[0] = constColor.rgba[0];
			r_stagedynamicverts[i].rgba[1] = constColor.rgba[1];
			r_stagedynamicverts[i].rgba[2] = constColor.rgba[2];
			r_stagedynamicverts[i].rgba[3] = constColor.rgba[3];
		}
	}
	else if (hasConstColor)
	{
		for (int i = 0; i < tess.numVertexes; i++)
		{
			r_stagedynamicverts[i].rgba[0] = constColor.rgba[0];
			r_stagedynamicverts[i].rgba[1] = constColor.rgba[1];
			r_stagedynamicverts[i].rgba[2] = constColor.rgba[2];
		}
	}
	else if (hasConstAlpha)
	{
		for (int i = 0; i < tess.numVertexes; i++)
		{
			r_stagedynamicverts[i].rgba[3] = constColor.rgba[3];
		}
	}

	// fog adjustment for colors to fade out as fog increases
	if (tess.fogNum)
	{
		switch (pStage->adjustColorsForFog)
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog ();
			break;

		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog ();
			break;

		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog ();
			break;

		case ACFF_NONE:
			break;
		}
	}

	// copy over the stuff that changes from stage-to-stage
	R_TransferDynamicVertexes (r_stagedynamicverts, tess.numVertexes, sizeof (stagedynamicvert_t));
}


/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords (shaderStage_t *pStage)
{
	// generate the texture coordinates
	switch (pStage->bundle.tcGen)
	{
	case TCGEN_FOG:
		RB_SetupTCGenFog ();
		break;

	case TCGEN_ENVIRONMENT_MAPPED:
		d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_VIEWORIGIN, backEnd.or.viewOrigin, 1);
		break;

	case TCGEN_VECTOR:
		d3d_Device->lpVtbl->SetVertexShaderConstantF (d3d_Device, VSREG_TCGENVEC0, pStage->bundle.tcGenVectors[0], 1);
		d3d_Device->lpVtbl->SetVertexShaderConstantF (d3d_Device, VSREG_TCGENVEC1, pStage->bundle.tcGenVectors[1], 1);
		break;

	default:
		break;
	}

	float turbTime[4] = {0, 0, 0, 0};
	float turbAmp[4] = {0, 0, 0, 0};
	qboolean turb = qfalse;

	for (int tm = 0; tm < pStage->bundle.numTexMods; tm++)
	{
		texModInfo_t *tmi = &pStage->bundle.texMods[tm];
		waveForm_t *wf = &tmi->wave;

		switch (pStage->bundle.texMods[tm].type)
		{
		case TMOD_NONE:
			// break out of for loop
			// (we can't return because we need to set the turb regs)
			tm = TR_MAX_TEXMODS;
			break;

		case TMOD_SCROLL:
			RB_SetupTCModScroll (tm, tmi->scroll);
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_SetupTCModScroll (tm, backEnd.currentEntity->e.shaderTexCoord);
			break;

		case TMOD_TURBULENT:
			turbTime[tm] = wf->phase + (tess.shaderTime * wf->frequency);
			turbAmp[tm] = wf->amplitude;
			turb = qtrue;	// flag that we need to send turbulent
			break;

		case TMOD_SCALE:
			RB_SetupTCModScale (tm, tmi->scale);
			break;

		case TMOD_STRETCH:
			RB_SetupTCModStretch (tm, wf);
			break;

		case TMOD_TRANSFORM:
			RB_SetupTCModTransform (tm, tmi);
			break;

		case TMOD_ROTATE:
			RB_SetupTCModRotate (tm, tmi->rotateSpeed);
			break;

		default:
			break;
		}
	}

	if (turb)
	{
		// send turbulent factors
		d3d_Device->lpVtbl->SetVertexShaderConstantF (d3d_Device, VSREG_TMODTURBTIME, turbTime, 1);
		d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_TMODTURBAMP, turbAmp, 1);
	}
}


static void RB_IterateStagesGeneric (shaderCommands_t *input, shader_t *shader)
{
	for (int stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t *pStage = shader->stages[stage];

		if (!pStage)
			break;

		ComputeColors (pStage);
		ComputeTexCoords (pStage);

		// set state
		R_BindAnimatedImage (pStage->TMU, &pStage->bundle);
		GL_State (pStage->stateBits);

		RB_SetVertexDeclaration (r_stageDeclaration);
		RB_SetVertexShader (pStage->vertexShader);
		RB_SetPixelShader (pStage->pixelShader);

		// draw
		R_DrawElements (input);
	}
}


void RB_StageIteratorGeneric (void)
{
	RB_DeformTessGeometry ();

	// copy over the stuff that's constant for all stages
	R_TransferStaticVertexes (&tess);
	R_TransferIndexes (&tess);

	// log this call
	if (r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment (va ("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name));
	}

	// set face culling appropriately
	GL_Cull (tess.shader->cullType);

	// set polygon offset if necessary
	if (tess.shader->polygonOffset)
	{
		glmatrix biasMatrix;

		memcpy (&biasMatrix, &backEnd.viewParms.projection, sizeof (biasMatrix));
		biasMatrix.m16[14] += r_depthBiasFactor->value;

		// set the new z-biased projection matrix
		RB_UpdateProjectionMatrix (&biasMatrix);
	}

	// call shader function
	RB_IterateStagesGeneric (&tess, tess.shader);

	// now do any dynamic lighting needed
	if (tess.dlightBits && tess.shader->sort <= SS_OPAQUE && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY)))
		ProjectDlightTexture ();

	// now do fog
	if (tess.fogNum && tess.shader->fogPass && tr.fogShader->stages[0])
	{
		// hack the fog shader statebits
		if (tess.shader->fogPass == FP_EQUAL)
			tr.fogShader->stages[0]->stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL;
		else tr.fogShader->stages[0]->stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

		// hack the shader to tess in case anything downstream is going to refer to it
		shader_t *saved = tess.shader;
		tess.shader = tr.fogShader;

		// and send it through the standard iterator
		RB_IterateStagesGeneric (&tess, tr.fogShader);

		// restore the original shader in case anything upstream depends on it
		tess.shader = saved;
	}

	// reset polygon offset
	if (tess.shader->polygonOffset)
	{
		// restore the original projection matrix
		RB_UpdateProjectionMatrix (&backEnd.viewParms.projection);
	}

	// accumulate this count here after all draw calls are done because they all reference it's value
	r_firstindex += tess.numIndexes;
}


void RB_EndSurface (void)
{
	shaderCommands_t *input = &tess;

	if (input->numIndexes == 0)
		return;

	if (input->indexes[SHADER_MAX_INDEXES - 1] != 0)
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");

	if (r_stagestaticverts[SHADER_MAX_VERTEXES - 1].xyz[0] != 0)
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");

	if (tess.shader == tr.shadowShader)
	{
		RB_ShadowTessEnd ();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if (r_debugSort->integer && r_debugSort->integer < tess.shader->sort)
		return;

	// update performance counters
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	// call off to shader specific tess end function
	tess.currentStageIteratorFunc ();

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
}

