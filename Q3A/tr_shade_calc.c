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
// tr_shade_calc.c

#include "tr_local.h"
#include "tr_program.h"

#define	WAVEVALUE(table, base, amplitude, phase, freq)  ((base) + table[myftol ((((phase) + tess.shaderTime * (freq)) * FUNCTABLE_SIZE)) & FUNCTABLE_MASK] * (amplitude))

static float *TableForFunc (genFunc_t func)
{
	switch (func)
	{
	case GF_SIN:
		return tr.sinTable;
	case GF_TRIANGLE:
		return tr.triangleTable;
	case GF_SQUARE:
		return tr.squareTable;
	case GF_SAWTOOTH:
		return tr.sawToothTable;
	case GF_INVERSE_SAWTOOTH:
		return tr.inverseSawToothTable;
	case GF_NONE:
	default:
		break;
	}

	ri.Error (ERR_DROP, "TableForFunc called with invalid function '%d' in shader '%s'\n", func, tess.shader->name);
	return NULL;
}

/*
** EvalWaveForm
**
** Evaluates a given waveForm_t, referencing backEnd.refdef.time directly
*/
static float EvalWaveForm (const waveForm_t *wf)
{
	float *table = TableForFunc (wf->func);
	return WAVEVALUE (table, wf->base, wf->amplitude, wf->phase, wf->frequency);
}

static float EvalWaveFormClamped (const waveForm_t *wf)
{
	float glow = EvalWaveForm (wf);

	if (glow < 0) return 0;
	if (glow > 1) return 1;

	return glow;
}

/*
====================================================================

DEFORMATIONS

====================================================================
*/

/*
========================
RB_CalcDeformVertexes

========================
*/
void RB_CalcDeformVertexes (deformStage_t *ds)
{
	int i;
	vec3_t	offset;
	float	scale;
	float	*table;

	if (ds->deformationWave.frequency == 0)
	{
		scale = EvalWaveForm (&ds->deformationWave);

		for (i = 0; i < tess.numVertexes; i++)
		{
			VectorScale (r_stagestaticverts[i].normal, scale, offset);

			r_stagestaticverts[i].xyz[0] += offset[0];
			r_stagestaticverts[i].xyz[1] += offset[1];
			r_stagestaticverts[i].xyz[2] += offset[2];
		}
	}
	else
	{
		table = TableForFunc (ds->deformationWave.func);

		for (i = 0; i < tess.numVertexes; i++)
		{
			float off = (r_stagestaticverts[i].xyz[0] + r_stagestaticverts[i].xyz[1] + r_stagestaticverts[i].xyz[2]) * ds->deformationSpread;

			scale = WAVEVALUE (table, ds->deformationWave.base,
				ds->deformationWave.amplitude,
				ds->deformationWave.phase + off,
				ds->deformationWave.frequency);

			VectorScale (r_stagestaticverts[i].normal, scale, offset);

			r_stagestaticverts[i].xyz[0] += offset[0];
			r_stagestaticverts[i].xyz[1] += offset[1];
			r_stagestaticverts[i].xyz[2] += offset[2];
		}
	}
}

/*
=========================
RB_CalcDeformNormals

Wiggle the normals for wavy environment mapping
=========================
*/
void RB_CalcDeformNormals (deformStage_t *ds)
{
	for (int i = 0; i < tess.numVertexes; i++)
	{
		float scale;
		float *xyz = r_stagestaticverts[i].xyz;

		scale = 0.98f;
		scale = R_NoiseGet4f (xyz[0] * scale, xyz[1] * scale, xyz[2] * scale, tess.shaderTime * ds->deformationWave.frequency);
		r_stagestaticverts[i].normal[0] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f (100 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale, tess.shaderTime * ds->deformationWave.frequency);
		r_stagestaticverts[i].normal[1] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f (200 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale, tess.shaderTime * ds->deformationWave.frequency);
		r_stagestaticverts[i].normal[2] += ds->deformationWave.amplitude * scale;

		VectorNormalizeFast (r_stagestaticverts[i].normal);
	}
}

