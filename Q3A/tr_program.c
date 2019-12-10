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

// GPU shader program generation for tcgen, tcmod and stage blending operations
// cgen and agen are left on the CPU because the additional evaluated fog modulation can vary at runtime
#include "tr_local.h"
#include <D3Dcompiler.h>
#include <stddef.h>
#include "tr_program.h"
#include "resource.h"

// this ensures that all shaders use a consistent set of registers
static D3D_SHADER_MACRO staticDefines[] = {
	// definition 0 is reserved for "VERTEXSHADER" or "PIXELSHADER"
	{NULL, NULL},
	// vertex shader regs
	DEFINE_SHADER_REGISTER (VSREG_MVPMATRIX),
	DEFINE_SHADER_REGISTER (VSREG_PROJECTIONMATRIX),
	DEFINE_SHADER_REGISTER (VSREG_MODELVIEWMATRIX),
	DEFINE_SHADER_REGISTER (VSREG_SKYMATRIX),
	DEFINE_SHADER_REGISTER (VSREG_MOVEORIGIN),
	DEFINE_SHADER_REGISTER (VSREG_TMODTURBTIME),
	DEFINE_SHADER_REGISTER (VSREG_TCGENVEC0),
	DEFINE_SHADER_REGISTER (VSREG_TCGENVEC1),
	DEFINE_SHADER_REGISTER (VSREG_INVERSERT),
	// pixel shader regs
	DEFINE_SHADER_REGISTER (PSREG_TMODMATRIX0),
	DEFINE_SHADER_REGISTER (PSREG_TMODMATRIX1),
	DEFINE_SHADER_REGISTER (PSREG_TMODMATRIX2),
	DEFINE_SHADER_REGISTER (PSREG_TMODMATRIX3),
	DEFINE_SHADER_REGISTER (PSREG_TRANSLATE0),
	DEFINE_SHADER_REGISTER (PSREG_TRANSLATE1),
	DEFINE_SHADER_REGISTER (PSREG_TRANSLATE2),
	DEFINE_SHADER_REGISTER (PSREG_TRANSLATE3),
	DEFINE_SHADER_REGISTER (PSREG_TMODTURBAMP),
	DEFINE_SHADER_REGISTER (PSREG_FOGDISTANCE),
	DEFINE_SHADER_REGISTER (PSREG_FOGDEPTH),
	DEFINE_SHADER_REGISTER (PSREG_FOGEYET),
	DEFINE_SHADER_REGISTER (PSREG_FOGEYEOUTSIDE),
	DEFINE_SHADER_REGISTER (PSREG_VIEWORIGIN),
	DEFINE_SHADER_REGISTER (PSREG_DLRADIUS),
	DEFINE_SHADER_REGISTER (PSREG_DLORIGIN),
	DEFINE_SHADER_REGISTER (PSREG_DLCOLOUR),
	DEFINE_SHADER_REGISTER (PSREG_IDENTITYLIGHT),
	DEFINE_SHADER_REGISTER (PSREG_GAMMA),
	DEFINE_SHADER_REGISTER (PSREG_BRIGHTNESS),
	// we always need to close the static defines in case a generated shader adds no new defines
	{NULL, NULL}
};

// one less because we don't count the closed-off define
static const int numStaticDefines = (sizeof (staticDefines) / sizeof (staticDefines[0])) - 1;

#define MAX_SHADER_CACHE	1024

typedef struct shadercache_s
{
	IDirect3DVertexShader9 *vs;
	IDirect3DPixelShader9 *ps;
	int key;
} shadercache_t;

shadercache_t shaderCache[MAX_SHADER_CACHE];
int numShaderCache = 0;

// needed for the shader compiler
// pD3DCompile is defined in D3Dcompiler.h so we don't need to typedef it ourselves
HINSTANCE hInstCompiler = NULL;
pD3DCompile QD3DCompile = NULL;

// these are our built-in programs for simple drawing
program_t r_genericProgram;
program_t r_skyboxProgram;
program_t r_dlightProgram;

IDirect3DVertexDeclaration9 *r_stageDeclaration = NULL;
IDirect3DVertexBuffer9 *r_stageStaticVertexBuffer = NULL;
IDirect3DVertexBuffer9 *r_stageDynamicVertexBuffer = NULL;
IDirect3DIndexBuffer9 *r_stageIndexBuffer = NULL;

