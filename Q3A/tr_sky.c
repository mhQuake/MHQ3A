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
// tr_sky.c
#include "tr_local.h"
#include "tr_program.h"

static const vec3_t skyCloudVertexes[] = {
	{1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, 1}, {-1, -1, -1}
};

static const glIndex_t skyCloudIndexes[36] = {1, 0, 2, 1, 2, 3, 7, 6, 4, 7, 4, 5, 5, 4, 0, 5, 0, 1, 3, 2, 6, 3, 6, 7, 0, 4, 6, 0, 6, 2, 5, 1, 3, 5, 3, 7};
static const int sky_texorder[6] = {0, 2, 1, 3, 4, 5};

typedef struct skypolyvert_s
{
	float xyz[3];
	float st[2];
} skypolyvert_t;

static const skypolyvert_t skyboxpolys[6][4] = {
	{{{1, 1, -1}, {0, 1}}, {{1, 1, 1}, {0, 0}}, {{1, -1, 1}, {1, 0}}, {{1, -1, -1}, {1, 1}}},
	{{{-1, -1, -1}, {0, 1}}, {{-1, -1, 1}, {0, 0}}, {{-1, 1, 1}, {1, 0}}, {{-1, 1, -1}, {1, 1}}},
	{{{-1, 1, -1}, {0, 1}}, {{-1, 1, 1}, {0, 0}}, {{1, 1, 1}, {1, 0}}, {{1, 1, -1}, {1, 1}}},
	{{{1, -1, -1}, {0, 1}}, {{1, -1, 1}, {0, 0}}, {{-1, -1, 1}, {1, 0}}, {{-1, -1, -1}, {1, 1}}},
	{{{1, 1, 1}, {0, 1}}, {{-1, 1, 1}, {0, 0}}, {{-1, -1, 1}, {1, 0}}, {{1, -1, 1}, {1, 1}}},
	{{{-1, 1, -1}, {0, 1}}, {{1, 1, -1}, {0, 0}}, {{1, -1, -1}, {1, 0}}, {{-1, -1, -1}, {1, 1}}}
};


static void R_DrawSkyBox (image_t *boximages[], int stateBits)
{
	if (!boximages[0] || boximages[0] == tr.defaultImage) return;

	GL_State (stateBits);
	RB_SetProgram (&r_skyboxProgram);

	float identityLight[] = { tr.identityLightFloat, tr.identityLightFloat, tr.identityLightFloat, 1 };
	float moveOrigin[] = {backEnd.viewParms.or.origin[0], backEnd.viewParms.or.origin[1], backEnd.viewParms.or.origin[2], 0};

	d3d_Device->lpVtbl->SetVertexShaderConstantF (d3d_Device, VSREG_MOVEORIGIN, moveOrigin, 1);
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_IDENTITYLIGHT, identityLight, 1);

	for (int i = 0; i < 6; i++)
	{
		if (!boximages[sky_texorder[i]]) continue;

		GL_BindTexture (0, boximages[sky_texorder[i]]);
		d3d_Device->lpVtbl->DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLEFAN, 2, skyboxpolys[i], sizeof (skypolyvert_t));
	}
}


void R_BuildCloudData (void)
{
	// position the sky texcoords matrix
	glmatrix skyMatrix;

	R_IdentityMatrix (&skyMatrix);
	R_TranslateMatrix (&skyMatrix, -backEnd.viewParms.or.origin[0], -backEnd.viewParms.or.origin[1], -backEnd.viewParms.or.origin[2]);
	d3d_Device->lpVtbl->SetVertexShaderConstantF (d3d_Device, VSREG_SKYMATRIX, skyMatrix.m16, 4);

	// set up for drawing
	tess.numVertexes = 8;
	tess.numIndexes = 36;

	for (int i = 0; i < 36; i++)
		tess.indexes[i] = skyCloudIndexes[i];

	for (int i = 0; i < 8; i++)
	{
		r_stagestaticverts[i].xyz[0] = backEnd.viewParms.or.origin[0] + skyCloudVertexes[i][0];
		r_stagestaticverts[i].xyz[1] = backEnd.viewParms.or.origin[1] + skyCloudVertexes[i][1];
		r_stagestaticverts[i].xyz[2] = backEnd.viewParms.or.origin[2] + skyCloudVertexes[i][2];

		r_stagestaticverts[i].st[0] = 4096.0f;
		r_stagestaticverts[i].st[1] = tess.shader->sky.cloudHeight;
	}

	RB_StageIteratorGeneric ();
}


/*
================
RB_StageIteratorSky

All of the visible sky triangles are in tess

Other things could be stuck in here, like birds in the sky, etc
================
*/
void RB_StageIteratorSky (void)
{
	// because sky is now a full box around the environment it should only be drawn once per-view instead of once per-surface
	if (backEnd.skyRenderedThisView) return;

	// draw the outer skybox
	R_DrawSkyBox (tess.shader->sky.outerbox, 0);

	// generate the vertexes for all the clouds, which will be drawn by the generic shader routine
	R_BuildCloudData ();

	// draw the inner skybox
	R_DrawSkyBox (tess.shader->sky.innerbox, GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

	// note that sky was drawn so we will draw a sun later
	backEnd.skyRenderedThisView = qtrue;
}

