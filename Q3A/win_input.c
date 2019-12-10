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
// win_input.c -- win32 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "client.h"
#include "win_local.h"

#define	DIRECTINPUT_VERSION	0x0800

#include <dinput.h>

#pragma comment (lib, "dinput8.lib")

typedef struct
{
	int			oldButtonState;

	qboolean	mouseActive;
	qboolean	mouseInitialized;
	qboolean  mouseStartupDelayed; // delay mouse init to try DI again when we have a window
} WinMouseVars_t;

static WinMouseVars_t s_wmv;

static int	window_center_x, window_center_y;

// MIDI definitions
static void IN_StartupMIDI (void);
static void IN_ShutdownMIDI (void);

#define MAX_MIDIIN_DEVICES	8

typedef struct
{
	int			numDevices;
	MIDIINCAPS	caps[MAX_MIDIIN_DEVICES];

	HMIDIIN		hMidiIn;
} MidiInfo_t;

static MidiInfo_t s_midiInfo;

// Joystick definitions
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V

typedef struct
{
	qboolean	avail;
	int			id;			// joystick number
	JOYCAPS		jc;

	int			oldbuttonstate;
	int			oldpovstate;

	JOYINFOEX	ji;
} joystickInfo_t;

static	joystickInfo_t	joy;


cvar_t	*in_midi;
cvar_t	*in_midiport;
cvar_t	*in_midichannel;
cvar_t	*in_mididevice;

cvar_t	*in_mouse;
cvar_t  *in_logitechbug;
cvar_t	*in_joystick;
cvar_t	*in_joyBallScale;
cvar_t	*in_debugJoystick;
cvar_t	*joy_threshold;

qboolean	in_appactive;

// forward-referenced functions
void IN_StartupJoystick (void);
void IN_JoyMove (void);

static void MidiInfo_f (void);

typedef struct mousemove_s
{
	int x;
	int y;
} mousemove_t;


/*
============================================================

WIN32 MOUSE CONTROL

============================================================
*/

/*
================
IN_InitWin32Mouse
================
*/
void IN_InitWin32Mouse (void)
{
}

/*
================
IN_ShutdownWin32Mouse
================
*/
void IN_ShutdownWin32Mouse (void)
{
}

/*
================
IN_ActivateWin32Mouse
================
*/
void IN_ActivateWin32Mouse (void)
{
	int			width, height;
	RECT		window_rect;

	width = GetSystemMetrics (SM_CXSCREEN);
	height = GetSystemMetrics (SM_CYSCREEN);

	GetWindowRect (g_wv.hWnd, &window_rect);
	if (window_rect.left < 0)
		window_rect.left = 0;
	if (window_rect.top < 0)
		window_rect.top = 0;
	if (window_rect.right >= width)
		window_rect.right = width - 1;
	if (window_rect.bottom >= height - 1)
		window_rect.bottom = height - 1;
	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	SetCursorPos (window_center_x, window_center_y);

	SetCapture (g_wv.hWnd);
	ClipCursor (&window_rect);
	while (ShowCursor (FALSE) >= 0);
}

/*
================
IN_DeactivateWin32Mouse
================
*/
void IN_DeactivateWin32Mouse (void)
{
	ClipCursor (NULL);
	ReleaseCapture ();
	while (ShowCursor (TRUE) < 0);
}


/*
================
IN_Win32Mouse
================
*/
void IN_Win32Mouse (mousemove_t *move)
{
	POINT current_pos;

	// find mouse movement
	GetCursorPos (&current_pos);

	// force the mouse to the center, so there's room to move
	SetCursorPos (window_center_x, window_center_y);

	move->x = current_pos.x - window_center_x;
	move->y = current_pos.y - window_center_y;
}


/*
============================================================

DIRECT INPUT MOUSE CONTROL

============================================================
*/

static LPDIRECTINPUT		di_Object;
static LPDIRECTINPUTDEVICE	di_Mouse;

void IN_DIMouse (mousemove_t *move);

