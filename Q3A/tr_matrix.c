/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_main.c

#include "tr_local.h"
#include "tr_program.h"

#include <xmmintrin.h>
#include <smmintrin.h>


glmatrix *R_IdentityMatrix (glmatrix *m)
{
	m->m4x4[0][0] = 1; m->m4x4[0][1] = 0; m->m4x4[0][2] = 0; m->m4x4[0][3] = 0;
	m->m4x4[1][0] = 0; m->m4x4[1][1] = 1; m->m4x4[1][2] = 0; m->m4x4[1][3] = 0;
	m->m4x4[2][0] = 0; m->m4x4[2][1] = 0; m->m4x4[2][2] = 1; m->m4x4[2][3] = 0;
	m->m4x4[3][0] = 0; m->m4x4[3][1] = 0; m->m4x4[3][2] = 0; m->m4x4[3][3] = 1;

	return m;
}


glmatrix *R_MultMatrix (glmatrix *out, glmatrix *m1, glmatrix *m2)
{
	// up to 4x the perf of raw C code
	__m128 mrow;

	__m128 m2c0 = _mm_loadu_ps (m2->m4x4[0]);
	__m128 m2c1 = _mm_loadu_ps (m2->m4x4[1]);
	__m128 m2c2 = _mm_loadu_ps (m2->m4x4[2]);
	__m128 m2c3 = _mm_loadu_ps (m2->m4x4[3]);

	_MM_TRANSPOSE4_PS (m2c0, m2c1, m2c2, m2c3);

	mrow = _mm_loadu_ps (m1->m4x4[0]);
	_mm_storeu_ps (out->m4x4[0], _mm_hadd_ps (_mm_hadd_ps (_mm_mul_ps (mrow, m2c0), _mm_mul_ps (mrow, m2c1)), _mm_hadd_ps (_mm_mul_ps (mrow, m2c2), _mm_mul_ps (mrow, m2c3))));

	mrow = _mm_loadu_ps (m1->m4x4[1]);
	_mm_storeu_ps (out->m4x4[1], _mm_hadd_ps (_mm_hadd_ps (_mm_mul_ps (mrow, m2c0), _mm_mul_ps (mrow, m2c1)), _mm_hadd_ps (_mm_mul_ps (mrow, m2c2), _mm_mul_ps (mrow, m2c3))));

	mrow = _mm_loadu_ps (m1->m4x4[2]);
	_mm_storeu_ps (out->m4x4[2], _mm_hadd_ps (_mm_hadd_ps (_mm_mul_ps (mrow, m2c0), _mm_mul_ps (mrow, m2c1)), _mm_hadd_ps (_mm_mul_ps (mrow, m2c2), _mm_mul_ps (mrow, m2c3))));

	mrow = _mm_loadu_ps (m1->m4x4[3]);
	_mm_storeu_ps (out->m4x4[3], _mm_hadd_ps (_mm_hadd_ps (_mm_mul_ps (mrow, m2c0), _mm_mul_ps (mrow, m2c1)), _mm_hadd_ps (_mm_mul_ps (mrow, m2c2), _mm_mul_ps (mrow, m2c3))));

	return out;
}


