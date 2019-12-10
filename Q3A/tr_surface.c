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

// tr_surf.c
#include "tr_local.h"
#include "tr_program.h"

/*

THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================


/*
==============
RB_CheckOverflow
==============
*/
void RB_CheckOverflow (int verts, int indexes)
{
	if (tess.numVertexes + verts < SHADER_MAX_VERTEXES && tess.numIndexes + indexes < SHADER_MAX_INDEXES)
	{
		return;
	}

	RB_EndSurface ();

	if (verts >= SHADER_MAX_VERTEXES)
	{
		ri.Error (ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES);
	}
	if (indexes >= SHADER_MAX_INDEXES)
	{
		ri.Error (ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES);
	}

	RB_BeginSurface (tess.shader, tess.fogNum);
}


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt (vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2)
{
	vec3_t		normal;
	int			ndx;

	RB_CHECKOVERFLOW (4, 6);

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[tess.numIndexes++] = ndx;
	tess.indexes[tess.numIndexes++] = ndx + 1;
	tess.indexes[tess.numIndexes++] = ndx + 3;

	tess.indexes[tess.numIndexes++] = ndx + 3;
	tess.indexes[tess.numIndexes++] = ndx + 1;
	tess.indexes[tess.numIndexes++] = ndx + 2;

	r_stagestaticverts[ndx].xyz[0] = origin[0] + left[0] + up[0];
	r_stagestaticverts[ndx].xyz[1] = origin[1] + left[1] + up[1];
	r_stagestaticverts[ndx].xyz[2] = origin[2] + left[2] + up[2];

	r_stagestaticverts[ndx + 1].xyz[0] = origin[0] - left[0] + up[0];
	r_stagestaticverts[ndx + 1].xyz[1] = origin[1] - left[1] + up[1];
	r_stagestaticverts[ndx + 1].xyz[2] = origin[2] - left[2] + up[2];

	r_stagestaticverts[ndx + 2].xyz[0] = origin[0] - left[0] - up[0];
	r_stagestaticverts[ndx + 2].xyz[1] = origin[1] - left[1] - up[1];
	r_stagestaticverts[ndx + 2].xyz[2] = origin[2] - left[2] - up[2];

	r_stagestaticverts[ndx + 3].xyz[0] = origin[0] + left[0] - up[0];
	r_stagestaticverts[ndx + 3].xyz[1] = origin[1] + left[1] - up[1];
	r_stagestaticverts[ndx + 3].xyz[2] = origin[2] + left[2] - up[2];

	// constant normal all the way around
	VectorSubtract (vec3_origin, backEnd.viewParms.or.axis[0], normal);

	r_stagestaticverts[ndx].normal[0] = r_stagestaticverts[ndx + 1].normal[0] = r_stagestaticverts[ndx + 2].normal[0] = r_stagestaticverts[ndx + 3].normal[0] = normal[0];
	r_stagestaticverts[ndx].normal[1] = r_stagestaticverts[ndx + 1].normal[1] = r_stagestaticverts[ndx + 2].normal[1] = r_stagestaticverts[ndx + 3].normal[1] = normal[1];
	r_stagestaticverts[ndx].normal[2] = r_stagestaticverts[ndx + 1].normal[2] = r_stagestaticverts[ndx + 2].normal[2] = r_stagestaticverts[ndx + 3].normal[2] = normal[2];

	// standard square texture coordinates
	r_stagestaticverts[ndx].st[0] = r_stagestaticverts[ndx].lm[0] = s1;
	r_stagestaticverts[ndx].st[1] = r_stagestaticverts[ndx].lm[1] = t1;

	r_stagestaticverts[ndx + 1].st[0] = r_stagestaticverts[ndx + 1].lm[0] = s2;
	r_stagestaticverts[ndx + 1].st[1] = r_stagestaticverts[ndx + 1].lm[1] = t1;

	r_stagestaticverts[ndx + 2].st[0] = r_stagestaticverts[ndx + 2].lm[0] = s2;
	r_stagestaticverts[ndx + 2].st[1] = r_stagestaticverts[ndx + 2].lm[1] = t2;

	r_stagestaticverts[ndx + 3].st[0] = r_stagestaticverts[ndx + 3].lm[0] = s1;
	r_stagestaticverts[ndx + 3].st[1] = r_stagestaticverts[ndx + 3].lm[1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	* (unsigned int *) &tess.vertexColors[ndx] =
		*(unsigned int *) &tess.vertexColors[ndx + 1] =
		*(unsigned int *) &tess.vertexColors[ndx + 2] =
		*(unsigned int *) &tess.vertexColors[ndx + 3] =
		*(unsigned int *) color;

	tess.numVertexes += 4;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp (vec3_t origin, vec3_t left, vec3_t up, byte *color)
{
	RB_AddQuadStampExt (origin, left, up, color, 0, 0, 1, 1);
}

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite (void)
{
	vec3_t		left, up;
	float		radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;

	if (backEnd.currentEntity->e.rotation == 0)
	{
		VectorScale (backEnd.viewParms.or.axis[1], radius, left);
		VectorScale (backEnd.viewParms.or.axis[2], radius, up);
	}
	else
	{
		float	s, c;
		float	ang;

		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin (ang);
		c = cos (ang);

		VectorScale (backEnd.viewParms.or.axis[1], c * radius, left);
		VectorMA (left, -s * radius, backEnd.viewParms.or.axis[2], left);

		VectorScale (backEnd.viewParms.or.axis[2], c * radius, up);
		VectorMA (up, s * radius, backEnd.viewParms.or.axis[1], up);
	}

	if (backEnd.viewParms.isMirror)
	{
		VectorSubtract (vec3_origin, left, left);
	}

	RB_AddQuadStamp (backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA);
}


/*
=============
RB_SurfacePolychain
=============
*/
void RB_SurfacePolychain (srfPoly_t *p)
{
	int		i;
	int		numv;

	RB_CHECKOVERFLOW (p->numVerts, 3 * (p->numVerts - 2));

	// fan triangles into the tess array
	numv = tess.numVertexes;

	for (i = 0; i < p->numVerts; i++)
	{
		VectorCopy (p->verts[i].xyz, r_stagestaticverts[numv].xyz);

		r_stagestaticverts[numv].st[0] = p->verts[i].st[0];
		r_stagestaticverts[numv].st[1] = p->verts[i].st[1];

		*(int *) &tess.vertexColors[numv] = *(int *) p->verts[i].modulate;
		numv++;
	}

	// generate fan indexes into the tess array
	for (i = 0; i < p->numVerts - 2; i++)
	{
		tess.indexes[tess.numIndexes++] = tess.numVertexes;
		tess.indexes[tess.numIndexes++] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes + i + 2;
	}

	tess.numVertexes = numv;
}


/*
=============
RB_SurfaceTriangles
=============
*/
void RB_SurfaceTriangles (srfTriangles_t *srf)
{
	int			i;
	drawVert_t	*dv;
	byte		*color;
	int			dlightBits;

	dlightBits = srf->dlightBits;
	tess.dlightBits |= dlightBits;

	RB_CHECKOVERFLOW (srf->numVerts, srf->numIndexes);

	for (i = 0; i < srf->numIndexes; i += 3)
	{
		tess.indexes[tess.numIndexes + i + 0] = tess.numVertexes + srf->indexes[i + 0];
		tess.indexes[tess.numIndexes + i + 1] = tess.numVertexes + srf->indexes[i + 1];
		tess.indexes[tess.numIndexes + i + 2] = tess.numVertexes + srf->indexes[i + 2];
	}

	tess.numIndexes += srf->numIndexes;

	stagestaticvert_t *sverts = &r_stagestaticverts[tess.numVertexes];

	dv = srf->verts;

	color = tess.vertexColors[tess.numVertexes];

	for (i = 0; i < srf->numVerts; i++, dv++, color += 4)
	{
		sverts[i].xyz[0] = dv->xyz[0];
		sverts[i].xyz[1] = dv->xyz[1];
		sverts[i].xyz[2] = dv->xyz[2];

		sverts[i].normal[0] = dv->normal[0];
		sverts[i].normal[1] = dv->normal[1];
		sverts[i].normal[2] = dv->normal[2];

		sverts[i].st[0] = dv->st[0];
		sverts[i].st[1] = dv->st[1];

		sverts[i].lm[0] = dv->lightmap[0];
		sverts[i].lm[1] = dv->lightmap[1];

		*(int *) color = *(int *) dv->color;
	}

	tess.numVertexes += srf->numVerts;
}



/*
==============
RB_SurfaceBeam
==============
*/
void RB_SurfaceBeam (void)
{
#ifdef GL_REMOVED
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize (normalized_direction) == 0)
		return;

	PerpendicularVector (perpvec, normalized_direction);

	VectorScale (perpvec, 4, perpvec);

	for (i = 0; i < NUM_BEAM_SEGS; i++)
	{
		RotatePointAroundVector (start_points[i], normalized_direction, perpvec, (360.0 / NUM_BEAM_SEGS)*i);
		//		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd (start_points[i], direction, end_points[i]);
	}

	GL_BindTexture (0, tr.whiteImage);

	GL_State (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);

	glColor3f (1, 0, 0);

	glBegin (GL_TRIANGLE_STRIP);
	for (i = 0; i <= NUM_BEAM_SEGS; i++)
	{
		glVertex3fv (start_points[i % NUM_BEAM_SEGS]);
		glVertex3fv (end_points[i % NUM_BEAM_SEGS]);
	}
	glEnd ();
#endif
}

//================================================================================

static void DoRailCore (const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth)
{
	float		spanWidth2;
	int			vbase;
	float		t = len / 256.0f;

	vbase = tess.numVertexes;

	spanWidth2 = -spanWidth;

	// FIXME: use quad stamp?
	VectorMA (start, spanWidth, up, r_stagestaticverts[tess.numVertexes].xyz);
	r_stagestaticverts[tess.numVertexes].st[0] = 0;
	r_stagestaticverts[tess.numVertexes].st[1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0] * 0.25;
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1] * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2] * 0.25;
	tess.numVertexes++;

	VectorMA (start, spanWidth2, up, r_stagestaticverts[tess.numVertexes].xyz);
	r_stagestaticverts[tess.numVertexes].st[0] = 0;
	r_stagestaticverts[tess.numVertexes].st[1] = 1;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	VectorMA (end, spanWidth, up, r_stagestaticverts[tess.numVertexes].xyz);
	r_stagestaticverts[tess.numVertexes].st[0] = t;
	r_stagestaticverts[tess.numVertexes].st[1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	VectorMA (end, spanWidth2, up, r_stagestaticverts[tess.numVertexes].xyz);
	r_stagestaticverts[tess.numVertexes].st[0] = t;
	r_stagestaticverts[tess.numVertexes].st[1] = 1;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

static void DoRailDiscs (int numSegs, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up)
{
	int i;
	vec3_t	pos[4];
	vec3_t	v;
	int		spanWidth = r_railWidth->integer;
	float c, s;
	float		scale;

	if (numSegs > 1)
		numSegs--;
	if (!numSegs)
		return;

	scale = 0.25;

	for (i = 0; i < 4; i++)
	{
		c = cos (DEG2RAD (45 + i * 90));
		s = sin (DEG2RAD (45 + i * 90));
		v[0] = (right[0] * c + up[0] * s) * scale * spanWidth;
		v[1] = (right[1] * c + up[1] * s) * scale * spanWidth;
		v[2] = (right[2] * c + up[2] * s) * scale * spanWidth;
		VectorAdd (start, v, pos[i]);

		if (numSegs > 1)
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd (pos[i], dir, pos[i]);
		}
	}

	for (i = 0; i < numSegs; i++)
	{
		int j;

		RB_CHECKOVERFLOW (4, 6);

		for (j = 0; j < 4; j++)
		{
			VectorCopy (pos[j], r_stagestaticverts[tess.numVertexes].xyz);

			r_stagestaticverts[tess.numVertexes].st[0] = (j < 2);
			r_stagestaticverts[tess.numVertexes].st[1] = (j && j != 3);

			tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
			tess.numVertexes++;

			VectorAdd (pos[j], dir, pos[j]);
		}

		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 0;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 2;
	}
}