static ID3DBlob *R_CompileShaderCommon (const char *src, int len, const char *entrypoint, const D3D_SHADER_MACRO *defines, const char *profile)
{
	ID3DBlob *ShaderBlob = NULL;
	ID3DBlob *ErrorBlob = NULL;

	HRESULT hr = QD3DCompile (
		src,
		len,
		NULL,
		defines,
		NULL,
		entrypoint,
		profile,
		0,
		0,
		&ShaderBlob,
		&ErrorBlob);

	if (ErrorBlob)
	{
		ri.Printf (PRINT_WARNING, "Shader %s error:\n%s\n", entrypoint, (char *) ErrorBlob->lpVtbl->GetBufferPointer (ErrorBlob));
		ErrorBlob->lpVtbl->Release (ErrorBlob);
	}
	else if (FAILED (hr))
		ri.Printf (PRINT_WARNING, "Terminal shader %s error!!!\n", entrypoint);

	// return the shader blob we got (it can be NULL and the caller will check for it)
	return ShaderBlob;
}


IDirect3DVertexShader9 *R_CreateVertexShader (char *src, int len, D3D_SHADER_MACRO *defines)
{
	IDirect3DVertexShader9 *vs = NULL;
	ID3DBlob *ShaderBlob = NULL;

	// definition 0 is reserved for "VERTEXSHADER" or "PIXELSHADER"
	defines[0].Name = "VERTEXSHADER";
	defines[0].Definition = "1";

	if ((ShaderBlob = R_CompileShaderCommon (src, len, "VSMain", defines, "vs_3_0")) != NULL)
	{
		d3d_Device->lpVtbl->CreateVertexShader (d3d_Device, (DWORD *) ShaderBlob->lpVtbl->GetBufferPointer (ShaderBlob), &vs);
		ShaderBlob->lpVtbl->Release (ShaderBlob);
	}

	return vs;
}


IDirect3DPixelShader9 *R_CreatePixelShader (char *src, int len, D3D_SHADER_MACRO *defines)
{
	IDirect3DPixelShader9 *ps = NULL;
	ID3DBlob *ShaderBlob = NULL;

	// definition 0 is reserved for "VERTEXSHADER" or "PIXELSHADER"
	defines[0].Name = "PIXELSHADER";
	defines[0].Definition = "1";

	if ((ShaderBlob = R_CompileShaderCommon (src, len, "PSMain", defines, "ps_3_0")) != NULL)
	{
		d3d_Device->lpVtbl->CreatePixelShader (d3d_Device, (DWORD *) ShaderBlob->lpVtbl->GetBufferPointer (ShaderBlob), &ps);
		ShaderBlob->lpVtbl->Release (ShaderBlob);
	}

	return ps;
}


qboolean RB_CheckShaderCache (shaderStage_t *pStage, int stageKey)
{
	for (int i = 0; i < numShaderCache; i++)
	{
		// these should never happen
		if (!shaderCache[i].vs) continue;
		if (!shaderCache[i].ps) continue;

		// not a match
		if (shaderCache[i].key != stageKey) continue;

		// this was a match
		pStage->vertexShader = shaderCache[i].vs;
		pStage->pixelShader = shaderCache[i].ps;

		// ri.Printf (PRINT_ALL, "shader from cache: %i\n", stageKey);

		return qtrue;
	}

	// not matched at all
	return qfalse;
}


void RB_CreateShaderCache (shaderStage_t *pStage, int stageKey, char *source, int length, D3D_SHADER_MACRO *stageDefines)
{
	pStage->vertexShader = R_CreateVertexShader (source, length, stageDefines);
	pStage->pixelShader = R_CreatePixelShader (source, length, stageDefines);

	// add them to the cache
	if (numShaderCache < MAX_SHADER_CACHE)
	{
		shaderCache[numShaderCache].vs = pStage->vertexShader;
		shaderCache[numShaderCache].ps = pStage->pixelShader;
		shaderCache[numShaderCache].key = stageKey;
		numShaderCache++;
	}
	else ri.Error (ERR_FATAL, "numShaderCache < MAX_SHADER_CACHE");
}