/*
========================
RB_CalcBulgeVertexes

========================
*/
void RB_CalcBulgeVertexes (deformStage_t *ds)
{
	float now = backEnd.refdef.time * ds->bulgeSpeed * 0.001f;

	for (int i = 0; i < tess.numVertexes; i++)
	{
		int off = (float) (FUNCTABLE_SIZE / (M_PI * 2)) * (r_stagestaticverts[i].st[0] * ds->bulgeWidth + now);
		float scale = tr.sinTable[off & FUNCTABLE_MASK] * ds->bulgeHeight;

		r_stagestaticverts[i].xyz[0] += r_stagestaticverts[i].normal[0] * scale;
		r_stagestaticverts[i].xyz[1] += r_stagestaticverts[i].normal[1] * scale;
		r_stagestaticverts[i].xyz[2] += r_stagestaticverts[i].normal[2] * scale;
	}
}


/*
======================
RB_CalcMoveVertexes

A deformation that can move an entire surface along a wave path
======================
*/
void RB_CalcMoveVertexes (deformStage_t *ds)
{
	int			i;
	float		*table;
	float		scale;
	vec3_t		offset;

	table = TableForFunc (ds->deformationWave.func);

	scale = WAVEVALUE (table, ds->deformationWave.base,
		ds->deformationWave.amplitude,
		ds->deformationWave.phase,
		ds->deformationWave.frequency);

	VectorScale (ds->moveVector, scale, offset);

	for (i = 0; i < tess.numVertexes; i++)
	{
		VectorAdd (r_stagestaticverts[i].xyz, offset, r_stagestaticverts[i].xyz);
	}
}


/*
=============
DeformText

Change a polygon into a bunch of text polygons
=============
*/
void DeformText (const char *text)
{
	int		i;
	vec3_t	origin, width, height;
	int		len;
	int		ch;
	byte	color[4];
	float	bottom, top;
	vec3_t	mid;

	height[0] = 0;
	height[1] = 0;
	height[2] = -1;
	CrossProduct (r_stagestaticverts[0].normal, height, width);

	// find the midpoint of the box
	VectorClear (mid);
	bottom = 999999;
	top = -999999;

	for (i = 0; i < 4; i++)
	{
		VectorAdd (r_stagestaticverts[i].xyz, mid, mid);

		if (r_stagestaticverts[i].xyz[2] < bottom) bottom = r_stagestaticverts[i].xyz[2];
		if (r_stagestaticverts[i].xyz[2] > top) top = r_stagestaticverts[i].xyz[2];
	}

	VectorScale (mid, 0.25f, origin);

	// determine the individual character size
	height[0] = 0;
	height[1] = 0;
	height[2] = (top - bottom) * 0.5f;

	VectorScale (width, height[2] * -0.75f, width);

	// determine the starting position
	len = strlen (text);
	VectorMA (origin, (len - 1), width, origin);

	// clear the shader indexes
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	color[0] = color[1] = color[2] = color[3] = 255;

	// draw each character
	for (i = 0; i < len; i++)
	{
		ch = text[i];
		ch &= 255;

		if (ch != ' ')
		{
			int		row, col;
			float	frow, fcol, size;

			row = ch >> 4;
			col = ch & 15;

			frow = row * 0.0625f;
			fcol = col * 0.0625f;
			size = 0.0625f;

			RB_AddQuadStampExt (origin, width, height, color, fcol, frow, fcol + size, frow + size);
		}

		VectorMA (origin, -2, width, origin);
	}
}

/*
==================
GlobalVectorToLocal
==================
*/
static void GlobalVectorToLocal (const vec3_t in, vec3_t out)
{
	out[0] = DotProduct (in, backEnd.or.axis[0]);
	out[1] = DotProduct (in, backEnd.or.axis[1]);
	out[2] = DotProduct (in, backEnd.or.axis[2]);
}

