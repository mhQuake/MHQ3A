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
** WIN_GLIMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_LogComment
** GLimp_Shutdown
**
** Note that the GLW_xxx functions are Windows specific GL-subsystem
** related functions that are relevant ONLY to win_glimp.c
*/
#include <assert.h>
#include "tr_local.h"
#include "qcommon.h"
#include "resource.h"
#include "glw_win.h"
#include "win_local.h"

LONG WINAPI QChangeDisplaySettings (DEVMODEA* lpDevMode, DWORD dwFlags)
{
	// this stub replaces all ChangeDisplaySettings calls in the engine during porting so that we don't inadvertently get a lost device
	return DISP_CHANGE_SUCCESSFUL;
}


extern void WG_CheckHardwareGamma (void);
extern void WG_RestoreGamma (void);

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

#define TRY_PFD_SUCCESS		0
#define TRY_PFD_FAIL_SOFT	1
#define TRY_PFD_FAIL_HARD	2

#define	WINDOW_CLASS_NAME	"MHQ3A"

static void		GLW_InitExtensions (void);
static rserr_t	GLW_SetMode (const char *drivername,
	int mode,
	int colorbits,
	qboolean cdsFullscreen);

static qboolean s_classRegistered = qfalse;


//
// variable declarations
//
glwstate_t glw_state;

cvar_t	*r_allowSoftwareGL;		// don't abort out if the pixelformat claims software
cvar_t	*r_maskMinidriver;		// allow a different dll name to be treated as if it were opengl32.dll