/*
========================
IN_InitDIMouse
========================
*/
qboolean IN_InitDIMouse (void)
{
	if (SUCCEEDED (DirectInput8Create (GetModuleHandle (NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8, (void**) &di_Object, NULL)))
	{
		if (SUCCEEDED (di_Object->lpVtbl->CreateDevice (di_Object, &GUID_SysMouse, &di_Mouse, NULL)))
		{
			if (SUCCEEDED (di_Mouse->lpVtbl->SetDataFormat (di_Mouse, &c_dfDIMouse2)))
			{
				if (SUCCEEDED (di_Mouse->lpVtbl->SetCooperativeLevel (di_Mouse, g_wv.hWnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE)))
				{
					DIPROPDWORD	dipdw = {{sizeof (DIPROPDWORD), sizeof (DIPROPHEADER), 0, DIPH_DEVICE}, 16};

					if (SUCCEEDED (di_Mouse->lpVtbl->SetProperty (di_Mouse, DIPROP_BUFFERSIZE, &dipdw.diph)))
					{
						// clear any pending samples
						mousemove_t move = {0, 0};

						IN_DIMouse (&move);
						IN_DIMouse (&move);

						Com_Printf ("DirectInput initialized.\n");
						return qtrue;
					}
				}
			}
		}
	}

	return qfalse;
}


/*
==========================
IN_ShutdownDIMouse
==========================
*/
void IN_ShutdownDIMouse (void)
{
	if (di_Mouse)
	{
		di_Mouse->lpVtbl->Release (di_Mouse);
		di_Mouse = NULL;
	}

	if (di_Object)
	{
		di_Object->lpVtbl->Release (di_Object);
		di_Object = NULL;
	}
}

/*
==========================
IN_ActivateDIMouse
==========================
*/
void IN_ActivateDIMouse (void)
{
	if (!di_Mouse)
		return;

	// we may fail to reacquire if the window has been recreated
	if (FAILED (di_Mouse->lpVtbl->Acquire (di_Mouse)))
	{
		if (!IN_InitDIMouse ())
		{
			Com_Printf ("Falling back to Win32 mouse support...\n");
			Cvar_Set ("in_mouse", "-1");
		}
	}
}

/*
==========================
IN_DeactivateDIMouse
==========================
*/
void IN_DeactivateDIMouse (void)
{
	if (!di_Mouse)
		return;

	di_Mouse->lpVtbl->Unacquire (di_Mouse);
}


/*
===================
IN_DIMouse
===================
*/
void IN_DIMouse (mousemove_t *move)
{
	if (!di_Mouse)
		return;

	// fetch new events
	for (;;)
	{
		DWORD dwElements = 16;
		DIDEVICEOBJECTDATA di_Data[16];

		switch (di_Mouse->lpVtbl->GetDeviceData (di_Mouse, sizeof (DIDEVICEOBJECTDATA), di_Data, &dwElements, 0))
		{
		case DIERR_INPUTLOST:
		case DIERR_NOTACQUIRED:
			// device was lost; reacquire it
			di_Mouse->lpVtbl->Acquire (di_Mouse);
			return;

		case DI_OK:
			// we can read now
			break;

		default:
			// something else went wrong; attempt to recreate everything
			IN_ShutdownDIMouse ();
			Sleep (100);
			IN_InitDIMouse ();
			return;
		}

		if (dwElements < 1) break;

		// accumulate this move
		for (int i = 0; i < dwElements; i++)
		{
			DIDEVICEOBJECTDATA *od = &di_Data[i];

			switch (od->dwOfs)
			{
			case DIMOFS_BUTTON0:
				if (od->dwData & 0x80)
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE1, qtrue, 0, NULL);
				else Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE1, qfalse, 0, NULL);
				break;

			case DIMOFS_BUTTON1:
				if (od->dwData & 0x80)
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE2, qtrue, 0, NULL);
				else Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE2, qfalse, 0, NULL);
				break;

			case DIMOFS_BUTTON2:
				if (od->dwData & 0x80)
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE3, qtrue, 0, NULL);
				else Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE3, qfalse, 0, NULL);
				break;

			case DIMOFS_BUTTON3:
				if (od->dwData & 0x80)
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE4, qtrue, 0, NULL);
				else Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MOUSE4, qfalse, 0, NULL);
				break;

			case DIMOFS_X:
				move->x += (signed int) od->dwData;
				break;

			case DIMOFS_Y:
				move->y += (signed int) od->dwData;
				break;

				// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=50
			case DIMOFS_Z:
				if ((signed int) od->dwData > 0)
				{
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MWHEELUP, qtrue, 0, NULL);
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MWHEELUP, qfalse, 0, NULL);
				}
				else if ((signed int) od->dwData < 0)
				{
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL);
					Sys_QueEvent (od->dwTimeStamp, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL);
				}

				break;
			}
		}
	}
}