/*
=====================
AutospriteDeform

Assuming all the triangles for this shader are independant
quads, rebuild them as forward facing sprites
=====================
*/
static void AutospriteDeform (void)
{
	int		i;
	int		oldVerts;
	vec3_t	mid, delta;
	float	radius;
	vec3_t	left, up;
	vec3_t	leftDir, upDir;

	if (tess.numVertexes & 3)
		ri.Printf (PRINT_WARNING, "Autosprite shader %s had odd vertex count", tess.shader->name);

	if (tess.numIndexes != (tess.numVertexes >> 2) * 6)
		ri.Printf (PRINT_WARNING, "Autosprite shader %s had odd index count", tess.shader->name);

	oldVerts = tess.numVertexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	if (backEnd.currentEntity != &tr.worldEntity)
	{
		GlobalVectorToLocal (backEnd.viewParms.or.axis[1], leftDir);
		GlobalVectorToLocal (backEnd.viewParms.or.axis[2], upDir);
	}
	else
	{
		VectorCopy (backEnd.viewParms.or.axis[1], leftDir);
		VectorCopy (backEnd.viewParms.or.axis[2], upDir);
	}

	for (i = 0; i < oldVerts; i += 4)
	{
		// find the midpoint
		stagestaticvert_t *svert = &r_stagestaticverts[i];

		mid[0] = 0.25f * (svert[0].xyz[0] + svert[1].xyz[0] + svert[2].xyz[0] + svert[3].xyz[0]);
		mid[1] = 0.25f * (svert[0].xyz[1] + svert[1].xyz[1] + svert[2].xyz[1] + svert[3].xyz[1]);
		mid[2] = 0.25f * (svert[0].xyz[2] + svert[1].xyz[2] + svert[2].xyz[2] + svert[3].xyz[2]);

		VectorSubtract (svert[0].xyz, mid, delta);
		radius = VectorLength (delta) * 0.707f;		// / sqrt(2)

		VectorScale (leftDir, radius, left);
		VectorScale (upDir, radius, up);

		if (backEnd.viewParms.isMirror)
		{
			VectorSubtract (vec3_origin, left, left);
		}

		// compensate for scale in the axes if necessary
		if (backEnd.currentEntity->e.nonNormalizedAxes)
		{
			float axisLength;
			axisLength = VectorLength (backEnd.currentEntity->e.axis[0]);

			if (!axisLength)
				axisLength = 0;
			else axisLength = 1.0f / axisLength;

			VectorScale (left, axisLength, left);
			VectorScale (up, axisLength, up);
		}

		RB_AddQuadStamp (mid, left, up, tess.vertexColors[i]);
	}
}


/*
=====================
Autosprite2Deform

Autosprite2 will pivot a rectangular quad along the center of its long axis
=====================
*/
int edgeVerts[6][2] = {
	{0, 1},
	{0, 2},
	{0, 3},
	{1, 2},
	{1, 3},
	{2, 3}
};