int Sys_LoadResourceData (int resourceid, void **resbuf)
{
	// per MSDN, UnlockResource is obsolete and does nothing any more.  There is
	// no way to free the memory used by a resource after you're finished with it.
	// If you ask me this is kinda fucked, but what do I know?  We'll just leak it.
	if (resbuf)
	{
		HRSRC hResInfo = FindResource (NULL, MAKEINTRESOURCE (resourceid), RT_RCDATA);

		if (hResInfo)
		{
			HGLOBAL hResData = LoadResource (NULL, hResInfo);

			if (hResData)
			{
				resbuf[0] = (byte *) LockResource (hResData);
				return SizeofResource (NULL, hResInfo);
			}
		}
	}

	return 0;
}


void R_CreateBuiltinProgram (program_t *prog, int resourceID, DWORD FVF)
{
	char *src = NULL;
	int len = Sys_LoadResourceData (resourceID, (void **) &src);
	D3DVERTEXELEMENT9 elem[MAX_FVF_DECL_SIZE];

	// create a vertex declaration from the FVF
	D3DXDeclaratorFromFVF (FVF, elem);

	// and create the program
	d3d_Device->lpVtbl->CreateVertexDeclaration (d3d_Device, elem, &prog->vd);

	prog->vs = R_CreateVertexShader (src, len, staticDefines);
	prog->ps = R_CreatePixelShader (src, len, staticDefines);
}


void R_ProgramOnLostDevice (void)
{
	if (r_stageStaticVertexBuffer)
	{
		r_stageStaticVertexBuffer->lpVtbl->Release (r_stageStaticVertexBuffer);
		r_stageStaticVertexBuffer = NULL;
	}

	if (r_stageDynamicVertexBuffer)
	{
		r_stageDynamicVertexBuffer->lpVtbl->Release (r_stageDynamicVertexBuffer);
		r_stageDynamicVertexBuffer = NULL;
	}

	if (r_stageIndexBuffer)
	{
		r_stageIndexBuffer->lpVtbl->Release (r_stageIndexBuffer);
		r_stageIndexBuffer = NULL;
	}
}


void R_ProgramOnResetDevice (void)
{
	d3d_Device->lpVtbl->CreateVertexBuffer (
		d3d_Device,
		sizeof (stagestaticvert_t) * BUFFER_MAX_VERTEXES,
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		0,
		D3DPOOL_DEFAULT,
		&r_stageStaticVertexBuffer,
		NULL
	);

	d3d_Device->lpVtbl->CreateVertexBuffer (
		d3d_Device,
		sizeof (stagestaticvert_t) * BUFFER_MAX_VERTEXES,
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		0,
		D3DPOOL_DEFAULT,
		&r_stageDynamicVertexBuffer,
		NULL
	);

	d3d_Device->lpVtbl->CreateIndexBuffer (
		d3d_Device,
		sizeof (unsigned short) * BUFFER_MAX_INDEXES,
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&r_stageIndexBuffer,
		NULL
	);
}


