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
/*
** QGL.H
*/

#ifndef __QGL_H__
#define __QGL_H__

#include <windows.h>

#include <d3d9.h>

extern IDirect3D9 *d3d_Object;
extern IDirect3DDevice9 *d3d_Device;
extern D3DCAPS9 d3d_DeviceCaps;
extern IDirect3DQuery9 *d3d_Event;

void QGL_RegisterDirect3DResource (void *res);
void QGL_RemoveDirect3DResource (void **res);
qboolean QGL_InitDirect3D (HWND hWnd);
D3DPRESENT_PARAMETERS *QGL_SetPresentParameters (HWND hWnd, int width, int height, D3DFORMAT format, int refreshRate, qboolean fullscreen, qboolean vsync);
void QGL_ShutdownDirect3D (void);
qboolean QGL_CheckScene (void);

#ifndef __D3DX9TEX_H__
// define D3DX constants missing since we no longer have D3DX available
#define D3DX_FILTER_NONE             (1 << 0)
#define D3DX_FILTER_POINT            (2 << 0)
#define D3DX_FILTER_LINEAR           (3 << 0)
#define D3DX_FILTER_TRIANGLE         (4 << 0)
#define D3DX_FILTER_BOX              (5 << 0)

#define D3DX_FILTER_MIRROR_U         (1 << 16)
#define D3DX_FILTER_MIRROR_V         (2 << 16)
#define D3DX_FILTER_MIRROR_W         (4 << 16)
#define D3DX_FILTER_MIRROR           (7 << 16)

#define D3DX_FILTER_DITHER           (1 << 19)
#define D3DX_FILTER_DITHER_DIFFUSION (2 << 19)

#define D3DX_FILTER_SRGB_IN          (1 << 21)
#define D3DX_FILTER_SRGB_OUT         (2 << 21)
#define D3DX_FILTER_SRGB             (3 << 21)

typedef enum _D3DXIMAGE_FILEFORMAT
{
	D3DXIFF_BMP = 0,
	D3DXIFF_JPG = 1,
	D3DXIFF_TGA = 2,
	D3DXIFF_PNG = 3,
	D3DXIFF_DDS = 4,
	D3DXIFF_PPM = 5,
	D3DXIFF_DIB = 6,
	D3DXIFF_HDR = 7,
	D3DXIFF_PFM = 8,
	D3DXIFF_FORCE_DWORD = 0x7fffffff
} D3DXIMAGE_FILEFORMAT;

#define MAX_FVF_DECL_SIZE (MAXD3DDECLLENGTH + 1) // +1 for END

typedef HRESULT (WINAPI *D3DXLoadSurfaceFromMemoryProc) (LPDIRECT3DSURFACE9, CONST PALETTEENTRY *, CONST RECT *, LPCVOID, D3DFORMAT, UINT, CONST PALETTEENTRY *, CONST RECT *, DWORD, D3DCOLOR);
typedef HRESULT (WINAPI *D3DXSaveSurfaceToFileProc) (LPCSTR, D3DXIMAGE_FILEFORMAT, LPDIRECT3DSURFACE9, CONST PALETTEENTRY *, CONST RECT *);
typedef HRESULT (WINAPI *D3DXLoadSurfaceFromSurfaceProc) (LPDIRECT3DSURFACE9, CONST PALETTEENTRY *, CONST RECT *, LPDIRECT3DSURFACE9, CONST PALETTEENTRY *, CONST RECT *, DWORD, D3DCOLOR);
typedef HRESULT (WINAPI *D3DXDeclaratorFromFVFProc) (DWORD FVF, D3DVERTEXELEMENT9 pDeclarator[MAX_FVF_DECL_SIZE]);

extern D3DXLoadSurfaceFromMemoryProc D3DXLoadSurfaceFromMemory;
extern D3DXSaveSurfaceToFileProc D3DXSaveSurfaceToFile;
extern D3DXLoadSurfaceFromSurfaceProc D3DXLoadSurfaceFromSurface;
extern D3DXDeclaratorFromFVFProc D3DXDeclaratorFromFVF;
#endif

#endif