static void Autosprite2Deform (void)
{
	int		i, j, k;
	int		indexes;
	vec3_t	forward;

	if (tess.numVertexes & 3)
		ri.Printf (PRINT_WARNING, "Autosprite2 shader %s had odd vertex count", tess.shader->name);

	if (tess.numIndexes != (tess.numVertexes >> 2) * 6)
		ri.Printf (PRINT_WARNING, "Autosprite2 shader %s had odd index count", tess.shader->name);

	if (backEnd.currentEntity != &tr.worldEntity)
		GlobalVectorToLocal (backEnd.viewParms.or.axis[0], forward);
	else Vector3Copy (forward, backEnd.viewParms.or.axis[0]);

	// this is a lot of work for two triangles...
	// we could precalculate a lot of it is an issue, but it would mess up
	// the shader abstraction
	for (i = 0, indexes = 0; i < tess.numVertexes; i += 4, indexes += 6)
	{
		float	lengths[2];
		int		nums[2];
		vec3_t	mid[2];
		vec3_t	major, minor;
		float	*v1, *v2;

		// find the midpoint
		stagestaticvert_t *svert = &r_stagestaticverts[i];

		// identify the two shortest edges
		nums[0] = nums[1] = 0;
		lengths[0] = lengths[1] = 999999;

		for (j = 0; j < 6; j++)
		{
			float	l;
			vec3_t	temp;

			v1 = svert[edgeVerts[j][0]].xyz;
			v2 = svert[edgeVerts[j][1]].xyz;

			VectorSubtract (v1, v2, temp);
			l = DotProduct (temp, temp);

			if (l < lengths[0])
			{
				nums[1] = nums[0];
				lengths[1] = lengths[0];
				nums[0] = j;
				lengths[0] = l;
			}
			else if (l < lengths[1])
			{
				nums[1] = j;
				lengths[1] = l;
			}
		}

		for (j = 0; j < 2; j++)
		{
			v1 = svert[edgeVerts[nums[j]][0]].xyz;
			v2 = svert[edgeVerts[nums[j]][1]].xyz;

			mid[j][0] = 0.5f * (v1[0] + v2[0]);
			mid[j][1] = 0.5f * (v1[1] + v2[1]);
			mid[j][2] = 0.5f * (v1[2] + v2[2]);
		}

		// find the vector of the major axis
		VectorSubtract (mid[1], mid[0], major);

		// cross this with the view direction to get minor axis
		CrossProduct (major, forward, minor);
		VectorNormalize (minor);

		// re-project the points
		for (j = 0; j < 2; j++)
		{
			float	l;

			v1 = svert[edgeVerts[nums[j]][0]].xyz;
			v2 = svert[edgeVerts[nums[j]][1]].xyz;

			l = 0.5 * sqrt (lengths[j]);

			// we need to see which direction this edge
			// is used to determine direction of projection
			for (k = 0; k < 5; k++)
				if (tess.indexes[indexes + k] == i + edgeVerts[nums[j]][0] && tess.indexes[indexes + k + 1] == i + edgeVerts[nums[j]][1])
					break;

			if (k == 5)
			{
				VectorMA (mid[j], l, minor, v1);
				VectorMA (mid[j], -l, minor, v2);
			}
			else
			{
				VectorMA (mid[j], -l, minor, v1);
				VectorMA (mid[j], l, minor, v2);
			}
		}
	}
}


/*
=====================
RB_DeformTessGeometry

=====================
*/
void RB_DeformTessGeometry (void)
{
	for (int i = 0; i < tess.shader->numDeforms; i++)
	{
		deformStage_t *ds = &tess.shader->deforms[i];

		switch (ds->deformation)
		{
		case DEFORM_NONE:
			break;

		case DEFORM_NORMALS:
			RB_CalcDeformNormals (ds);
			break;

		case DEFORM_WAVE:
			RB_CalcDeformVertexes (ds);
			break;

		case DEFORM_BULGE:
			RB_CalcBulgeVertexes (ds);
			break;

		case DEFORM_MOVE:
			RB_CalcMoveVertexes (ds);
			break;

		case DEFORM_PROJECTION_SHADOW:
			RB_ProjectionShadowDeform ();
			break;

		case DEFORM_AUTOSPRITE:
			AutospriteDeform ();
			break;

		case DEFORM_AUTOSPRITE2:
			Autosprite2Deform ();
			break;

		case DEFORM_TEXT0:
		case DEFORM_TEXT1:
		case DEFORM_TEXT2:
		case DEFORM_TEXT3:
		case DEFORM_TEXT4:
		case DEFORM_TEXT5:
		case DEFORM_TEXT6:
		case DEFORM_TEXT7:
			DeformText (backEnd.refdef.text[ds->deformation - DEFORM_TEXT0]);
			break;
		}
	}
}


/*
====================================================================

COLORS

====================================================================
*/