/*
** RB_SurfaceRailRinges
*/
void RB_SurfaceRailRings (void)
{
	refEntity_t *e;
	int			numSegs;
	int			len;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy (e->oldorigin, start);
	VectorCopy (e->origin, end);

	// compute variables
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	MakeNormalVectors (vec, right, up);
	numSegs = (len) / r_railSegmentLength->value;
	if (numSegs <= 0)
	{
		numSegs = 1;
	}

	VectorScale (vec, r_railSegmentLength->value, vec);

	DoRailDiscs (numSegs, start, vec, right, up);
}

/*
** RB_SurfaceRailCore
*/
void RB_SurfaceRailCore (void)
{
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy (e->oldorigin, start);
	VectorCopy (e->origin, end);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	// compute side vector
	VectorSubtract (start, backEnd.viewParms.or.origin, v1);
	VectorNormalize (v1);
	VectorSubtract (end, backEnd.viewParms.or.origin, v2);
	VectorNormalize (v2);
	CrossProduct (v1, v2, right);
	VectorNormalize (right);

	DoRailCore (start, end, right, len, r_railCoreWidth->integer);
}

/*
** RB_SurfaceLightningBolt
*/
void RB_SurfaceLightningBolt (void)
{
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	e = &backEnd.currentEntity->e;

	VectorCopy (e->oldorigin, end);
	VectorCopy (e->origin, start);

	// compute variables
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	// compute side vector
	VectorSubtract (start, backEnd.viewParms.or.origin, v1);
	VectorNormalize (v1);
	VectorSubtract (end, backEnd.viewParms.or.origin, v2);
	VectorNormalize (v2);
	CrossProduct (v1, v2, right);
	VectorNormalize (right);

	for (i = 0; i < 4; i++)
	{
		vec3_t	temp;

		DoRailCore (start, end, right, len, 8);
		RotatePointAroundVector (temp, vec, right, 45);
		VectorCopy (temp, right);
	}
}

