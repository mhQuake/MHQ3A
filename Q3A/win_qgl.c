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

#include <float.h>
#include "tr_local.h"
#include "glw_win.h"
#include "tr_program.h"

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "dxguid.lib")


IDirect3D9 *d3d_Object = NULL;
IDirect3DDevice9 *d3d_Device = NULL;
D3DCAPS9 d3d_DeviceCaps;
D3DPRESENT_PARAMETERS d3d_PresentParams;
IDirect3DQuery9 *d3d_Event = NULL;

HINSTANCE hInstD3DX = NULL;

D3DXLoadSurfaceFromMemoryProc D3DXLoadSurfaceFromMemory = NULL;
D3DXSaveSurfaceToFileProc D3DXSaveSurfaceToFile = NULL;
D3DXLoadSurfaceFromSurfaceProc D3DXLoadSurfaceFromSurface = NULL;
D3DXDeclaratorFromFVFProc D3DXDeclaratorFromFVF = NULL;

#define MAX_D3D_RESOURCES	16384

typedef struct d3d_resource_s
{
	IDirect3DResource9 *Resource;
	D3DPOOL Pool;
} d3d_resource_t;

IDirect3DResource9 *d3d_Resources[MAX_D3D_RESOURCES];
int d3d_NumResources = 0;

// implementing a simple resource pool so that we don't have to deal with releasing objects all the time
void QGL_RegisterDirect3DResource (void *res)
{
	for (int i = 0; i < d3d_NumResources; i++)
	{
		if (!d3d_Resources[i])
		{
			d3d_Resources[i] = (IDirect3DResource9 *) res;
			return;
		}
	}

	if (d3d_NumResources < MAX_D3D_RESOURCES)
	{
		d3d_Resources[d3d_NumResources] = (IDirect3DResource9 *) res;
		d3d_NumResources++;
	}
	else ri.Error (ERR_FATAL, "MAX_D3D_RESOURCES exceeded");
}


void QGL_RemoveDirect3DResource (void **res)
{
	if (!res) return;
	if (!*res) return;

	for (int i = 0; i < d3d_NumResources; i++)
	{
		if (!d3d_Resources[i]) continue;

		if ((void *) d3d_Resources[i] == *res)
		{
			d3d_Resources[i]->lpVtbl->Release (d3d_Resources[i]);
			d3d_Resources[i] = NULL;
			*res = NULL;
			return;
		}
	}
}


// default present params for modes; selected items can be changed later when the actual mode is set
static const D3DPRESENT_PARAMETERS d3d_DefaultPPWindowed = {
	0,										// backbuffer width
	0,										// backbuffer height
	D3DFMT_UNKNOWN,							// backbuffer format
	1,										// backbuffer count
	D3DMULTISAMPLE_NONE,					// no multisampling
	0,										// no multisample quality
	D3DSWAPEFFECT_DISCARD,					// discard swap
	NULL,									// no window yet; will be filled in later
	TRUE,									// windowed mode
	TRUE,									// enable depth/stencil buffer
	D3DFMT_D24S8,							// depth/stencil format
	D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL,	// discard depth/stencil too
	0,										// no refresh rate for windowed modes
	D3DPRESENT_INTERVAL_DEFAULT				// can be modified later
};

static const D3DPRESENT_PARAMETERS d3d_DefaultPPFullScreen = {
	0,										// backbuffer width will be filled in later
	0,										// backbuffer height will be filled in later
	D3DFMT_UNKNOWN,							// backbuffer format will be filled in later
	1,										// backbuffer count
	D3DMULTISAMPLE_NONE,					// no multisampling
	0,										// no multisample quality
	D3DSWAPEFFECT_DISCARD,					// discard swap
	NULL,									// no window yet; will be filled in later
	FALSE,									// fullscreen mode
	TRUE,									// enable depth/stencil buffer
	D3DFMT_D24S8,							// depth/stencil format
	D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL,	// discard depth/stencil too
	0,										// refresh rate will be filled in later
	D3DPRESENT_INTERVAL_DEFAULT				// can be modified later
};


D3DPRESENT_PARAMETERS *QGL_GetPresentParameters (HWND hWnd, int width, int height, D3DFORMAT format, int refreshRate, qboolean fullscreen)
{
	static D3DPRESENT_PARAMETERS pp;

	if (fullscreen)
	{
		// fullscreen mode
		memcpy (&pp, &d3d_DefaultPPFullScreen, sizeof (D3DPRESENT_PARAMETERS));

		// and copy the mode to the present params
		pp.BackBufferWidth = width;
		pp.BackBufferHeight = height;
		pp.FullScreen_RefreshRateInHz = refreshRate;
		pp.BackBufferFormat = format;
	}
	else
	{
		// windowed mode
		memcpy (&pp, &d3d_DefaultPPWindowed, sizeof (D3DPRESENT_PARAMETERS));

		// and copy the mode to the present params
		pp.BackBufferWidth = width;
		pp.BackBufferHeight = height;
	}

	// update the window used
	pp.hDeviceWindow = hWnd;

	// update vsync flag
	if (r_swapInterval->value)
		pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	else pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	return &pp;
}