void RB_CalcWaveColor (const waveForm_t *wf, unsigned char *dstColors)
{
	float glow;

	if (wf->func == GF_NOISE)
		glow = wf->base + R_NoiseGet4f (0, 0, 0, (tess.shaderTime + wf->phase) * wf->frequency) * wf->amplitude;
	else glow = EvalWaveForm (wf) * tr.identityLight;

	if (glow < 0)
		glow = 0;
	else if (glow > 1)
		glow = 1;

	dstColors[0] = dstColors[1] = dstColors[2] = myftol (255 * glow);
	dstColors[3] = 255;
}


void RB_CalcWaveAlpha (const waveForm_t *wf, unsigned char *dstColors)
{
	dstColors[3] = 255 * EvalWaveFormClamped (wf);
}


void RB_CalcModulateColorsByFog (void)
{
	fogparms_t fogparms;

	RB_CalcFogParms (&tr.world->fogs[tess.fogNum], &fogparms);

	for (int i = 0; i < tess.numVertexes; i++)
	{
		float st[2];

		RB_CalcFogTexCoord (&fogparms, r_stagestaticverts[i].xyz, st);

		float f = 1.0 - R_FogFactor (st[0], st[1]);

		r_stagedynamicverts[i].rgba[0] *= f;
		r_stagedynamicverts[i].rgba[1] *= f;
		r_stagedynamicverts[i].rgba[2] *= f;
	}
}


void RB_CalcModulateAlphasByFog (void)
{
	fogparms_t fogparms;

	RB_CalcFogParms (&tr.world->fogs[tess.fogNum], &fogparms);

	for (int i = 0; i < tess.numVertexes; i++)
	{
		float st[2];

		RB_CalcFogTexCoord (&fogparms, r_stagestaticverts[i].xyz, st);

		float f = 1.0 - R_FogFactor (st[0], st[1]);

		r_stagedynamicverts[i].rgba[3] *= f;
	}
}


void RB_CalcModulateRGBAsByFog (void)
{
	fogparms_t fogparms;

	RB_CalcFogParms (&tr.world->fogs[tess.fogNum], &fogparms);

	for (int i = 0; i < tess.numVertexes; i++)
	{
		float st[2];

		RB_CalcFogTexCoord (&fogparms, r_stagestaticverts[i].xyz, st);

		float f = 1.0 - R_FogFactor (st[0], st[1]);

		r_stagedynamicverts[i].rgba[0] *= f;
		r_stagedynamicverts[i].rgba[1] *= f;
		r_stagedynamicverts[i].rgba[2] *= f;
		r_stagedynamicverts[i].rgba[3] *= f;
	}
}


/*
====================================================================

TEX COORDS

====================================================================
*/