/*
============================================================

MOUSE CONTROL

============================================================
*/

/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse (void)
{
	if (!s_wmv.mouseInitialized)
		return;

	if (!in_mouse->integer)
	{
		s_wmv.mouseActive = qfalse;
		return;
	}

	if (s_wmv.mouseActive)
		return;

	s_wmv.mouseActive = qtrue;

	if (in_mouse->integer != -1)
		IN_ActivateDIMouse ();

	IN_ActivateWin32Mouse ();
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse (void)
{
	if (!s_wmv.mouseInitialized)
		return;

	if (!s_wmv.mouseActive)
		return;

	s_wmv.mouseActive = qfalse;

	IN_DeactivateDIMouse ();
	IN_DeactivateWin32Mouse ();
}



/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse (void)
{
	s_wmv.mouseInitialized = qfalse;
	s_wmv.mouseStartupDelayed = qfalse;

	if (in_mouse->integer == 0)
	{
		Com_Printf ("Mouse control not active.\n");
		return;
	}

	if (in_mouse->integer == -1)
		Com_Printf ("Skipping check for DirectInput\n");
	else
	{
		if (!g_wv.hWnd)
		{
			Com_Printf ("No window for DirectInput mouse init, delaying\n");
			s_wmv.mouseStartupDelayed = qtrue;
			return;
		}

		if (IN_InitDIMouse ())
		{
			s_wmv.mouseInitialized = qtrue;
			return;
		}

		Com_Printf ("Falling back to Win32 mouse support...\n");
	}

	s_wmv.mouseInitialized = qtrue;
	IN_InitWin32Mouse ();
}

/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate)
{
	if (!s_wmv.mouseInitialized)
		return;

	// perform button actions
	for (int i = 0; i < 3; i++)
	{
		if ((mstate & (1 << i)) && !(s_wmv.oldButtonState & (1 << i))) Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, K_MOUSE1 + i, qtrue, 0, NULL);
		if (!(mstate & (1 << i)) && (s_wmv.oldButtonState & (1 << i))) Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, K_MOUSE1 + i, qfalse, 0, NULL);
	}

	s_wmv.oldButtonState = mstate;
}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (void)
{
	mousemove_t move = {0, 0};

	if (di_Mouse)
	{
		IN_DIMouse (&move);

		// di has about this much lower sensitivity so bring the scale back up
		move.x = (move.x * 3) / 2;
		move.y = (move.y * 3) / 2;
	}
	else IN_Win32Mouse (&move);

	if (!move.x && !move.y)
		return;
	else Sys_QueEvent (0, SE_MOUSE, move.x, move.y, 0, NULL);
}


/*
=========================================================================

=========================================================================
*/

/*
===========
IN_Startup
===========
*/
void IN_Startup (void)
{
	Com_Printf ("\n------- Input Initialization -------\n");
	IN_StartupMouse ();
	IN_StartupJoystick ();
	IN_StartupMIDI ();
	Com_Printf ("------------------------------------\n");

	in_mouse->modified = qfalse;
	in_joystick->modified = qfalse;
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
	IN_ShutdownDIMouse ();
	IN_ShutdownMIDI ();
	Cmd_RemoveCommand ("midiinfo");
}