qboolean R_TryLoadD3DX (int version)
{
	// because we don't have d3dx any more we load it this way - yuck!!!
	if ((hInstD3DX = LoadLibrary (va ("d3dx9_%i.dll", version))) != NULL)
	{
		// now try to load them load them
		if ((D3DXLoadSurfaceFromMemory = (D3DXLoadSurfaceFromMemoryProc) GetProcAddress (hInstD3DX, "D3DXLoadSurfaceFromMemory")) == NULL) return qfalse;
		if ((D3DXSaveSurfaceToFile = (D3DXSaveSurfaceToFileProc) GetProcAddress (hInstD3DX, "D3DXSaveSurfaceToFileA")) == NULL) return qfalse;
		if ((D3DXLoadSurfaceFromSurface = (D3DXLoadSurfaceFromSurfaceProc) GetProcAddress (hInstD3DX, "D3DXLoadSurfaceFromSurface")) == NULL) return qfalse;
		if ((D3DXDeclaratorFromFVF = (D3DXDeclaratorFromFVFProc) GetProcAddress (hInstD3DX, "D3DXDeclaratorFromFVF")) == NULL) return qfalse;

		// loaded OK
		return qtrue;
	}
	else
	{
		// didn't load at all
		return qfalse;
	}
}


void R_UnloadD3DX (void)
{
	// clear the procs
	D3DXLoadSurfaceFromMemory = NULL;
	D3DXSaveSurfaceToFile = NULL;
	D3DXLoadSurfaceFromSurface = NULL;
	D3DXDeclaratorFromFVF = NULL;

	// and unload the library
	if (hInstD3DX)
	{
		FreeLibrary (hInstD3DX);
		hInstD3DX = NULL;
	}
}