/*
** GLW_StartDriverAndSetMode
*/
static qboolean GLW_StartDriverAndSetMode (const char *drivername,
	int mode,
	int colorbits,
	qboolean cdsFullscreen)
{
	rserr_t err;

	err = GLW_SetMode (drivername, r_mode->integer, colorbits, cdsFullscreen);

	switch (err)
	{
	case RSERR_INVALID_FULLSCREEN:
		ri.Printf (PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
		return qfalse;
	case RSERR_INVALID_MODE:
		ri.Printf (PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
		return qfalse;
	default:
		break;
	}
	return qtrue;
}


/*
** GLW_InitDriver
**
** - get a DC if one doesn't exist
** - create an HGLRC if one doesn't exist
*/
static qboolean GLW_InitDriver (const char *drivername, int colorbits)
{
	QGL_InitDirect3D (g_wv.hWnd);

	glConfig.colorBits = 32;
	glConfig.depthBits = 24;
	glConfig.stencilBits = 8;

	return qtrue;
}

/*
** GLW_CreateWindow
**
** Responsible for creating the Win32 window and initializing the OpenGL driver.
*/
#define	WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)

static qboolean GLW_CreateWindow (const char *drivername, int width, int height, int colorbits, qboolean cdsFullscreen)
{
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	// register the window class if necessary
	if (!s_classRegistered)
	{
		WNDCLASS wc;

		memset (&wc, 0, sizeof (wc));

		wc.style = 0;
		wc.lpfnWndProc = (WNDPROC) glw_state.wndproc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = g_wv.hInstance;
		wc.hIcon = LoadIcon (g_wv.hInstance, MAKEINTRESOURCE (IDI_ICON1));
		wc.hCursor = LoadCursor (NULL, IDC_ARROW);
		wc.hbrBackground = (void *) COLOR_GRAYTEXT;
		wc.lpszMenuName = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME;

		if (!RegisterClass (&wc))
		{
			ri.Error (ERR_FATAL, "GLW_CreateWindow: could not register window class");
		}

		s_classRegistered = qtrue;
		ri.Printf (PRINT_ALL, "...registered window class\n");
	}

	// create the HWND if one does not already exist
	if (!g_wv.hWnd)
	{
		// compute width and height
		r.left = 0;
		r.top = 0;
		r.right = width;
		r.bottom = height;

		exstyle = 0;
		stylebits = WINDOW_STYLE | WS_SYSMENU;
		AdjustWindowRect (&r, stylebits, FALSE);

		w = r.right - r.left;
		h = r.bottom - r.top;

		vid_xpos = ri.Cvar_Get ("vid_xpos", "", 0);
		vid_ypos = ri.Cvar_Get ("vid_ypos", "", 0);
		x = vid_xpos->integer;
		y = vid_ypos->integer;

		// adjust window coordinates if necessary 
		// so that the window is completely on screen
		if (x < 0) x = 0;
		if (y < 0) y = 0;

		if (w < glw_state.desktopWidth && h < glw_state.desktopHeight)
		{
			if (x + w > glw_state.desktopWidth) x = (glw_state.desktopWidth - w);
			if (y + h > glw_state.desktopHeight) y = (glw_state.desktopHeight - h);
		}

		g_wv.hWnd = CreateWindowEx (
			exstyle,
			WINDOW_CLASS_NAME,
			"MHQ3A",
			stylebits,
			x, y, w, h,
			NULL,
			NULL,
			g_wv.hInstance,
			NULL);

		if (!g_wv.hWnd)
		{
			ri.Error (ERR_FATAL, "GLW_CreateWindow() - Couldn't create window");
		}

		ShowWindow (g_wv.hWnd, SW_SHOW);
		UpdateWindow (g_wv.hWnd);
		ri.Printf (PRINT_ALL, "...created window@%d,%d (%dx%d)\n", x, y, w, h);
	}
	else
	{
		ri.Printf (PRINT_ALL, "...window already present, CreateWindowEx skipped\n");
	}

	if (!GLW_InitDriver (drivername, colorbits))
	{
		ShowWindow (g_wv.hWnd, SW_HIDE);
		DestroyWindow (g_wv.hWnd);
		g_wv.hWnd = NULL;

		return qfalse;
	}

	SetForegroundWindow (g_wv.hWnd);
	SetFocus (g_wv.hWnd);

	return qtrue;
}


static void PrintCDSError (int value)
{
	switch (value)
	{
	case DISP_CHANGE_RESTART:
		ri.Printf (PRINT_ALL, "restart required\n");
		break;
	case DISP_CHANGE_BADPARAM:
		ri.Printf (PRINT_ALL, "bad param\n");
		break;
	case DISP_CHANGE_BADFLAGS:
		ri.Printf (PRINT_ALL, "bad flags\n");
		break;
	case DISP_CHANGE_FAILED:
		ri.Printf (PRINT_ALL, "DISP_CHANGE_FAILED\n");
		break;
	case DISP_CHANGE_BADMODE:
		ri.Printf (PRINT_ALL, "bad mode\n");
		break;
	case DISP_CHANGE_NOTUPDATED:
		ri.Printf (PRINT_ALL, "not updated\n");
		break;
	default:
		ri.Printf (PRINT_ALL, "unknown error %d\n", value);
		break;
	}
}

/*
** GLW_SetMode
*/
static rserr_t GLW_SetMode (const char *drivername,
	int mode,
	int colorbits,
	qboolean cdsFullscreen)
{
	HDC hDC;
	const char *win_fs[] = {"W", "FS"};
	int		cdsRet;
	DEVMODE dm;

	// print out informational messages
	ri.Printf (PRINT_ALL, "...setting mode %d:", mode);
	if (!R_GetModeInfo (&glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode))
	{
		ri.Printf (PRINT_ALL, " invalid mode\n");
		return RSERR_INVALID_MODE;
	}
	ri.Printf (PRINT_ALL, " %d %d %s\n", glConfig.vidWidth, glConfig.vidHeight, win_fs[cdsFullscreen]);

	// check our desktop attributes
	hDC = GetDC (GetDesktopWindow ());
	glw_state.desktopBitsPixel = GetDeviceCaps (hDC, BITSPIXEL);
	glw_state.desktopWidth = GetDeviceCaps (hDC, HORZRES);
	glw_state.desktopHeight = GetDeviceCaps (hDC, VERTRES);
	ReleaseDC (GetDesktopWindow (), hDC);

	// do a CDS if needed
	if (cdsFullscreen)
	{
		memset (&dm, 0, sizeof (dm));

		dm.dmSize = sizeof (dm);

		dm.dmPelsWidth = glConfig.vidWidth;
		dm.dmPelsHeight = glConfig.vidHeight;
		dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

		if (r_displayRefresh->integer != 0)
		{
			dm.dmDisplayFrequency = r_displayRefresh->integer;
			dm.dmFields |= DM_DISPLAYFREQUENCY;
		}

		// try to change color depth if possible
		if (colorbits != 0)
		{
			if (glw_state.allowdisplaydepthchange)
			{
				dm.dmBitsPerPel = colorbits;
				dm.dmFields |= DM_BITSPERPEL;
				ri.Printf (PRINT_ALL, "...using colorsbits of %d\n", colorbits);
			}
			else
			{
				ri.Printf (PRINT_ALL, "WARNING:...changing depth not supported on Win95 < pre-OSR 2.x\n");
			}
		}
		else
		{
			ri.Printf (PRINT_ALL, "...using desktop display depth of %d\n", glw_state.desktopBitsPixel);
		}

		// if we're already in fullscreen then just create the window
		if (glw_state.cdsFullscreen)
		{
			ri.Printf (PRINT_ALL, "...already fullscreen, avoiding redundant CDS\n");

			if (!GLW_CreateWindow (drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue))
			{
				ri.Printf (PRINT_ALL, "...restoring display settings\n");
				QChangeDisplaySettings (0, 0);
				return RSERR_INVALID_MODE;
			}
		}
		else
		{
			// need to call CDS
			ri.Printf (PRINT_ALL, "...calling CDS: ");

			// try setting the exact mode requested, because some drivers don't report
			// the low res modes in EnumDisplaySettings, but still work
			if ((cdsRet = QChangeDisplaySettings (&dm, CDS_FULLSCREEN)) == DISP_CHANGE_SUCCESSFUL)
			{
				ri.Printf (PRINT_ALL, "ok\n");

				if (!GLW_CreateWindow (drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue))
				{
					ri.Printf (PRINT_ALL, "...restoring display settings\n");
					QChangeDisplaySettings (0, 0);
					return RSERR_INVALID_MODE;
				}

				glw_state.cdsFullscreen = qtrue;
			}
			else
			{
				// the exact mode failed, so scan EnumDisplaySettings for the next largest mode
				DEVMODE		devmode;
				int			modeNum;

				ri.Printf (PRINT_ALL, "failed, ");

				PrintCDSError (cdsRet);

				ri.Printf (PRINT_ALL, "...trying next higher resolution:");

				// we could do a better matching job here...
				for (modeNum = 0;; modeNum++)
				{
					if (!EnumDisplaySettings (NULL, modeNum, &devmode))
					{
						modeNum = -1;
						break;
					}
					if (devmode.dmPelsWidth >= glConfig.vidWidth
						&& devmode.dmPelsHeight >= glConfig.vidHeight
						&& devmode.dmBitsPerPel >= 15)
					{
						break;
					}
				}

				if (modeNum != -1 && (cdsRet = QChangeDisplaySettings (&devmode, CDS_FULLSCREEN)) == DISP_CHANGE_SUCCESSFUL)
				{
					ri.Printf (PRINT_ALL, " ok\n");
					if (!GLW_CreateWindow (drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue))
					{
						ri.Printf (PRINT_ALL, "...restoring display settings\n");
						QChangeDisplaySettings (0, 0);
						return RSERR_INVALID_MODE;
					}

					glw_state.cdsFullscreen = qtrue;
				}
				else
				{
					ri.Printf (PRINT_ALL, " failed, ");

					PrintCDSError (cdsRet);

					ri.Printf (PRINT_ALL, "...restoring display settings\n");
					QChangeDisplaySettings (0, 0);

					glw_state.cdsFullscreen = qfalse;
					glConfig.isFullscreen = qfalse;
					if (!GLW_CreateWindow (drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qfalse))
					{
						return RSERR_INVALID_MODE;
					}
					return RSERR_INVALID_FULLSCREEN;
				}
			}
		}
	}
	else
	{
		if (glw_state.cdsFullscreen)
		{
			QChangeDisplaySettings (0, 0);
		}

		glw_state.cdsFullscreen = qfalse;
		if (!GLW_CreateWindow (drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, qfalse))
		{
			return RSERR_INVALID_MODE;
		}
	}

	// success, now check display frequency, although this won't be valid on Voodoo(2)
	memset (&dm, 0, sizeof (dm));
	dm.dmSize = sizeof (dm);
	if (EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &dm))
	{
		glConfig.displayFrequency = dm.dmDisplayFrequency;
	}

	// NOTE: this is overridden later on standalone 3Dfx drivers
	glConfig.isFullscreen = cdsFullscreen;

	return RSERR_OK;
}

/*
** GLW_InitExtensions
*/
static void GLW_InitExtensions (void)
{
	ri.Printf (PRINT_ALL, "Initializing OpenGL extensions\n");
	glConfig.textureCompression = TC_NONE;
	glConfig.textureEnvAddAvailable = qfalse;
}

/*
** GLW_CheckOSVersion
*/
static qboolean GLW_CheckOSVersion (void)
{
	glw_state.allowdisplaydepthchange = qtrue;
	return qtrue;
}


/*
** GLW_LoadOpenGL
**
** GLimp_win.c internal function that attempts to load and use
** a specific OpenGL DLL.
*/
static qboolean GLW_LoadOpenGL (const char *drivername)
{
	char buffer[1024];
	qboolean cdsFullscreen;

	Q_strncpyz (buffer, drivername, sizeof (buffer));
	Q_strlwr (buffer);

	// determine if we're on a standalone driver
	glConfig.driverType = GLDRV_ICD;

	// load the driver and bind our function pointers to it
	cdsFullscreen = r_fullscreen->integer;

	// create the window and set up the context
	if (!GLW_StartDriverAndSetMode (drivername, r_mode->integer, r_colorbits->integer, cdsFullscreen))
	{
		// if we're on a 24/32-bit desktop and we're going fullscreen on an ICD,
		// try it again but with a 16-bit desktop
		if (glConfig.driverType == GLDRV_ICD)
		{
			if (r_colorbits->integer != 16 || cdsFullscreen != qtrue || r_mode->integer != 3)
			{
				if (!GLW_StartDriverAndSetMode (drivername, 3, 16, qtrue))
				{
					goto fail;
				}
			}
		}
		else goto fail;
	}

	return qtrue;

fail:;
	return qfalse;
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame (void)
{
	// don't flip if drawing to front buffer
	if (glState.inBeginScene)
	{
		d3d_Device->lpVtbl->EndScene (d3d_Device);
		glState.inBeginScene = qfalse;

		if (d3d_Device->lpVtbl->Present (d3d_Device, NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST)
			glState.deviceLost = qtrue;
	}
}


static void GLW_StartOpenGL (void)
{
	qboolean attemptedOpenGL32 = qfalse;
	qboolean attempted3Dfx = qfalse;

	// load and initialize the specific OpenGL driver
	if (!GLW_LoadOpenGL (r_glDriver->string))
	{
		if (!Q_stricmp (r_glDriver->string, OPENGL_DRIVER_NAME))
		{
			attemptedOpenGL32 = qtrue;
		}
		else if (!Q_stricmp (r_glDriver->string, _3DFX_DRIVER_NAME))
		{
			attempted3Dfx = qtrue;
		}

		if (!attempted3Dfx)
		{
			attempted3Dfx = qtrue;
			if (GLW_LoadOpenGL (_3DFX_DRIVER_NAME))
			{
				ri.Cvar_Set ("r_glDriver", _3DFX_DRIVER_NAME);
				r_glDriver->modified = qfalse;
			}
			else
			{
				if (!attemptedOpenGL32)
				{
					if (!GLW_LoadOpenGL (OPENGL_DRIVER_NAME))
					{
						ri.Error (ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n");
					}
					ri.Cvar_Set ("r_glDriver", OPENGL_DRIVER_NAME);
					r_glDriver->modified = qfalse;
				}
				else
				{
					ri.Error (ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n");
				}
			}
		}
		else if (!attemptedOpenGL32)
		{
			attemptedOpenGL32 = qtrue;
			if (GLW_LoadOpenGL (OPENGL_DRIVER_NAME))
			{
				ri.Cvar_Set ("r_glDriver", OPENGL_DRIVER_NAME);
				r_glDriver->modified = qfalse;
			}
			else
			{
				ri.Error (ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n");
			}
		}
	}
}


/*
** GLimp_Init
**
** This is the platform specific OpenGL initialization function.  It
** is responsible for loading OpenGL, initializing it, setting
** extensions, creating a window of the appropriate size, doing
** fullscreen manipulations, etc.  Its overall responsibility is
** to make sure that a functional OpenGL subsystem is operating
** when it returns to the ref.
*/
void GLimp_Init (void)
{
	char	buf[1024];
	cvar_t *lastValidRenderer = ri.Cvar_Get ("r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE);
	cvar_t	*cv;

	ri.Printf (PRINT_ALL, "Initializing OpenGL subsystem\n");

	// check OS version to see if we can do fullscreen display changes
	if (!GLW_CheckOSVersion ())
	{
		ri.Error (ERR_FATAL, "GLimp_Init() - incorrect operating system\n");
	}

	// save off hInstance and wndproc
	cv = ri.Cvar_Get ("win_hinstance", "", 0);
	sscanf (cv->string, "%i", (int *) &g_wv.hInstance);

	cv = ri.Cvar_Get ("win_wndproc", "", 0);
	sscanf (cv->string, "%i", (int *) &glw_state.wndproc);

	r_allowSoftwareGL = ri.Cvar_Get ("r_allowSoftwareGL", "0", CVAR_LATCH);
	r_maskMinidriver = ri.Cvar_Get ("r_maskMinidriver", "0", CVAR_LATCH);

	// load appropriate DLL and initialize subsystem
	GLW_StartOpenGL ();

#ifdef GL_REMOVED
	// get our config strings
	Q_strncpyz (glConfig.vendor_string, glGetString (GL_VENDOR), sizeof (glConfig.vendor_string));
	Q_strncpyz (glConfig.renderer_string, glGetString (GL_RENDERER), sizeof (glConfig.renderer_string));
	Q_strncpyz (glConfig.version_string, glGetString (GL_VERSION), sizeof (glConfig.version_string));
	Q_strncpyz (glConfig.extensions_string, glGetString (GL_EXTENSIONS), sizeof (glConfig.extensions_string));
#endif

	// chipset specific configuration
	Q_strncpyz (buf, glConfig.renderer_string, sizeof (buf));
	Q_strlwr (buf);

	// NOTE: if changing cvars, do it within this block.  This allows them
	// to be overridden when testing driver fixes, etc. but only sets
	// them to their default state when the hardware is first installed/run.
	if (Q_stricmp (lastValidRenderer->string, glConfig.renderer_string))
	{
		glConfig.hardwareType = GLHW_GENERIC;
		ri.Cvar_Set ("r_textureMode", "GL_LINEAR_MIPMAP_LINEAR");
	}

	ri.Cvar_Set ("r_lastValidRenderer", glConfig.renderer_string);

	GLW_InitExtensions ();
	WG_CheckHardwareGamma ();
}


/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.
*/
void GLimp_Shutdown (void)
{
	QGL_ShutdownDirect3D ();

	// destroy window
	if (g_wv.hWnd)
	{
		ri.Printf (PRINT_ALL, "...destroying window\n");
		ShowWindow (g_wv.hWnd, SW_HIDE);
		DestroyWindow (g_wv.hWnd);
		g_wv.hWnd = NULL;
	}

	// close the r_logFile
	if (glw_state.log_fp)
	{
		fclose (glw_state.log_fp);
		glw_state.log_fp = 0;
	}

	// reset display settings
	if (glw_state.cdsFullscreen)
	{
		ri.Printf (PRINT_ALL, "...resetting display\n");
		QChangeDisplaySettings (0, 0);
		glw_state.cdsFullscreen = qfalse;
	}

	memset (&glConfig, 0, sizeof (glConfig));
	memset (&glState, 0, sizeof (glState));
}

/*
** GLimp_LogComment
*/
void GLimp_LogComment (char *comment)
{
	if (glw_state.log_fp)
	{
		fprintf (glw_state.log_fp, "%s", comment);
	}
}