/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	// MIDI input controler variables
	in_midi = Cvar_Get ("in_midi", "0", CVAR_ARCHIVE);
	in_midiport = Cvar_Get ("in_midiport", "1", CVAR_ARCHIVE);
	in_midichannel = Cvar_Get ("in_midichannel", "1", CVAR_ARCHIVE);
	in_mididevice = Cvar_Get ("in_mididevice", "0", CVAR_ARCHIVE);

	Cmd_AddCommand ("midiinfo", MidiInfo_f);

	// mouse variables
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE | CVAR_LATCH);
	in_logitechbug = Cvar_Get ("in_logitechbug", "0", CVAR_ARCHIVE);

	// joystick variables
	in_joystick = Cvar_Get ("in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH);
	in_joyBallScale = Cvar_Get ("in_joyBallScale", "0.02", CVAR_ARCHIVE);
	in_debugJoystick = Cvar_Get ("in_debugjoystick", "0", CVAR_TEMP);

	joy_threshold = Cvar_Get ("joy_threshold", "0.15", CVAR_ARCHIVE);

	IN_Startup ();
}


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate (qboolean active)
{
	in_appactive = active;

	if (!active)
	{
		IN_DeactivateMouse ();
	}
}


/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame (void)
{
	// post joystick events
	IN_JoyMove ();

	if (!s_wmv.mouseInitialized)
	{
		if (s_wmv.mouseStartupDelayed && g_wv.hWnd)
		{
			Com_Printf ("Proceeding with delayed mouse init\n");
			IN_StartupMouse ();
			s_wmv.mouseStartupDelayed = qfalse;
		}
		return;
	}

	if (cls.keyCatchers & KEYCATCH_CONSOLE)
	{
		// temporarily deactivate if not in the game and
		// running on the desktop
		// voodoo always counts as full screen
		if (Cvar_VariableValue ("r_fullscreen") == 0
			&& strcmp (Cvar_VariableString ("r_glDriver"), _3DFX_DRIVER_NAME))
		{
			IN_DeactivateMouse ();
			return;
		}
	}

	if (!in_appactive)
	{
		IN_DeactivateMouse ();
		return;
	}

	IN_ActivateMouse ();

	// post events to the system que
	IN_MouseMove ();

}


/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{
	s_wmv.oldButtonState = 0;
}


/*
=========================================================================

JOYSTICK

=========================================================================
*/