void RB_CalcFogParms (fog_t *fog, fogparms_t *parms)
{
	vec3_t local;

	// all fogging distance is based on world Z units
	VectorSubtract (backEnd.or.origin, backEnd.viewParms.or.origin, local);
	parms->DistanceVector[0] = -backEnd.or.modelMatrix[2];
	parms->DistanceVector[1] = -backEnd.or.modelMatrix[6];
	parms->DistanceVector[2] = -backEnd.or.modelMatrix[10];
	parms->DistanceVector[3] = DotProduct (local, backEnd.viewParms.or.axis[0]);

	// scale the fog vectors based on the fog's thickness
	parms->DistanceVector[0] *= fog->tcScale;
	parms->DistanceVector[1] *= fog->tcScale;
	parms->DistanceVector[2] *= fog->tcScale;
	parms->DistanceVector[3] *= fog->tcScale;

	// rotate the gradient vector for this orientation
	if (fog->hasSurface)
	{
		parms->DepthVector[0] = fog->surface[0] * backEnd.or.axis[0][0] + fog->surface[1] * backEnd.or.axis[0][1] + fog->surface[2] * backEnd.or.axis[0][2];
		parms->DepthVector[1] = fog->surface[0] * backEnd.or.axis[1][0] + fog->surface[1] * backEnd.or.axis[1][1] + fog->surface[2] * backEnd.or.axis[1][2];
		parms->DepthVector[2] = fog->surface[0] * backEnd.or.axis[2][0] + fog->surface[1] * backEnd.or.axis[2][1] + fog->surface[2] * backEnd.or.axis[2][2];
		parms->DepthVector[3] = -fog->surface[3] + DotProduct (backEnd.or.origin, fog->surface);

		parms->eyeT = DotProduct (backEnd.or.viewOrigin, parms->DepthVector) + parms->DepthVector[3];
	}
	else
	{
		// non-surface fog always has eye inside
		parms->eyeT = 1;
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog
	if (parms->eyeT < 0)
		parms->eyeOutside = qtrue;
	else parms->eyeOutside = qfalse;

	parms->DistanceVector[3] += 1.0 / 512;
}


void RB_CalcFogTexCoord (fogparms_t *parms, float *v, float *st)
{
	// calculate the length in fog
	float s = DotProduct (v, parms->DistanceVector) + parms->DistanceVector[3];
	float t = DotProduct (v, parms->DepthVector) + parms->DepthVector[3];

	// partially clipped fogs use the T axis		
	if (parms->eyeOutside)
	{
		if (t < 1.0)
		{
			// point is outside, so no fogging
			t = 1.0 / 32;
		}
		else
		{
			// cut the distance at the fog plane
			t = 1.0 / 32 + 30.0 / 32 * t / (t - parms->eyeT);
		}
	}
	else
	{
		if (t < 0)
		{
			// point is outside, so no fogging
			t = 1.0 / 32;
		}
		else t = 31.0 / 32;
	}

	st[0] = s;
	st[1] = t;
}


#if id386 && !((defined __linux__ || defined __FreeBSD__) && (defined __i386__)) // rb010123
long myftol (float f)
{
	static int tmp;
	__asm fld f
	__asm fistp tmp
	__asm mov eax, tmp
}
#endif

/*
** RB_CalcSpecularAlpha
**
** Calculates specular coefficient and places it in the alpha channel
*/
vec3_t lightOrigin = {-960, 1980, 96};		// FIXME: track dynamically

void RB_CalcSpecularAlpha (void)
{
	for (int i = 0; i < tess.numVertexes; i++)
	{
		vec3_t viewer, reflected, lightDir;
		float l, ilength;
		int b;

		VectorSubtract (lightOrigin, r_stagestaticverts[i].xyz, lightDir);
		VectorNormalizeFast (lightDir);

		// calculate the specular color
		float d = DotProduct (r_stagestaticverts[i].normal, lightDir);

		// we don't optimize for the d < 0 case since this tends to
		// cause visual artifacts such as faceted "snapping"
		reflected[0] = r_stagestaticverts[i].normal[0] * 2 * d - lightDir[0];
		reflected[1] = r_stagestaticverts[i].normal[1] * 2 * d - lightDir[1];
		reflected[2] = r_stagestaticverts[i].normal[2] * 2 * d - lightDir[2];

		VectorSubtract (backEnd.or.viewOrigin, r_stagestaticverts[i].xyz, viewer);
		ilength = Q_rsqrt (DotProduct (viewer, viewer));

		l = DotProduct (reflected, viewer);
		l *= ilength;

		if (l < 0)
			r_stagedynamicverts[i].rgba[3] = 0;
		else
		{
			l = l * l;
			l = l * l;
			b = l * 255;

			if (b > 255)
				r_stagedynamicverts[i].rgba[3] = 255;
			else r_stagedynamicverts[i].rgba[3] = b;
		}
	}
}


void RB_CalcPortalAlpha (void)
{
	for (int i = 0; i < tess.numVertexes; i++)
	{
		float len;
		vec3_t v;

		VectorSubtract (r_stagestaticverts[i].xyz, backEnd.viewParms.or.origin, v);

		len = VectorLength (v);
		len /= tess.shader->portalRange;

		if (len < 0)
			r_stagedynamicverts[i].rgba[3] = 0;
		else if (len > 1)
			r_stagedynamicverts[i].rgba[3] = 0xff;
		else r_stagedynamicverts[i].rgba[3] = len * 0xff;
	}
}


/*
** RB_CalcDiffuseColor
**
** The basic vertex lighting calc
*/
void RB_CalcDiffuseColor (void)
{
	trRefEntity_t *ent = backEnd.currentEntity;

	for (int i = 0; i < tess.numVertexes; i++)
	{
		float incoming = DotProduct (r_stagestaticverts[i].normal, ent->lightDir);

		if (incoming <= 0)
		{
			r_stagedynamicverts[i].rgba[0] = myftol (ent->ambientLight[0]);
			r_stagedynamicverts[i].rgba[1] = myftol (ent->ambientLight[1]);
			r_stagedynamicverts[i].rgba[2] = myftol (ent->ambientLight[2]);
		}
		else
		{
			for (int j = 0; j < 3; j++)
			{
				int c = myftol (ent->ambientLight[j] + incoming * ent->directedLight[j]);

				if (c > 255)
					r_stagedynamicverts[i].rgba[j] = 255;
				else r_stagedynamicverts[i].rgba[j] = c;
			}
		}

		r_stagedynamicverts[i].rgba[3] = 255;
	}
}



void RB_SetupTCModTransform (int modstage, texModInfo_t *tmi)
{
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_TMODMATRIX0 + modstage, tmi->matrix[0], 1);
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_TRANSLATE0 + modstage, float4 (tmi->translate[0], tmi->translate[1], 0, 0), 1);
}


