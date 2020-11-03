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
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "tr_program.h"

glconfig_t	glConfig;
glstate_t	glState;

static void GfxInfo_f (void);

cvar_t	*r_railWidth;
cvar_t	*r_railCoreWidth;
cvar_t	*r_railSegmentLength;

cvar_t	*r_ignoreFastPath;

cvar_t	*r_verbose;
cvar_t	*r_ignore;

cvar_t	*r_displayRefresh;

cvar_t	*r_detailTextures;

cvar_t	*r_znear;

cvar_t	*r_showSmp;
cvar_t	*r_skipBackEnd;

cvar_t	*r_ignorehwgamma;

cvar_t	*r_inGameVideo;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
cvar_t	*r_dlightBacks;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_facePlaneCull;
cvar_t	*r_showcluster;
cvar_t	*r_nocurves;

cvar_t	*r_allowExtensions;

cvar_t	*r_ext_compressed_textures;
cvar_t	*r_ext_gamma_control;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;

cvar_t	*r_ignoreGLErrors;
cvar_t	*r_logFile;

cvar_t	*r_stencilbits;
cvar_t	*r_depthbits;
cvar_t	*r_colorbits;
cvar_t	*r_stereo;
cvar_t	*r_primitives;
cvar_t	*r_texturebits;

cvar_t  *r_glDriver;
cvar_t	*r_lightmap;
cvar_t	*r_vertexLight;
cvar_t	*r_uiFullScreen;
cvar_t	*r_shadows;
cvar_t	*r_mode;
cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_swapInterval;
cvar_t	*r_textureMode;
cvar_t	*r_depthBiasFactor;
cvar_t	*r_gamma;
cvar_t	*r_brightness;
cvar_t	*r_desaturate_lightmaps;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_fullscreen;

cvar_t	*r_customwidth;
cvar_t	*r_customheight;
cvar_t	*r_customaspect;

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;
cvar_t	*r_saveFontData;


static void AssertCvarRange (cvar_t *cv, float minVal, float maxVal, qboolean shouldBeIntegral)
{
	if (shouldBeIntegral)
	{
		if ((int) cv->value != cv->integer)
		{
			ri.Printf (PRINT_WARNING, "WARNING: cvar '%s' must be integral (%f)\n", cv->name, cv->value);
			ri.Cvar_Set (cv->name, va ("%d", cv->integer));
		}
	}

	if (cv->value < minVal)
	{
		ri.Printf (PRINT_WARNING, "WARNING: cvar '%s' out of range (%f < %f)\n", cv->name, cv->value, minVal);
		ri.Cvar_Set (cv->name, va ("%f", minVal));
	}
	else if (cv->value > maxVal)
	{
		ri.Printf (PRINT_WARNING, "WARNING: cvar '%s' out of range (%f > %f)\n", cv->name, cv->value, maxVal);
		ri.Cvar_Set (cv->name, va ("%f", maxVal));
	}
}


/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL (void)
{
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_glDriver
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma

	if (glConfig.vidWidth == 0)
	{
		GLimp_Init ();

		if (d3d_DeviceCaps.MaxTextureWidth > d3d_DeviceCaps.MaxTextureHeight)
			glConfig.maxTextureSize = d3d_DeviceCaps.MaxTextureHeight;
		else glConfig.maxTextureSize = d3d_DeviceCaps.MaxTextureWidth;

		glConfig.maxActiveTextures = d3d_DeviceCaps.MaxTextureBlendStages;

		if (glConfig.maxActiveTextures < 8)
		{
			ri.Error (ERR_FATAL, "InitOpenGL : glConfig.maxActiveTextures < 8");
			return;
		}
	}

	// print info
	GfxInfo_f ();

	// set default state
	GL_SetDefaultState ();
}