/*
===============
IN_StartupJoystick
===============
*/
void IN_StartupJoystick (void)
{
	int			numdevs;
	MMRESULT	mmr;

	// assume no joystick
	joy.avail = qfalse;

	if (!in_joystick->integer)
	{
		Com_Printf ("Joystick is not active.\n");
		return;
	}

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
		Com_Printf ("joystick not found -- driver not present\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	mmr = 0;
	for (joy.id = 0; joy.id < numdevs; joy.id++)
	{
		Com_Memset (&joy.ji, 0, sizeof (joy.ji));
		joy.ji.dwSize = sizeof (joy.ji);
		joy.ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy.id, &joy.ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Com_Printf ("joystick not found -- no valid joysticks (%x)\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	Com_Memset (&joy.jc, 0, sizeof (joy.jc));
	if ((mmr = joyGetDevCaps (joy.id, &joy.jc, sizeof (joy.jc))) != JOYERR_NOERROR)
	{
		Com_Printf ("joystick not found -- invalid joystick capabilities (%x)\n", mmr);
		return;
	}

	Com_Printf ("Joystick found.\n");
	Com_Printf ("Pname: %s\n", joy.jc.szPname);
	Com_Printf ("OemVxD: %s\n", joy.jc.szOEMVxD);
	Com_Printf ("RegKey: %s\n", joy.jc.szRegKey);

	Com_Printf ("Numbuttons: %i / %i\n", joy.jc.wNumButtons, joy.jc.wMaxButtons);
	Com_Printf ("Axis: %i / %i\n", joy.jc.wNumAxes, joy.jc.wMaxAxes);
	Com_Printf ("Caps: 0x%x\n", joy.jc.wCaps);
	if (joy.jc.wCaps & JOYCAPS_HASPOV)
	{
		Com_Printf ("HASPOV\n");
	}
	else
	{
		Com_Printf ("no POV\n");
	}

	// old button and POV states default to no buttons pressed
	joy.oldbuttonstate = 0;
	joy.oldpovstate = 0;

	// mark the joystick as available
	joy.avail = qtrue;
}

/*
===========
JoyToF
===========
*/
float JoyToF (int value)
{
	float	fValue;

	// move centerpoint to zero
	value -= 32768;

	// convert range from -32768..32767 to -1..1 
	fValue = (float) value / 32768.0;

	if (fValue < -1)
	{
		fValue = -1;
	}
	if (fValue > 1)
	{
		fValue = 1;
	}
	return fValue;
}

int JoyToI (int value)
{
	// move centerpoint to zero
	value -= 32768;

	return value;
}

int	joyDirectionKeys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY16, K_JOY17,
	K_JOY18, K_JOY19,
	K_JOY20, K_JOY21,
	K_JOY22, K_JOY23,

	K_JOY24, K_JOY25,
	K_JOY26, K_JOY27
};

/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove (void)
{
	float	fAxisValue;
	int		i;
	DWORD	buttonstate, povstate;
	int		x, y;

	// verify joystick is available and that the user wants to use it
	if (!joy.avail)
	{
		return;
	}

	// collect the joystick data, if possible
	Com_Memset (&joy.ji, 0, sizeof (joy.ji));
	joy.ji.dwSize = sizeof (joy.ji);
	joy.ji.dwFlags = JOY_RETURNALL;

	if (joyGetPosEx (joy.id, &joy.ji) != JOYERR_NOERROR)
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,\
				// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy.avail = false;
		return;
	}

	if (in_debugJoystick->integer)
	{
		Com_Printf ("%8x %5i %5.2f %5.2f %5.2f %5.2f %6i %6i\n",
			joy.ji.dwButtons,
			joy.ji.dwPOV,
			JoyToF (joy.ji.dwXpos), JoyToF (joy.ji.dwYpos),
			JoyToF (joy.ji.dwZpos), JoyToF (joy.ji.dwRpos),
			JoyToI (joy.ji.dwUpos), JoyToI (joy.ji.dwVpos));
	}

	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = joy.ji.dwButtons;
	for (i = 0; i < joy.jc.wNumButtons; i++)
	{
		if ((buttonstate & (1 << i)) && !(joy.oldbuttonstate & (1 << i)))
		{
			Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, K_JOY1 + i, qtrue, 0, NULL);
		}
		if (!(buttonstate & (1 << i)) && (joy.oldbuttonstate & (1 << i)))
		{
			Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, K_JOY1 + i, qfalse, 0, NULL);
		}
	}
	joy.oldbuttonstate = buttonstate;

	povstate = 0;

	// convert main joystick motion into 6 direction button bits
	for (i = 0; i < joy.jc.wNumAxes && i < 4; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = JoyToF ((&joy.ji.dwXpos)[i]);

		if (fAxisValue < -joy_threshold->value)
		{
			povstate |= (1 << (i * 2));
		}
		else if (fAxisValue > joy_threshold->value)
		{
			povstate |= (1 << (i * 2 + 1));
		}
	}

	// convert POV information from a direction into 4 button bits
	if (joy.jc.wCaps & JOYCAPS_HASPOV)
	{
		if (joy.ji.dwPOV != JOY_POVCENTERED)
		{
			if (joy.ji.dwPOV == JOY_POVFORWARD)
				povstate |= 1 << 12;
			if (joy.ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 1 << 13;
			if (joy.ji.dwPOV == JOY_POVRIGHT)
				povstate |= 1 << 14;
			if (joy.ji.dwPOV == JOY_POVLEFT)
				povstate |= 1 << 15;
		}
	}

	// determine which bits have changed and key an auxillary event for each change
	for (i = 0; i < 16; i++)
	{
		if ((povstate & (1 << i)) && !(joy.oldpovstate & (1 << i)))
		{
			Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, joyDirectionKeys[i], qtrue, 0, NULL);
		}

		if (!(povstate & (1 << i)) && (joy.oldpovstate & (1 << i)))
		{
			Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, joyDirectionKeys[i], qfalse, 0, NULL);
		}
	}
	joy.oldpovstate = povstate;

	// if there is a trackball like interface, simulate mouse moves
	if (joy.jc.wNumAxes >= 6)
	{
		x = JoyToI (joy.ji.dwUpos) * in_joyBallScale->value;
		y = JoyToI (joy.ji.dwVpos) * in_joyBallScale->value;
		if (x || y)
		{
			Sys_QueEvent (g_wv.sysMsgTime, SE_MOUSE, x, y, 0, NULL);
		}
	}
}