void GL_InitPrograms (void)
{
	// statically linking to d3dcompiler.lib causes it to use d3dcompiler_47.dll which is not on Windows 7 so link dynamically instead
	if (!hInstCompiler)
	{
		for (int i = 99; i > 32; i--)
		{
			if ((hInstCompiler = LoadLibrary (va ("d3dcompiler_%i.dll", i))) != NULL)
			{
				if ((QD3DCompile = (pD3DCompile) GetProcAddress (hInstCompiler, "D3DCompile")) != NULL)
					break;
				else
				{
					FreeLibrary (hInstCompiler);
					hInstCompiler = NULL;
				}
			}
		}

		if (!hInstCompiler)
		{
			// totally failed to load
			ri.Error (ERR_FATAL, "Failed to load D3DCompile");
			return;
		}

		if (!QD3DCompile)
		{
			// this should never happen
			ri.Error (ERR_FATAL, "Failed to load D3DCompile");
			return;
		}

		// now create our builtin programs - for simplicity these will use FVF codes instead of full decls
		R_CreateBuiltinProgram (&r_genericProgram, IDR_GENERIC_HLSL, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
		R_CreateBuiltinProgram (&r_skyboxProgram, IDR_SKYBOX_HLSL, D3DFVF_XYZ | D3DFVF_TEX1);
		R_CreateBuiltinProgram (&r_dlightProgram, IDR_DLIGHT_HLSL, D3DFVF_XYZ);

		// this must match the layout of the vertex structs
		D3DVERTEXELEMENT9 stageLayout[] = {
			VDECL (0, offsetof (stagestaticvert_t, xyz), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
			VDECL (0, offsetof (stagestaticvert_t, st), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0),
			VDECL (0, offsetof (stagestaticvert_t, lm), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1),
			VDECL (0, offsetof (stagestaticvert_t, normal), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0),
			VDECL (1, offsetof (stagedynamicvert_t, color), D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0),
			D3DDECL_END ()
		};

		d3d_Device->lpVtbl->CreateVertexDeclaration (d3d_Device, stageLayout, &r_stageDeclaration);

		R_ProgramOnResetDevice ();
	}
}


void R_ShutdownProgram (program_t *prog)
{
	if (prog->vd)
	{
		prog->vd->lpVtbl->Release (prog->vd);
		prog->vd = NULL;
	}

	if (prog->vs)
	{
		prog->vs->lpVtbl->Release (prog->vs);
		prog->vs = NULL;
	}

	if (prog->ps)
	{
		prog->ps->lpVtbl->Release (prog->ps);
		prog->ps = NULL;
	}
}


void GL_ShutdownPrograms (void)
{
	for (int i = 0; i < numShaderCache; i++)
	{
		if (shaderCache[i].vs)
		{
			shaderCache[i].vs->lpVtbl->Release (shaderCache[i].vs);
			shaderCache[i].vs = NULL;
		}

		if (shaderCache[i].ps)
		{
			shaderCache[i].ps->lpVtbl->Release (shaderCache[i].ps);
			shaderCache[i].ps = NULL;
		}
	}

	memset (shaderCache, 0, sizeof (shaderCache));
	numShaderCache = 0;

	R_ProgramOnLostDevice ();

	if (r_stageDeclaration)
	{
		r_stageDeclaration->lpVtbl->Release (r_stageDeclaration);
		r_stageDeclaration = NULL;
	}

	R_ShutdownProgram (&r_genericProgram);
	R_ShutdownProgram (&r_skyboxProgram);
	R_ShutdownProgram (&r_dlightProgram);

	// any future calls into here are errors
	QD3DCompile = NULL;

	// and unload the library
	if (hInstCompiler)
	{
		FreeLibrary (hInstCompiler);
		hInstCompiler = NULL;
	}
}


static const char *tcGenDefines[] = {
	"TCGEN_BAD",
	"TCGEN_IDENTITY",
	"TCGEN_LIGHTMAP",
	"TCGEN_TEXTURE",
	"TCGEN_ENVIRONMENT_MAPPED",
	"TCGEN_FOG",
	"TCGEN_VECTOR"
};

static const char *tModDefines[4][8] = {
	// most tmods can use transform nowadays
	{"TMOD_NONE0", "TMOD_TRANSFORM0", "TMOD_TURB0", "TMOD_TRANSFORM0", "TMOD_TRANSFORM0", "TMOD_TRANSFORM0", "TMOD_TRANSFORM0", "TMOD_TRANSFORM0"},
	{"TMOD_NONE1", "TMOD_TRANSFORM1", "TMOD_TURB1", "TMOD_TRANSFORM1", "TMOD_TRANSFORM1", "TMOD_TRANSFORM1", "TMOD_TRANSFORM1", "TMOD_TRANSFORM1"},
	{"TMOD_NONE2", "TMOD_TRANSFORM2", "TMOD_TURB2", "TMOD_TRANSFORM2", "TMOD_TRANSFORM2", "TMOD_TRANSFORM2", "TMOD_TRANSFORM2", "TMOD_TRANSFORM2"},
	{"TMOD_NONE3", "TMOD_TRANSFORM3", "TMOD_TURB3", "TMOD_TRANSFORM3", "TMOD_TRANSFORM3", "TMOD_TRANSFORM3", "TMOD_TRANSFORM3", "TMOD_TRANSFORM3"}
};

static const char *skyDefinition = "TCGEN_SKY";
static const char *standardDefinition = "1";

void R_CreateHLSLProgramFromShader (shader_t *sh)
{
	// ensure that it's initialized
	GL_InitPrograms ();

	char *source = NULL;
	int length = Sys_LoadResourceData (IDR_SHADE_HLSL, (void **) &source);

	for (int i = 0; i < MAX_SHADER_STAGES; i++)
	{
		// create program sources for this stage
		shaderStage_t *pStage = sh->stages[i];

		if (!pStage) break;

		// set the TMU that will be used by this stage
		pStage->TMU = i;

		D3D_SHADER_MACRO stageDefines[128];	// fixme - make this big enough?
		int currStageDefine = numStaticDefines;

		// copy over the static defines
		memcpy (stageDefines, staticDefines, sizeof (staticDefines));

		// to help avoid unnecessary texture sets better, each stage uses it's own TMU; this way a lightmap
		// in stage 0 shared by many surfaces won't need to be replaced all the time
		char textureDefineValue[5];
		char samplerDefineValue[5];

		stageDefines[currStageDefine].Name = "TEXTURESTAGE";
		stageDefines[currStageDefine].Definition = textureDefineValue;
		sprintf (textureDefineValue, "t%i", pStage->TMU);
		currStageDefine++;

		stageDefines[currStageDefine].Name = "SAMPLERSTAGE";
		stageDefines[currStageDefine].Definition = samplerDefineValue;
		sprintf (samplerDefineValue, "s%i", pStage->TMU);
		currStageDefine++;

		if (sh->isSky)
			stageDefines[currStageDefine].Name = skyDefinition;
		else stageDefines[currStageDefine].Name = tcGenDefines[pStage->bundle.tcGen];

		stageDefines[currStageDefine].Definition = standardDefinition;
		currStageDefine++;

		// evaluate texcoord modification type
		for (int tm = 0; tm < pStage->bundle.numTexMods; tm++)
		{
			// a lot of these could move to the VS but if there is a turb in the middle it'll screw up as it can't linearly interpolate,
			// so we do them all in the fs anyway instead.
			switch (pStage->bundle.texMods[tm].type)
			{
			case TMOD_NONE:
				// break out of for loop
				tm = TR_MAX_TEXMODS;
				break;

			case TMOD_TRANSFORM:
			case TMOD_TURBULENT:
			case TMOD_SCROLL:
			case TMOD_SCALE:
			case TMOD_STRETCH:
			case TMOD_ROTATE:
			case TMOD_ENTITY_TRANSLATE:
				// store the define
				stageDefines[currStageDefine].Name = tModDefines[tm][pStage->bundle.texMods[tm].type];
				stageDefines[currStageDefine].Definition = standardDefinition;
				currStageDefine++;
				break;

			default:
				// no texmod
				break;
			}
		}

		// the last define is a NULL define
		stageDefines[currStageDefine].Name = NULL;
		stageDefines[currStageDefine].Definition = NULL;

		// build a unique key describing the shader ops we have on this stage
		int stageKey = (sh->isSky ? 1 : 0) |
			(pStage->bundle.texMods[3].type << 28) |
			(pStage->bundle.texMods[2].type << 24) |
			(pStage->bundle.texMods[1].type << 20) |
			(pStage->bundle.texMods[0].type << 16) |
			(pStage->bundle.tcGen << 12) |
			(pStage->rgbGen << 8) |
			(pStage->alphaGen << 4) |
			(pStage->TMU << 1);

		// and either take it from the cache or build it from scratch
		if (!RB_CheckShaderCache (pStage, stageKey))
		{
			// create the stage shaders
			RB_CreateShaderCache (pStage, stageKey, source, length, stageDefines);
		}
	}
}


void RB_SetVertexDeclaration (IDirect3DVertexDeclaration9 *vd)
{
	if (glState.currentvd != vd)
	{
		d3d_Device->lpVtbl->SetVertexDeclaration (d3d_Device, vd);
		glState.currentvd = vd;
	}
}


void RB_SetVertexShader (IDirect3DVertexShader9 *vs)
{
	if (glState.currentvs != vs)
	{
		d3d_Device->lpVtbl->SetVertexShader (d3d_Device, vs);
		glState.currentvs = vs;
	}
}


void RB_SetPixelShader (IDirect3DPixelShader9 *ps)
{
	if (glState.currentps != ps)
	{
		d3d_Device->lpVtbl->SetPixelShader (d3d_Device, ps);
		glState.currentps = ps;
	}
}


void RB_SetProgram (program_t *prog)
{
	RB_SetVertexDeclaration (prog->vd);
	RB_SetVertexShader (prog->vs);
	RB_SetPixelShader (prog->ps);
}