/*
** VectorArrayNormalize
*
* The inputs to this routing seem to always be close to length = 1.0 (about 0.6 to 2.0)
* This means that we don't have to worry about zero length or enormously long vectors.
*/
static void VectorArrayNormalize (stagestaticvert_t *verts, unsigned int count)
{
	// No assembly version for this architecture, or C_ONLY defined
	// given the input, it's safe to call VectorNormalizeFast
	while (count--)
	{
		VectorNormalizeFast (verts->normal);
		verts++;
	}
}


/*
** LerpMeshVertexes
*/
static void LerpMeshVertexes (md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	stagestaticvert_t *out = &r_stagestaticverts[tess.numVertexes];

	newXyz = (short *) ((byte *) surf + surf->ofsXyzNormals) + (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if (backlerp == 0)
	{
		// just copy the vertexes
		for (vertNum = 0; vertNum < numVerts; vertNum++,
			newXyz += 4, newNormals += 4,
			out++)
		{
			out->xyz[0] = newXyz[0] * newXyzScale;
			out->xyz[1] = newXyz[1] * newXyzScale;
			out->xyz[2] = newXyz[2] * newXyzScale;

			lat = (newNormals[0] >> 8) & 0xff;
			lng = (newNormals[0] & 0xff);
			lat *= (FUNCTABLE_SIZE / 256);
			lng *= (FUNCTABLE_SIZE / 256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			out->normal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			out->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			out->normal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK];
		}
	}
	else
	{
		// interpolate and copy the vertex and normal
		oldXyz = (short *) ((byte *) surf + surf->ofsXyzNormals) + (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum = 0; vertNum < numVerts; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			out++)
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			out->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			out->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			out->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = (newNormals[0] >> 8) & 0xff;
			lng = (newNormals[0] & 0xff);
			lat *= 4;
			lng *= 4;

			uncompressedNewNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK];

			lat = (oldNormals[0] >> 8) & 0xff;
			lng = (oldNormals[0] & 0xff);
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4))&FUNCTABLE_MASK];

			out->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			out->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			out->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;
		}

		VectorArrayNormalize (&r_stagestaticverts[tess.numVertexes], numVerts);
	}
}