/*
** R_GetModeInfo
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

vidmode_t r_vidModes[] =
{
	{"Mode  0: 320x240", 320, 240, 1},
	{"Mode  1: 400x300", 400, 300, 1},
	{"Mode  2: 512x384", 512, 384, 1},
	{"Mode  3: 640x480", 640, 480, 1},
	{"Mode  4: 800x600", 800, 600, 1},
	{"Mode  5: 960x720", 960, 720, 1},
	{"Mode  6: 1024x768", 1024, 768, 1},
	{"Mode  7: 1152x864", 1152, 864, 1},
	{"Mode  8: 1280x1024", 1280, 1024, 1},
	{"Mode  9: 1600x1200", 1600, 1200, 1},
	{"Mode 10: 2048x1536", 2048, 1536, 1},
	{"Mode 11: 856x480 (wide)", 856, 480, 1}
};

static int	s_numVidModes = (sizeof (r_vidModes) / sizeof (r_vidModes[0]));

qboolean R_GetModeInfo (int *width, int *height, float *windowAspect, int mode)
{
	vidmode_t	*vm;

	if (mode < -1)
	{
		return qfalse;
	}
	if (mode >= s_numVidModes)
	{
		return qfalse;
	}

	if (mode == -1)
	{
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		*windowAspect = r_customaspect->value;
		return qtrue;
	}

	vm = &r_vidModes[mode];

	*width = vm->width;
	*height = vm->height;
	*windowAspect = (float) vm->width / (vm->height * vm->pixelAspect);

	return qtrue;
}

/*
** R_ModeList_f
*/
static void R_ModeList_f (void)
{
	int i;

	ri.Printf (PRINT_ALL, "\n");
	for (i = 0; i < s_numVidModes; i++)
	{
		ri.Printf (PRINT_ALL, "%s\n", r_vidModes[i].description);
	}
	ri.Printf (PRINT_ALL, "\n");
}


/*
==============================================================================

SCREEN SHOTS

NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
vanilla q3 .. baseq3/screenshots/ *.tga
team arena .. missionpack/screenshots/ *.tga

two commands: "screenshot" and "screenshotJPEG"
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes

==============================================================================
*/