glmatrix *R_LoadMatrix (glmatrix *m, float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
{
	m->m4x4[0][0] = _11; m->m4x4[0][1] = _12; m->m4x4[0][2] = _13; m->m4x4[0][3] = _14;
	m->m4x4[1][0] = _21; m->m4x4[1][1] = _22; m->m4x4[1][2] = _23; m->m4x4[1][3] = _24;
	m->m4x4[2][0] = _31; m->m4x4[2][1] = _32; m->m4x4[2][2] = _33; m->m4x4[2][3] = _34;
	m->m4x4[3][0] = _41; m->m4x4[3][1] = _42; m->m4x4[3][2] = _43; m->m4x4[3][3] = _44;

	return m;
}


glmatrix *R_CopyMatrix (glmatrix *out, const glmatrix *in)
{
	return memcpy (out, in, sizeof (glmatrix));
}


glmatrix *R_FrustumMatrix (glmatrix *m, float fovx, float fovy, float znear)
{
	// http://www.terathon.com/gdc07_lengyel.pdf
	// since we're *not* actually projecting shadows on the far plane in Quake, this variant of the infinite projection is OK to use
	glmatrix m2 = {
		1.0f / tan ((fovx * M_PI) / 360.0),
		0,
		0,
		0,
		0,
		1.0f / tan ((fovy * M_PI) / 360.0),
		0,
		0,
		0,
		0,
		-1.0f,
		-1.0f,
		0,
		0,
		-znear,
		0
	};

	return R_MultMatrix (m, &m2, m);
}



glmatrix *R_OrthoMatrix (glmatrix *m, float left, float right, float bottom, float top, float zNear, float zFar)
{
	glmatrix m2 = {
		2 / (right - left),
		0,
		0,
		0,
		0,
		2 / (top - bottom),
		0,
		0,
		0,
		0,
		-2 / (zFar - zNear),
		0,
		-((right + left) / (right - left)),
		-((top + bottom) / (top - bottom)),
		-((zFar + zNear) / (zFar - zNear)),
		1
	};

	return R_MultMatrix (m, &m2, m);
}


glmatrix *R_TranslateMatrix (glmatrix *m, float x, float y, float z)
{
	m->m4x4[3][0] += x * m->m4x4[0][0] + y * m->m4x4[1][0] + z * m->m4x4[2][0];
	m->m4x4[3][1] += x * m->m4x4[0][1] + y * m->m4x4[1][1] + z * m->m4x4[2][1];
	m->m4x4[3][2] += x * m->m4x4[0][2] + y * m->m4x4[1][2] + z * m->m4x4[2][2];
	m->m4x4[3][3] += x * m->m4x4[0][3] + y * m->m4x4[1][3] + z * m->m4x4[2][3];

	return m;
}


glmatrix *R_ScaleMatrix (glmatrix *m, float x, float y, float z)
{
	Vector4Scalef (m->m4x4[0], m->m4x4[0], x);
	Vector4Scalef (m->m4x4[1], m->m4x4[1], y);
	Vector4Scalef (m->m4x4[2], m->m4x4[2], z);

	return m;
}


glmatrix *R_RotateMatrixAxis (glmatrix *m, float angle, float x, float y, float z)
{
	float xyz[3] = {x, y, z};

	Vector3Normalize (xyz);
	angle = DEG2RAD (angle);

	float sa = sin (angle);
	float ca = cos (angle);

	glmatrix m2 = {
		(1.0f - ca) * xyz[0] * xyz[0] + ca,
		(1.0f - ca) * xyz[1] * xyz[0] + sa * xyz[2],
		(1.0f - ca) * xyz[2] * xyz[0] - sa * xyz[1],
		0.0f,
		(1.0f - ca) * xyz[0] * xyz[1] - sa * xyz[2],
		(1.0f - ca) * xyz[1] * xyz[1] + ca,
		(1.0f - ca) * xyz[2] * xyz[1] + sa * xyz[0],
		0.0f,
		(1.0f - ca) * xyz[0] * xyz[2] + sa * xyz[1],
		(1.0f - ca) * xyz[1] * xyz[2] - sa * xyz[0],
		(1.0f - ca) * xyz[2] * xyz[2] + ca,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	return R_MultMatrix (m, &m2, m);
}


glmatrix *R_RotateMatrix (glmatrix *m, float p, float y, float r)
{
	float sr = sin (DEG2RAD (r));
	float sp = sin (DEG2RAD (p));
	float sy = sin (DEG2RAD (y));
	float cr = cos (DEG2RAD (r));
	float cp = cos (DEG2RAD (p));
	float cy = cos (DEG2RAD (y));

	glmatrix m2 = {
		(cp * cy),
		(cp * sy),
		-sp,
		0.0f,
		(cr * -sy) + (sr * sp * cy),
		(cr * cy) + (sr * sp * sy),
		(sr * cp),
		0.0f,
		(sr * sy) + (cr * sp * cy),
		(-sr * cy) + (cr * sp * sy),
		(cr * cp),
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	return R_MultMatrix (m, &m2, m);
}


glmatrix *R_CameraMatrix (glmatrix *m, const float *origin, const float *angles)
{
	float sr = sin (DEG2RAD (angles[2]));
	float sp = sin (DEG2RAD (angles[0]));
	float sy = sin (DEG2RAD (angles[1]));
	float cr = cos (DEG2RAD (angles[2]));
	float cp = cos (DEG2RAD (angles[0]));
	float cy = cos (DEG2RAD (angles[1]));

	float _11 = -((cr * -sy) + (sr * sp * cy));
	float _21 = -((cr * cy) + (sr * sp * sy));
	float _31 = -(sr * cp);

	float _12 = (sr * sy) + (cr * sp * cy);
	float _22 = (-sr * cy) + (cr * sp * sy);
	float _32 = (cr * cp);

	float _13 = -(cp * cy);
	float _23 = -(cp * sy);
	float _33 = sp;

	glmatrix m2 = {
		_11, _12, _13, 0.0f,
		_21, _22, _23, 0.0f,
		_31, _32, _33, 0.0f,
		-origin[0] * _11 - origin[1] * _21 - origin[2] * _31,
		-origin[0] * _12 - origin[1] * _22 - origin[2] * _32,
		-origin[0] * _13 - origin[1] * _23 - origin[2] * _33,
		1.0f
	};

	return R_MultMatrix (m, &m2, m);
}


void R_InverseTransform (glmatrix *m, float *out, const float *in)
{
	// http://content.gpwiki.org/index.php/MathGem:Fast_Matrix_Inversion
	out[0] = in[0] * m->m4x4[0][0] + in[1] * m->m4x4[0][1] + in[2] * m->m4x4[0][2] - Vector3Dot (m->m4x4[0], m->m4x4[3]);
	out[1] = in[0] * m->m4x4[1][0] + in[1] * m->m4x4[1][1] + in[2] * m->m4x4[1][2] - Vector3Dot (m->m4x4[1], m->m4x4[3]);
	out[2] = in[0] * m->m4x4[2][0] + in[1] * m->m4x4[2][1] + in[2] * m->m4x4[2][2] - Vector3Dot (m->m4x4[2], m->m4x4[3]);
}


void R_Transform (glmatrix *m, float *out, const float *in)
{
	out[0] = in[0] * m->m4x4[0][0] + in[1] * m->m4x4[1][0] + in[2] * m->m4x4[2][0] + m->m4x4[3][0];
	out[1] = in[0] * m->m4x4[0][1] + in[1] * m->m4x4[1][1] + in[2] * m->m4x4[2][1] + m->m4x4[3][1];
	out[2] = in[0] * m->m4x4[0][2] + in[1] * m->m4x4[1][2] + in[2] * m->m4x4[2][2] + m->m4x4[3][2];
}


void R_Transform4x4 (glmatrix *m, float *out, const float *in)
{
	out[0] = in[0] * m->m4x4[0][0] + in[1] * m->m4x4[1][0] + in[2] * m->m4x4[2][0] + in[3] * m->m4x4[3][0];
	out[1] = in[0] * m->m4x4[0][1] + in[1] * m->m4x4[1][1] + in[2] * m->m4x4[2][1] + in[3] * m->m4x4[3][1];
	out[2] = in[0] * m->m4x4[0][2] + in[1] * m->m4x4[1][2] + in[2] * m->m4x4[2][2] + in[3] * m->m4x4[3][2];
	out[3] = in[0] * m->m4x4[0][3] + in[1] * m->m4x4[1][3] + in[2] * m->m4x4[2][3] + in[3] * m->m4x4[3][3];
}


static glmatrix r_mvp_matrix[3];

static void RB_UpdateMVPMatrix (void)
{
	// derive the MVP
	R_MultMatrix (&r_mvp_matrix[0], &r_mvp_matrix[2], &r_mvp_matrix[1]);

	// and send them all to our shaders
	// (this assumes that the layout is consistent and matches what we've used here, which it will be
	// so long as we use the VSREG_ defines everywhere and don't change them
	d3d_Device->lpVtbl->SetVertexShaderConstantF (d3d_Device, VSREG_MVPMATRIX, (float *) r_mvp_matrix, 12);
}


void RB_UpdateProjectionMatrix (glmatrix *projection)
{
	// copy it off
	R_CopyMatrix (&r_mvp_matrix[1], projection);
	RB_UpdateMVPMatrix ();
}


void RB_UpdateModelviewMatrix (glmatrix *modelview)
{
	// copy it off
	R_CopyMatrix (&r_mvp_matrix[2], modelview);
	RB_UpdateMVPMatrix ();
}