/*
=============
RB_SurfaceMesh
=============
*/
void RB_SurfaceMesh (md3Surface_t *surface)
{
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Bob, Doug;
	int				numVerts;

	if (backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame)
		backlerp = 0;
	else backlerp = backEnd.currentEntity->e.backlerp;

	RB_CHECKOVERFLOW (surface->numVerts, surface->numTriangles * 3);

	LerpMeshVertexes (surface, backlerp);

	triangles = (int *) ((byte *) surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	Bob = tess.numIndexes;
	Doug = tess.numVertexes;

	for (j = 0; j < indexes; j++)
		tess.indexes[Bob + j] = Doug + triangles[j];

	tess.numIndexes += indexes;

	texCoords = (float *) ((byte *) surface + surface->ofsSt);
	numVerts = surface->numVerts;

	for (j = 0; j < numVerts; j++)
	{
		r_stagestaticverts[Doug + j].st[0] = texCoords[j * 2 + 0];
		r_stagestaticverts[Doug + j].st[1] = texCoords[j * 2 + 1];

		// FIXME: fill in lightmapST for completeness?
	}

	tess.numVertexes += surface->numVerts;
}


/*
==============
RB_SurfaceFace
==============
*/
void RB_SurfaceFace (srfSurfaceFace_t *surf)
{
	int			i;
	unsigned	*indices;
	unsigned	short *tessIndexes;
	float		*v;
	float		*normal;
	int			ndx;
	int			Bob;
	int			numPoints;
	int			dlightBits;

	RB_CHECKOVERFLOW (surf->numPoints, surf->numIndices);

	dlightBits = surf->dlightBits;
	tess.dlightBits |= dlightBits;

	indices = (unsigned *) (((char  *) surf) + surf->ofsIndices);

	Bob = tess.numVertexes;
	tessIndexes = tess.indexes + tess.numIndexes;

	for (i = surf->numIndices - 1; i >= 0; i--)
	{
		tessIndexes[i] = indices[i] + Bob;
	}

	tess.numIndexes += surf->numIndices;

	ndx = tess.numVertexes;
	numPoints = surf->numPoints;
	normal = surf->plane.normal;

	for (i = 0, v = surf->points[0]; i < numPoints; i++, v += VERTEXSIZE, ndx++)
	{
		VectorCopy (v, r_stagestaticverts[ndx].xyz);

		r_stagestaticverts[ndx].st[0] = v[3];
		r_stagestaticverts[ndx].st[1] = v[4];
		r_stagestaticverts[ndx].lm[0] = v[5];
		r_stagestaticverts[ndx].lm[1] = v[6];

		r_stagestaticverts[ndx].normal[0] = normal[0];
		r_stagestaticverts[ndx].normal[1] = normal[1];
		r_stagestaticverts[ndx].normal[2] = normal[2];

		*(unsigned int *) &tess.vertexColors[ndx] = *(unsigned int *) &v[7];
	}

	tess.numVertexes += surf->numPoints;
}


static float	LodErrorForVolume (vec3_t local, float radius)
{
	vec3_t		world;
	float		d;

	// never let it go negative
	if (r_lodCurveError->value < 0)
	{
		return 0;
	}

	world[0] = local[0] * backEnd.or.axis[0][0] + local[1] * backEnd.or.axis[1][0] + local[2] * backEnd.or.axis[2][0] + backEnd.or.origin[0];
	world[1] = local[0] * backEnd.or.axis[0][1] + local[1] * backEnd.or.axis[1][1] + local[2] * backEnd.or.axis[2][1] + backEnd.or.origin[1];
	world[2] = local[0] * backEnd.or.axis[0][2] + local[1] * backEnd.or.axis[1][2] + local[2] * backEnd.or.axis[2][2] + backEnd.or.origin[2];

	VectorSubtract (world, backEnd.viewParms.or.origin, world);
	d = DotProduct (world, backEnd.viewParms.or.axis[0]);

	if (d < 0)
	{
		d = -d;
	}
	d -= radius;
	if (d < 1)
	{
		d = 1;
	}

	return r_lodCurveError->value / d;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
void RB_SurfaceGrid (srfGridMesh_t *cv)
{
	int		i, j;
	unsigned char *color;
	drawVert_t	*dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;
	int		dlightBits;

	dlightBits = cv->dlightBits;
	tess.dlightBits |= dlightBits;

	// determine the allowable discrepance
	lodError = LodErrorForVolume (cv->lodOrigin, cv->lodRadius);

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;

	for (i = 1; i < cv->width - 1; i++)
	{
		if (cv->widthLodError[i] <= lodError)
		{
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}

	widthTable[lodWidth] = cv->width - 1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;

	for (i = 1; i < cv->height - 1; i++)
	{
		if (cv->heightLodError[i] <= lodError)
		{
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}

	heightTable[lodHeight] = cv->height - 1;
	lodHeight++;

	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes
	used = 0;
	rows = 0;

	while (used < lodHeight - 1)
	{
		// see how many rows of both verts and indexes we can add without overflowing
		do
		{
			vrows = (SHADER_MAX_VERTEXES - tess.numVertexes) / lodWidth;
			irows = (SHADER_MAX_INDEXES - tess.numIndexes) / (lodWidth * 6);

			// if we don't have enough space for at least one strip, flush the buffer
			if (vrows < 2 || irows < 1)
			{
				RB_EndSurface ();
				RB_BeginSurface (tess.shader, tess.fogNum);
			}
			else
			{
				break;
			}
		} while (1);

		rows = irows;
		if (vrows < irows + 1)
		{
			rows = vrows - 1;
		}
		if (used + rows > lodHeight)
		{
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		stagestaticvert_t *sverts = &r_stagestaticverts[numVertexes];

		color = (unsigned char *) &tess.vertexColors[numVertexes];

		for (i = 0; i < rows; i++)
		{
			for (j = 0; j < lodWidth; j++)
			{
				dv = cv->verts + heightTable[used + i] * cv->width + widthTable[j];

				sverts->xyz[0] = dv->xyz[0];
				sverts->xyz[1] = dv->xyz[1];
				sverts->xyz[2] = dv->xyz[2];

				sverts->st[0] = dv->st[0];
				sverts->st[1] = dv->st[1];
				sverts->lm[0] = dv->lightmap[0];
				sverts->lm[1] = dv->lightmap[1];

				sverts->normal[0] = dv->normal[0];
				sverts->normal[1] = dv->normal[1];
				sverts->normal[2] = dv->normal[2];

				*(unsigned int *) color = *(unsigned int *) dv->color;

				sverts++;
				color += 4;
			}
		}

		// add the indexes
		int h = rows - 1;
		int w = lodWidth - 1;
		int numIndexes = tess.numIndexes;

		for (i = 0; i < h; i++)
		{
			for (j = 0; j < w; j++)
			{
				int		v1, v2, v3, v4;

				// vertex order to be reckognized as tristrips
				v1 = numVertexes + i*lodWidth + j + 1;
				v2 = v1 - 1;
				v3 = v2 + lodWidth;
				v4 = v3 + 1;

				tess.indexes[numIndexes++] = v2;
				tess.indexes[numIndexes++] = v3;
				tess.indexes[numIndexes++] = v1;

				tess.indexes[numIndexes++] = v1;
				tess.indexes[numIndexes++] = v3;
				tess.indexes[numIndexes++] = v4;
			}
		}

		tess.numIndexes = numIndexes;
		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	}
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
void RB_SurfaceAxis (void)
{
}

//===========================================================================

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
void RB_SurfaceEntity (surfaceType_t *surfType)
{
	switch (backEnd.currentEntity->e.reType)
	{
	case RT_SPRITE:
		RB_SurfaceSprite ();
		break;
	case RT_BEAM:
		RB_SurfaceBeam ();
		break;
	case RT_RAIL_CORE:
		RB_SurfaceRailCore ();
		break;
	case RT_RAIL_RINGS:
		RB_SurfaceRailRings ();
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt ();
		break;
	default:
		RB_SurfaceAxis ();
		break;
	}

	return;
}

void RB_SurfaceBad (surfaceType_t *surfType)
{
	ri.Printf (PRINT_ALL, "Bad surface tesselated.\n");
}

#if 0

void RB_SurfaceFlare (srfFlare_t *surf)
{
	vec3_t		left, up;
	float		radius;
	byte		color[4];
	vec3_t		dir;
	vec3_t		origin;
	float		d;

	// calculate the xyz locations for the four corners
	radius = 30;
	VectorScale (backEnd.viewParms.or.axis[1], radius, left);
	VectorScale (backEnd.viewParms.or.axis[2], radius, up);
	if (backEnd.viewParms.isMirror)
	{
		VectorSubtract (vec3_origin, left, left);
	}

	color[0] = color[1] = color[2] = color[3] = 255;

	VectorMA (surf->origin, 3, surf->normal, origin);
	VectorSubtract (origin, backEnd.viewParms.or.origin, dir);
	VectorNormalize (dir);
	VectorMA (origin, r_ignore->value, dir, origin);

	d = -DotProduct (dir, surf->normal);
	if (d < 0)
	{
		return;
	}
#if 0
	color[0] *= d;
	color[1] *= d;
	color[2] *= d;
#endif

	RB_AddQuadStamp (origin, left, up, color);
}

#else

void RB_SurfaceFlare (srfFlare_t *surf)
{
#if 0
	vec3_t		left, up;
	byte		color[4];

	color[0] = surf->color[0] * 255;
	color[1] = surf->color[1] * 255;
	color[2] = surf->color[2] * 255;
	color[3] = 255;

	VectorClear (left);
	VectorClear (up);

	left[0] = r_ignore->value;

	up[1] = r_ignore->value;

	RB_AddQuadStampExt (surf->origin, left, up, color, 0, 0, 1, 1);
#endif
}

#endif


void RB_SurfaceDisplayList (srfDisplayList_t *surf)
{
	// all apropriate state must be set in RB_BeginSurface
	// this isn't implemented yet...
}


void RB_SurfaceSkip (void *surf)
{
}


void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES]) (void *) = {
	(void (*) (void *)) RB_SurfaceBad,			// SF_BAD, 
	(void (*) (void *)) RB_SurfaceSkip,			// SF_SKIP, 
	(void (*) (void *)) RB_SurfaceFace,			// SF_FACE,
	(void (*) (void *)) RB_SurfaceGrid,			// SF_GRID,
	(void (*) (void *)) RB_SurfaceTriangles,	// SF_TRIANGLES,
	(void (*) (void *)) RB_SurfacePolychain,	// SF_POLY,
	(void (*) (void *)) RB_SurfaceMesh,			// SF_MD3,
	(void (*) (void *)) RB_SurfaceAnim,			// SF_MD4,
	(void (*) (void *)) RB_SurfaceFlare,		// SF_FLARE,
	(void (*) (void *)) RB_SurfaceEntity,		// SF_ENTITY
	(void (*) (void *)) RB_SurfaceDisplayList	// SF_DISPLAY_LIST
};