qboolean QGL_InitDirect3D (HWND hWnd)
{
	// create the D3D object
	if ((d3d_Object = Direct3DCreate9 (D3D_SDK_VERSION)) == NULL)
	{
		ri.Error (ERR_FATAL, "R_InitVideo : Direct3DCreate9 failed");
		return qfalse;
	}

	RECT cr;

	GetClientRect (hWnd, &cr);

	int width = cr.right - cr.left;
	int height = cr.bottom - cr.top;

	// get our present params and save them out
	memcpy (&d3d_PresentParams, QGL_GetPresentParameters (hWnd, width, height, D3DFMT_UNKNOWN, 0, qfalse), sizeof (d3d_PresentParams));

	// create it for first time
	if (FAILED (d3d_Object->lpVtbl->CreateDevice (
		d3d_Object,
		0,
		D3DDEVTYPE_HAL,
		hWnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
		&d3d_PresentParams,
		&d3d_Device)))
	{
		ri.Error (ERR_FATAL, "R_InitVideo : CreateDevice failed");
		return qfalse;
	}

	// clear to black
	d3d_Device->lpVtbl->Clear (d3d_Device, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	d3d_Device->lpVtbl->Present (d3d_Device, NULL, NULL, NULL, NULL);

	if (FAILED (d3d_Device->lpVtbl->CreateQuery (d3d_Device, D3DQUERYTYPE_EVENT, &d3d_Event)))
	{
		ri.Error (ERR_FATAL, "R_InitVideo : CreateQuery with D3DQUERYTYPE_EVENT failed");
		return qfalse;
	}

	// get caps
	d3d_Device->lpVtbl->GetDeviceCaps (d3d_Device, &d3d_DeviceCaps);

	// get the supported shader models
	const int iVertexShaderModel = D3DSHADER_VERSION_MAJOR (d3d_DeviceCaps.VertexShaderVersion);
	const int iPixelShaderModel = D3DSHADER_VERSION_MAJOR (d3d_DeviceCaps.PixelShaderVersion);

	// we're only interested in supporting SM3+ hardware
	if (iVertexShaderModel < 3 || iPixelShaderModel < 3)
	{
		ri.Error (ERR_FATAL, "R_InitVideo : iVertexShaderModel < 3 || iPixelShaderModel < 3");
		return qfalse;
	}

	// ensure that D3DX is unloaded before we begin
	R_UnloadD3DX ();

	// starting at 99 to future-proof things a little, 23 and previous were in static libs and
	// there was no plain old "d3dx9.dll"
	for (int i = 99; i > 23; i--)
	{
		// try to load this version
		if (R_TryLoadD3DX (i)) break;

		// unload if it didn't
		R_UnloadD3DX ();
	}

	// the HINSTANCE for the library should be valid if it loaded or NULL if it didn't
	if (!hInstD3DX)
	{
		ri.Error (ERR_FATAL, "R_InitVideo : failed to load D3DX\nPlease update your installation of DirectX...");
		return qfalse;
	}

	// now ensure that we disable d3d lighting
	d3d_Device->lpVtbl->SetRenderState (d3d_Device, D3DRS_LIGHTING, FALSE);

	// init programs
	GL_InitPrograms ();

	return qtrue;
}


void QGL_ShutdownDirect3D (void)
{
	// unload D3DX
	R_UnloadD3DX ();

	// unload resources
	for (int i = 0; i < MAX_D3D_RESOURCES; i++)
	{
		if (d3d_Resources[i] != NULL)
		{
			d3d_Resources[i]->lpVtbl->Release (d3d_Resources[i]);
			d3d_Resources[i] = NULL;
		}
	}

	d3d_NumResources = 0;

	// unload programs
	GL_ShutdownPrograms ();

	// unload event
	if (d3d_Event != NULL)
	{
		d3d_Event->lpVtbl->Release (d3d_Event);
		d3d_Event = NULL;
	}

	// unload device
	if (d3d_Device != NULL)
	{
		d3d_Device->lpVtbl->Release (d3d_Device);
		d3d_Device = NULL;
	}

	// unload object
	if (d3d_Object != NULL)
	{
		d3d_Object->lpVtbl->Release (d3d_Object);
		d3d_Object = NULL;
	}
}


void QGL_HandleLostDevice (void)
{
	// see if the device can be recovered
	switch (d3d_Device->lpVtbl->TestCooperativeLevel (d3d_Device))
	{
	case D3D_OK:
		// the device is no longer lost
		R_ProgramOnResetDevice ();
		ri.Printf (PRINT_ALL, "Lost device recovery complete\n");
		glState.deviceLost = qfalse;
		return;

	case D3DERR_DEVICELOST:
		// the device cannot be recovered at this time - yield some CPU time
		Sleep (10);
		break;

	case D3DERR_DEVICENOTRESET:
		// the device is ready to be reset
		R_ProgramOnLostDevice ();

		// reset the device and sleep a little to give it a chance to finish
		if (FAILED (d3d_Device->lpVtbl->Reset (d3d_Device, &d3d_PresentParams)))
		{
			ri.Error (ERR_FATAL, "Device reset failed (did you forget to Release something?)");
			return;
		}

		// yield some CPU to give the reset a chance to settle down
		Sleep (10);
		break;

	case D3DERR_DRIVERINTERNALERROR:
	default:
		// something bad happened
		ri.Error (ERR_FATAL, "QGL_HandleLostDevice: D3DERR_DRIVERINTERNALERROR");
		break;
	}
}


void R_ResetDevice (void)
{
	// wait or everything to finish up
	RB_EmulateGLFinish ();

	// the device is ready to be reset
	R_ProgramOnLostDevice ();

	// fixme - the event needs to be recreated here too
	if (d3d_Event != NULL)
	{
		d3d_Event->lpVtbl->Release (d3d_Event);
		d3d_Event = NULL;
	}

	// update present params for cvars
	if (r_swapInterval->value)
		d3d_PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	else d3d_PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	// reset the device and sleep a little to give it a chance to finish
	if (FAILED (d3d_Device->lpVtbl->Reset (d3d_Device, &d3d_PresentParams)))
	{
		ri.Error (ERR_FATAL, "Device reset failed (did you forget to Release something?)");
		return;
	}

	// yield some CPU to give the reset a chance to settle down
	Sleep (10);

	// the device is no longer lost
	R_ProgramOnResetDevice ();

	// fixme - need a bunch of reusable code here...
	if (FAILED (d3d_Device->lpVtbl->CreateQuery (d3d_Device, D3DQUERYTYPE_EVENT, &d3d_Event)))
	{
		ri.Error (ERR_FATAL, "R_InitVideo : CreateQuery with D3DQUERYTYPE_EVENT failed");
		return;
	}

	// bring default state back
	GL_SetDefaultState ();

	// unmodify cvars
	r_swapInterval->modified = qfalse;
}


qboolean QGL_CheckScene (void)
{
	if (!tr.registered) return qfalse;

	if (glState.deviceLost)
	{
		// attempt device recovery
		QGL_HandleLostDevice ();

		// even if it was recovered we want to skip this frame to allow everything to settle down
		return qfalse;
	}

	// swapinterval stuff
	if (r_swapInterval->modified)
	{
		R_ResetDevice ();
		return qfalse;
	}

	// issue a beginscene call if we haven't had one yet
	if (!glState.inBeginScene)
	{
		// always clear the rendertarget every frame because the brightpass will otherwise accumulate what
		// was previously there if the previous frame wasn't fully overdrawn
		d3d_Device->lpVtbl->Clear (d3d_Device, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);

		if (SUCCEEDED (d3d_Device->lpVtbl->BeginScene (d3d_Device)))
			glState.inBeginScene = qtrue;
		else return qfalse;
	}

	// it's OK to draw now
	return qtrue;
}