/*
==================
RB_TakeScreenshotCmd
==================
*/
const void *RB_TakeScreenshotCmd (const void *data)
{
	const screenshotCommand_t *cmd = (const screenshotCommand_t *) data;

	tr.screenshotRequested = qtrue;
	strcpy (tr.screenshotName, cmd->fileName);

	return (const void *) (cmd + 1);
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot (int x, int y, int width, int height, char *name)
{
	static char	fileName[MAX_OSPATH]; // bad things if two screenshots per frame?
	screenshotCommand_t	*cmd = R_GetCommandBuffer (sizeof (*cmd));

	if (!cmd)
		return;

	cmd->commandId = RC_SCREENSHOT;

	cmd->x = x;
	cmd->y = y;
	cmd->width = width;
	cmd->height = height;
	Q_strncpyz (fileName, name, sizeof (fileName));
	cmd->fileName = fileName;
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilename (int lastNumber, char *fileName)
{
	if (lastNumber < 0 || lastNumber > 9999)
	{
		Com_sprintf (fileName, MAX_OSPATH, "screenshots/shot9999.jpg");
		return;
	}

	int a = lastNumber / 1000;
	lastNumber -= a * 1000;

	int b = lastNumber / 100;
	lastNumber -= b * 100;

	int c = lastNumber / 10;
	lastNumber -= c * 10;

	int d = lastNumber;

	Com_sprintf (fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.jpg", a, b, c, d);
}


/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
void R_LevelShot (void)
{
#ifdef GL_REMOVED
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source;
	byte		*src, *dst;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	sprintf (checkname, "levelshots/%s.tga", tr.world->baseName);

	source = ri.Hunk_AllocateTempMemory (glConfig.vidWidth * glConfig.vidHeight * 3);

	buffer = ri.Hunk_AllocateTempMemory (128 * 128 * 3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	// pixel size

	glReadPixels (0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_RGB, GL_UNSIGNED_BYTE, source);

	// resample from source
	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for (y = 0; y < 128; y++)
	{
		for (x = 0; x < 128; x++)
		{
			r = g = b = 0;
			for (yy = 0; yy < 3; yy++)
			{
				for (xx = 0; xx < 4; xx++)
				{
					src = source + 3 * (glConfig.vidWidth * (int) ((y * 3 + yy)*yScale) + (int) ((x * 4 + xx)*xScale));
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * (y * 128 + x);
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	ri.FS_WriteFile (checkname, buffer, 128 * 128 * 3 + 18);

	ri.Hunk_FreeTempMemory (buffer);
	ri.Hunk_FreeTempMemory (source);

	ri.Printf (PRINT_ALL, "Wrote %s\n", checkname);
#endif
}

/*
==================
R_ScreenShot_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShot_f (void)
{
	char		checkname[MAX_OSPATH];
	static	int	lastNumber = -1;
	qboolean	silent;

	if (!strcmp (ri.Cmd_Argv (1), "levelshot"))
	{
		R_LevelShot ();
		return;
	}

	if (!strcmp (ri.Cmd_Argv (1), "silent"))
		silent = qtrue;
	else silent = qfalse;

	if (ri.Cmd_Argc () == 2 && !silent)
	{
		// explicit filename
		Com_sprintf (checkname, MAX_OSPATH, "screenshots/%s.jpg", ri.Cmd_Argv (1));
	}
	else
	{
		// scan for a free filename

		// if we have saved a previous screenshot, don't scan
		// again, because recording demo avis can involve
		// thousands of shots
		if (lastNumber == -1)
			lastNumber = 0;

		// scan for a free number
		for (; lastNumber <= 9999; lastNumber++)
		{
			R_ScreenshotFilename (lastNumber, checkname);

			if (!ri.FS_FileExists (checkname))
			{
				break; // file doesn't exist
			}
		}

		if (lastNumber == 10000)
		{
			ri.Printf (PRINT_ALL, "ScreenShot: Couldn't create a file\n");
			return;
		}

		lastNumber++;
	}

	R_TakeScreenshot (0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname);

	if (!silent)
	{
		ri.Printf (PRINT_ALL, "Wrote %s\n", checkname);
	}
}


//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState (void)
{
	for (int i = 0; i < MAX_SHADER_STAGES; i++)
	{
		// set all shader stages to trilinear
		d3d_Device->lpVtbl->SetSamplerState (d3d_Device, i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		d3d_Device->lpVtbl->SetSamplerState (d3d_Device, i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
		d3d_Device->lpVtbl->SetSamplerState (d3d_Device, i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

		// these must also recache
		glState.currenttextures[i] = NULL;
		glState.currentWrapClampMode[i] = -1;
	}

	GL_TextureMode (r_textureMode->string);

	// make sure our GL state vector is set correctly
	// toggle all state bits on and off so that the first set will go to something valid
	GL_State (0xffffffff);
	GL_State (0);

	GL_Cull (CT_BACK_SIDED);
	GL_Cull (CT_TWO_SIDED);

	/*
	GL_REMOVED
	glClearDepth( 1.0f );

	glCullFace(GL_FRONT);

	glColor4f (1,1,1,1);

	glEnable(GL_TEXTURE_2D);

	glShadeModel( GL_SMOOTH );
	glDepthFunc( GL_LEQUAL );

	// the vertex array is always enabled, but the color and texture
	// arrays are enabled and disabled around the compiled vertex array call
	glEnableClientState (GL_VERTEX_ARRAY);


	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glDepthMask( GL_TRUE );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	*/
}


/*
================
GfxInfo_f
================
*/
void GfxInfo_f (void)
{
	cvar_t *sys_cpustring = ri.Cvar_Get ("sys_cpustring", "", 0);

	const char *enablestrings[] =
	{
		"disabled",
		"enabled"
	};

	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	ri.Printf (PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string);
	ri.Printf (PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	ri.Printf (PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);
	ri.Printf (PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize);
	ri.Printf (PRINT_ALL, "GL_MAX_ACTIVE_TEXTURES_ARB: %d\n", glConfig.maxActiveTextures);
	ri.Printf (PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
	ri.Printf (PRINT_ALL, "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen->integer == 1]);

	if (glConfig.displayFrequency)
		ri.Printf (PRINT_ALL, "%d\n", glConfig.displayFrequency);
	else ri.Printf (PRINT_ALL, "N/A\n");

	ri.Printf (PRINT_ALL, "CPU: %s\n", sys_cpustring->string);
	ri.Printf (PRINT_ALL, "texturemode: %s\n", r_textureMode->string);
	ri.Printf (PRINT_ALL, "texture bits: %d\n", r_texturebits->integer);

	if (r_finish->integer)
		ri.Printf (PRINT_ALL, "Forcing glFinish\n");
}

/*
===============
R_Register
===============
*/
void R_Register (void)
{
	//
	// latched and archived variables
	//
	r_glDriver = ri.Cvar_Get ("r_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE | CVAR_LATCH);
	r_allowExtensions = ri.Cvar_Get ("r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_compressed_textures = ri.Cvar_Get ("r_ext_compressed_textures", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_gamma_control = ri.Cvar_Get ("r_ext_gamma_control", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_multitexture = ri.Cvar_Get ("r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_compiled_vertex_array = ri.Cvar_Get ("r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_LATCH);

#ifdef __linux__ // broken on linux
	r_ext_texture_env_add = ri.Cvar_Get ("r_ext_texture_env_add", "0", CVAR_ARCHIVE | CVAR_LATCH);
#else
	r_ext_texture_env_add = ri.Cvar_Get ("r_ext_texture_env_add", "1", CVAR_ARCHIVE | CVAR_LATCH);
#endif

	r_picmip = ri.Cvar_Get ("r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_roundImagesDown = ri.Cvar_Get ("r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_colorMipLevels = ri.Cvar_Get ("r_colorMipLevels", "0", CVAR_LATCH);
	AssertCvarRange (r_picmip, 0, 16, qtrue);
	r_detailTextures = ri.Cvar_Get ("r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_texturebits = ri.Cvar_Get ("r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_colorbits = ri.Cvar_Get ("r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_stereo = ri.Cvar_Get ("r_stereo", "0", CVAR_ARCHIVE | CVAR_LATCH);

#ifdef __linux__
	r_stencilbits = ri.Cvar_Get ("r_stencilbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
#else
	r_stencilbits = ri.Cvar_Get ("r_stencilbits", "8", CVAR_ARCHIVE | CVAR_LATCH);
#endif

	r_depthbits = ri.Cvar_Get ("r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_ignorehwgamma = ri.Cvar_Get ("r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_mode = ri.Cvar_Get ("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
	r_fullscreen = ri.Cvar_Get ("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_customwidth = ri.Cvar_Get ("r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH);
	r_customheight = ri.Cvar_Get ("r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH);
	r_customaspect = ri.Cvar_Get ("r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_simpleMipMaps = ri.Cvar_Get ("r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_vertexLight = ri.Cvar_Get ("r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_uiFullScreen = ri.Cvar_Get ("r_uifullscreen", "0", 0);
	r_subdivisions = ri.Cvar_Get ("r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH);
	r_ignoreFastPath = ri.Cvar_Get ("r_ignoreFastPath", "1", CVAR_ARCHIVE | CVAR_LATCH);

	// temporary latched variables that can only change over a restart
	r_displayRefresh = ri.Cvar_Get ("r_displayRefresh", "0", CVAR_LATCH);
	AssertCvarRange (r_displayRefresh, 0, 200, qtrue);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", CVAR_LATCH | CVAR_CHEAT);
	r_intensity = ri.Cvar_Get ("r_intensity", "1", CVAR_LATCH);
	r_singleShader = ri.Cvar_Get ("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH);

	// archived variables that can change at any time
	r_lodCurveError = ri.Cvar_Get ("r_lodCurveError", "250", CVAR_ARCHIVE | CVAR_CHEAT);
	r_lodbias = ri.Cvar_Get ("r_lodbias", "0", CVAR_ARCHIVE);
	r_znear = ri.Cvar_Get ("r_znear", "4", CVAR_CHEAT);
	AssertCvarRange (r_znear, 0.001f, 200, qtrue);
	r_ignoreGLErrors = ri.Cvar_Get ("r_ignoreGLErrors", "1", CVAR_ARCHIVE);
	r_inGameVideo = ri.Cvar_Get ("r_inGameVideo", "1", CVAR_ARCHIVE);
	r_drawSun = ri.Cvar_Get ("r_drawSun", "0", CVAR_ARCHIVE);
	r_dynamiclight = ri.Cvar_Get ("r_dynamiclight", "1", CVAR_ARCHIVE);
	r_dlightBacks = ri.Cvar_Get ("r_dlightBacks", "1", CVAR_ARCHIVE);
	r_finish = ri.Cvar_Get ("r_finish", "0", CVAR_ARCHIVE);
	r_textureMode = ri.Cvar_Get ("r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	r_swapInterval = ri.Cvar_Get ("r_swapInterval", "0", CVAR_ARCHIVE);

	r_gamma = ri.Cvar_Get ("r_gamma", "1", CVAR_ARCHIVE);
	r_brightness = ri.Cvar_Get ("r_brightness", "1", CVAR_ARCHIVE);
	r_desaturate_lightmaps = ri.Cvar_Get ("r_desaturate_lightmaps", "1", CVAR_ARCHIVE);

	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE);

	r_railWidth = ri.Cvar_Get ("r_railWidth", "16", CVAR_ARCHIVE);
	r_railCoreWidth = ri.Cvar_Get ("r_railCoreWidth", "6", CVAR_ARCHIVE);
	r_railSegmentLength = ri.Cvar_Get ("r_railSegmentLength", "32", CVAR_ARCHIVE);

	r_primitives = ri.Cvar_Get ("r_primitives", "0", CVAR_ARCHIVE);

	r_ambientScale = ri.Cvar_Get ("r_ambientScale", "0.6", CVAR_CHEAT);
	r_directedScale = ri.Cvar_Get ("r_directedScale", "1", CVAR_CHEAT);

	// temporary variables that can change at any time
	r_showImages = ri.Cvar_Get ("r_showImages", "0", CVAR_TEMP);

	r_debugLight = ri.Cvar_Get ("r_debuglight", "0", CVAR_TEMP);
	r_debugSort = ri.Cvar_Get ("r_debugSort", "0", CVAR_CHEAT);
	r_printShaders = ri.Cvar_Get ("r_printShaders", "0", 0);
	r_saveFontData = ri.Cvar_Get ("r_saveFontData", "0", 0);

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT);
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", CVAR_CHEAT);
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", 0);
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT);

	r_showSmp = ri.Cvar_Get ("r_showSmp", "0", CVAR_CHEAT);
	r_skipBackEnd = ri.Cvar_Get ("r_skipBackEnd", "0", CVAR_CHEAT);

	r_lodscale = ri.Cvar_Get ("r_lodscale", "5", CVAR_CHEAT);
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", CVAR_CHEAT);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", CVAR_CHEAT);
	r_ignore = ri.Cvar_Get ("r_ignore", "1", CVAR_CHEAT);
	r_nocull = ri.Cvar_Get ("r_nocull", "0", CVAR_CHEAT);
	r_novis = ri.Cvar_Get ("r_novis", "0", CVAR_CHEAT);
	r_showcluster = ri.Cvar_Get ("r_showcluster", "0", CVAR_CHEAT);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", CVAR_CHEAT);
	r_verbose = ri.Cvar_Get ("r_verbose", "0", CVAR_CHEAT);
	r_logFile = ri.Cvar_Get ("r_logFile", "0", CVAR_CHEAT);
	r_debugSurface = ri.Cvar_Get ("r_debugSurface", "0", CVAR_CHEAT);
	r_nobind = ri.Cvar_Get ("r_nobind", "0", CVAR_CHEAT);
	r_clear = ri.Cvar_Get ("r_clear", "0", CVAR_CHEAT);
	r_depthBiasFactor = ri.Cvar_Get ("r_depthBiasFactor", "-0.1", CVAR_ARCHIVE);
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", CVAR_CHEAT);
	r_noportals = ri.Cvar_Get ("r_noportals", "0", CVAR_CHEAT);
	r_shadows = ri.Cvar_Get ("cg_shadows", "1", 0);

	// make sure all the commands added here are also
	// removed in R_Shutdown
	ri.Cmd_AddCommand ("imagelist", R_ImageList_f);
	ri.Cmd_AddCommand ("shaderlist", R_ShaderList_f);
	ri.Cmd_AddCommand ("skinlist", R_SkinList_f);
	ri.Cmd_AddCommand ("modellist", R_Modellist_f);
	ri.Cmd_AddCommand ("modelist", R_ModeList_f);
	ri.Cmd_AddCommand ("screenshot", R_ScreenShot_f);
	ri.Cmd_AddCommand ("gfxinfo", GfxInfo_f);
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	ri.Printf (PRINT_ALL, "----- R_Init -----\n");

	// clear all our internal state
	Com_Memset (&tr, 0, sizeof (tr));
	Com_Memset (&backEnd, 0, sizeof (backEnd));
	Com_Memset (&tess, 0, sizeof (tess));

	// init function tables
	for (int i = 0; i < FUNCTABLE_SIZE; i++)
	{
		tr.sinTable[i] = sin (DEG2RAD (i * 360.0f / ((float) (FUNCTABLE_SIZE - 1))));
		tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float) i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if (i < FUNCTABLE_SIZE / 2)
		{
			if (i < FUNCTABLE_SIZE / 4)
				tr.triangleTable[i] = (float) i / (FUNCTABLE_SIZE / 4);
			else tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
		}
		else tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
	}

	R_InitFogTable ();

	R_NoiseInit ();

	R_Register ();

	R_ToggleSmpFrame ();

	InitOpenGL ();

	R_InitImages ();

	R_InitShaders ();

	R_InitSkins ();

	R_ModelInit ();

	R_InitFreeType ();

	ri.Printf (PRINT_ALL, "----- finished R_Init -----\n");
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown (qboolean destroyWindow)
{
	ri.Printf (PRINT_ALL, "RE_Shutdown (%i)\n", destroyWindow);

	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand ("screenshotJPEG");
	ri.Cmd_RemoveCommand ("screenshot");
	ri.Cmd_RemoveCommand ("imagelist");
	ri.Cmd_RemoveCommand ("shaderlist");
	ri.Cmd_RemoveCommand ("skinlist");
	ri.Cmd_RemoveCommand ("gfxinfo");
	ri.Cmd_RemoveCommand ("modelist");
	ri.Cmd_RemoveCommand ("shaderstate");

	if (tr.registered)
	{
		R_SyncRenderThread ();
		R_DeleteTextures ();
		GL_ShutdownPrograms ();
	}

	R_DoneFreeType ();

	// shut down platform specific OpenGL stuff
	if (destroyWindow)
	{
		GLimp_Shutdown ();
	}

	tr.registered = qfalse;
}


/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration (void)
{
	R_SyncRenderThread ();

	if (!Sys_LowPhysicalMemory ())
	{
		RB_ShowImages ();
	}
}


/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
refexport_t *GetRefAPI (int apiVersion, refimport_t *rimp)
{
	static refexport_t	re;

	ri = *rimp;

	Com_Memset (&re, 0, sizeof (re));

	if (apiVersion != REF_API_VERSION)
	{
		ri.Printf (PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion);
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;

	return &re;
}