/*
=========================================================================

MIDI

=========================================================================
*/

static void MIDI_NoteOff (int note)
{
	int qkey;

	qkey = note - 60 + K_AUX1;

	if (qkey > 255 || qkey < K_AUX1)
		return;

	Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, qkey, qfalse, 0, NULL);
}

static void MIDI_NoteOn (int note, int velocity)
{
	int qkey;

	if (velocity == 0)
		MIDI_NoteOff (note);

	qkey = note - 60 + K_AUX1;

	if (qkey > 255 || qkey < K_AUX1)
		return;

	Sys_QueEvent (g_wv.sysMsgTime, SE_KEY, qkey, qtrue, 0, NULL);
}

static void CALLBACK MidiInProc (HMIDIIN hMidiIn, UINT uMsg, DWORD dwInstance,
	DWORD dwParam1, DWORD dwParam2)
{
	int message;

	switch (uMsg)
	{
	case MIM_OPEN:
		break;
	case MIM_CLOSE:
		break;
	case MIM_DATA:
		message = dwParam1 & 0xff;

		// note on
		if ((message & 0xf0) == 0x90)
		{
			if (((message & 0x0f) + 1) == in_midichannel->integer)
				MIDI_NoteOn ((dwParam1 & 0xff00) >> 8, (dwParam1 & 0xff0000) >> 16);
		}
		else if ((message & 0xf0) == 0x80)
		{
			if (((message & 0x0f) + 1) == in_midichannel->integer)
				MIDI_NoteOff ((dwParam1 & 0xff00) >> 8);
		}
		break;
	case MIM_LONGDATA:
		break;
	case MIM_ERROR:
		break;
	case MIM_LONGERROR:
		break;
	}

	//	Sys_QueEvent( sys_msg_time, SE_KEY, wMsg, qtrue, 0, NULL );
}

static void MidiInfo_f (void)
{
	int i;

	const char *enableStrings[] = {"disabled", "enabled"};

	Com_Printf ("\nMIDI control:       %s\n", enableStrings[in_midi->integer != 0]);
	Com_Printf ("port:               %d\n", in_midiport->integer);
	Com_Printf ("channel:            %d\n", in_midichannel->integer);
	Com_Printf ("current device:     %d\n", in_mididevice->integer);
	Com_Printf ("number of devices:  %d\n", s_midiInfo.numDevices);
	for (i = 0; i < s_midiInfo.numDevices; i++)
	{
		if (i == Cvar_VariableValue ("in_mididevice"))
			Com_Printf ("***");
		else
			Com_Printf ("...");
		Com_Printf ("device %2d:       %s\n", i, s_midiInfo.caps[i].szPname);
		Com_Printf ("...manufacturer ID: 0x%hx\n", s_midiInfo.caps[i].wMid);
		Com_Printf ("...product ID:      0x%hx\n", s_midiInfo.caps[i].wPid);

		Com_Printf ("\n");
	}
}

static void IN_StartupMIDI (void)
{
	int i;

	if (!Cvar_VariableValue ("in_midi"))
		return;

	// enumerate MIDI IN devices
	s_midiInfo.numDevices = midiInGetNumDevs ();

	for (i = 0; i < s_midiInfo.numDevices; i++)
	{
		midiInGetDevCaps (i, &s_midiInfo.caps[i], sizeof (s_midiInfo.caps[i]));
	}

	// open the MIDI IN port
	if (midiInOpen (&s_midiInfo.hMidiIn,
		in_mididevice->integer,
		(unsigned long) MidiInProc,
		(unsigned long) NULL,
		CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
	{
		Com_Printf ("WARNING: could not open MIDI device %d: '%s'\n", in_mididevice->integer, s_midiInfo.caps[(int) in_mididevice->value]);
		return;
	}

	midiInStart (s_midiInfo.hMidiIn);
}

static void IN_ShutdownMIDI (void)
{
	if (s_midiInfo.hMidiIn)
	{
		midiInClose (s_midiInfo.hMidiIn);
	}
	Com_Memset (&s_midiInfo, 0, sizeof (s_midiInfo));
}