void RB_SetupTCModScroll (int modstage, const float scrollSpeed[2])
{
	float timeScale = tess.shaderTime;
	float adjustedScroll[] = {scrollSpeed[0] * timeScale, scrollSpeed[1] * timeScale, 0, 0};
	texModInfo_t tmi;

	tmi.matrix[0][0] = 1;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = adjustedScroll[0] - floor (adjustedScroll[0]);

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = 1;
	tmi.translate[1] = adjustedScroll[1] - floor (adjustedScroll[1]);

	RB_SetupTCModTransform (modstage, &tmi);
}


void RB_SetupTCModScale (int modstage, const float scale[2])
{
	texModInfo_t tmi;

	tmi.matrix[0][0] = scale[0];
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = scale[1];
	tmi.translate[1] = 0;

	RB_SetupTCModTransform (modstage, &tmi);
}


void RB_SetupTCModRotate (int modstage, float rotateSpeed)
{
	float timeScale = tess.shaderTime;
	float degs = -rotateSpeed * timeScale;
	int index = degs * (FUNCTABLE_SIZE / 360.0f);
	texModInfo_t tmi;

	float sinValue = tr.sinTable[index & FUNCTABLE_MASK];
	float cosValue = tr.sinTable[(index + FUNCTABLE_SIZE / 4) & FUNCTABLE_MASK];

	tmi.matrix[0][0] = cosValue;
	tmi.matrix[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.matrix[0][1] = sinValue;
	tmi.matrix[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_SetupTCModTransform (modstage, &tmi);
}


void RB_SetupTCModStretch (int modstage, const waveForm_t *wf)
{
	float p = 1.0f / EvalWaveForm (wf);
	texModInfo_t tmi;

	tmi.matrix[0][0] = p;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_SetupTCModTransform (modstage, &tmi);
}


void RB_SetupTCGenFog (void)
{
	fogparms_t fogparms;

	RB_CalcFogParms (&tr.world->fogs[tess.fogNum], &fogparms);

	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_FOGDISTANCE, fogparms.DistanceVector, 1);
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_FOGDEPTH, fogparms.DepthVector, 1);
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_FOGEYET, float4 (fogparms.eyeT, 0, 0, 0), 1);
	d3d_Device->lpVtbl->SetPixelShaderConstantF (d3d_Device, PSREG_FOGEYEOUTSIDE, float4 (fogparms.eyeOutside, 0, 0, 0), 1);
}

