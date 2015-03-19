/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "../../idlib/precompiled.h"
#include "../../renderer/tr_local.h"
#include "local.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

idCVar sys_videoRam("sys_videoRam", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "Texture memory on the video card (in megabytes) - 0: autodetect", 0, 512);

Display *dpy = NULL;
static int scrnum = 0;

Window win = 0;

static EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static EGLContext eglContext = EGL_NO_CONTEXT;
static EGLSurface eglSurface = EGL_NO_SURFACE;

void GLimp_WakeBackEnd(void *a)
{
	common->DPrintf("GLimp_WakeBackEnd stub\n");
}

void GLimp_EnableLogging(bool log)
{
	//common->DPrintf("GLimp_EnableLogging stub\n");
}

void GLimp_FrontEndSleep()
{
	common->DPrintf("GLimp_FrontEndSleep stub\n");
}

void *GLimp_BackEndSleep()
{
	common->DPrintf("GLimp_BackEndSleep stub\n");
	return 0;
}

bool GLimp_SpawnRenderThread(void (*a)())
{
	common->DPrintf("GLimp_SpawnRenderThread stub\n");
	return false;
}

void GLimp_ActivateContext()
{
#if 0
	assert(dpy);
	assert(ctx);
	glXMakeCurrent(dpy, win, ctx);
#endif
}

void GLimp_DeactivateContext()
{
#if 0
	assert(dpy);
	glXMakeCurrent(dpy, None, NULL);
#endif
}

/*
=================
GLimp_SaveGamma

save and restore the original gamma of the system
=================
*/
void GLimp_SaveGamma()
{
}

/*
=================
GLimp_RestoreGamma

save and restore the original gamma of the system
=================
*/
void GLimp_RestoreGamma()
{
}

/*
=================
GLimp_SetGamma

save gamma and brightness value to global structure
=================
*/
void GLimp_SetGamma(float b, float g)
{
	glConfig.valBrightness = b;
	glConfig.valGamma = g;
}

void GLimp_Shutdown()
{
	if (dpy) {

		Sys_XUninstallGrabs();

		GLimp_RestoreGamma();

		eglDestroyContext(eglDisplay, eglContext);
		eglDestroySurface(eglDisplay, eglSurface);
		eglTerminate(eglDisplay);

		XDestroyWindow(dpy, win);

		XFlush(dpy);

		// FIXME: that's going to crash
		//XCloseDisplay( dpy );

		dpy = NULL;
		win = 0;

		eglDisplay = EGL_NO_DISPLAY;
		eglContext = EGL_NO_CONTEXT;
		eglSurface = EGL_NO_SURFACE;
	}
}

void GLimp_SwapBuffers()
{
	assert(eglDisplay && eglSurface);
	eglSwapBuffers(eglDisplay, eglSurface);
}

/*
** XErrorHandler
**   the default X error handler exits the application
**   I found out that on some hosts some operations would raise X errors (GLXUnsupportedPrivateRequest)
**   but those don't seem to be fatal .. so the default would be to just ignore them
**   our implementation mimics the default handler behaviour (not completely cause I'm lazy)
*/
int idXErrorHandler(Display *l_dpy, XErrorEvent *ev)
{
	char buf[1024];
	common->Printf("Fatal X Error:\n");
	common->Printf("  Major opcode of failed request: %d\n", ev->request_code);
	common->Printf("  Minor opcode of failed request: %d\n", ev->minor_code);
	common->Printf("  Serial number of failed request: %lu\n", ev->serial);
	XGetErrorText(l_dpy, ev->error_code, buf, 1024);
	common->Printf("%s\n", buf);
	return 0;
}

bool GLimp_OpenDisplay(void)
{
	if (dpy) {
		return true;
	}

	if (cvarSystem->GetCVarInteger("net_serverDedicated") == 1) {
		common->DPrintf("not opening the display: dedicated server\n");
		return false;
	}

	common->Printf("Setup X display connection\n");

	// that should be the first call into X
	if (!XInitThreads()) {
		common->Printf("XInitThreads failed\n");
		return false;
	}

	// set up our custom error handler for X failures
	XSetErrorHandler(&idXErrorHandler);

	if (!(dpy = XOpenDisplay(NULL))) {
		common->Printf("Couldn't open the X display\n");
		return false;
	}

	scrnum = DefaultScreen(dpy);

	if (!(eglDisplay = eglGetDisplay((EGLNativeDisplayType) dpy))) {
		common->Printf("Couldn't open the EGL display\n");
		return false;
	}

	if (!eglInitialize(eglDisplay, NULL, NULL)) {
		common->Printf("Couldn't initialize EGL\n");
		return false;
	}

	return true;
}

/*
===============
GLX_Init
===============
*/
	EGLConfig eglConfig;
	EGLint eglNumConfig;

int GLX_Init(glimpParms_t a)
{
	EGLint attrib[] = {
		EGL_RED_SIZE, 8,	//  1,  2
		EGL_GREEN_SIZE, 8,	//  3,  4
		EGL_BLUE_SIZE, 8,	//  5,  6
		EGL_ALPHA_SIZE, 8,	//  7,  8
		EGL_DEPTH_SIZE, 24,	//  9, 10
		EGL_STENCIL_SIZE, 8,	// 11, 12
		EGL_BUFFER_SIZE, 24,	// 13, 14
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,	// 15, 16
		EGL_NONE,	// 17
	};
	// these match in the array
#define ATTR_RED_IDX	1
#define ATTR_GREEN_IDX	3
#define ATTR_BLUE_IDX	5
#define ATTR_ALPHA_IDX	7
#define ATTR_DEPTH_IDX	9
#define ATTR_STENCIL_IDX	11
#define ATTR_BUFFER_SIZE_IDX	13
	Window root;
	XVisualInfo *visinfo;
	XSetWindowAttributes attr;
	XSizeHints sizehints;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int actualWidth, actualHeight;
	int i;
	const char *glstring;

	if (!GLimp_OpenDisplay()) {
		return false;
	}

	common->Printf("Initializing OpenGL display\n");

	root = RootWindow(dpy, scrnum);
    
	glConfig.isFullscreen = a.fullScreen;

	if (glConfig.isFullscreen)
	{
		int screen_num = DefaultScreen(dpy);
		actualWidth = glConfig.vidWidth = DisplayWidth(dpy, screen_num);
		actualHeight = glConfig.vidHeight = DisplayHeight(dpy, screen_num);
	}
	else
	{
		actualWidth = glConfig.vidWidth;
		actualHeight = glConfig.vidHeight;
	}	
	// color, depth and stencil
	colorbits = 24;
	depthbits = 24;
	stencilbits = 8;

	for (i = 0; i < 16; i++) {
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i) {
			// one pass, reduce
			switch (i / 4) {
				case 2:

					if (colorbits == 24)
						colorbits = 16;

					break;
				case 1:

					if (depthbits == 24)
						depthbits = 16;
					else if (depthbits == 16)
						depthbits = 8;

				case 3:

					if (stencilbits == 24)
						stencilbits = 16;
					else if (stencilbits == 16)
						stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3) {		// reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2) {		// reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1) {		// reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		if (tcolorbits == 24) {
			attrib[ATTR_RED_IDX] = 8;
			attrib[ATTR_GREEN_IDX] = 8;
			attrib[ATTR_BLUE_IDX] = 8;
			attrib[ATTR_BUFFER_SIZE_IDX] = 24;
		} else {
			// must be 16 bit
			attrib[ATTR_RED_IDX] = 4;
			attrib[ATTR_GREEN_IDX] = 4;
			attrib[ATTR_BLUE_IDX] = 4;
			attrib[ATTR_BUFFER_SIZE_IDX] = 16;
		}

		attrib[ATTR_DEPTH_IDX] = tdepthbits;	// default to 24 depth
		attrib[ATTR_STENCIL_IDX] = tstencilbits;

		if (!eglChooseConfig(eglDisplay, attrib, &eglConfig, 1, &eglNumConfig)) {
			common->Printf("Couldn't get a EGL config 0x%04x\n", eglGetError());
			continue;
		}

		common->Printf("Using %d/%d/%d Color bits, %d Alpha bits, %d depth, %d stencil display.\n",
		               attrib[ATTR_RED_IDX], attrib[ATTR_GREEN_IDX],
		               attrib[ATTR_BLUE_IDX], attrib[ATTR_ALPHA_IDX],
		               attrib[ATTR_DEPTH_IDX],
		               attrib[ATTR_STENCIL_IDX]);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	if (!eglNumConfig) {
		common->Printf("No acceptable EGL configurations found!\n");
		return false;
	}

	visinfo = (XVisualInfo*)malloc(sizeof(XVisualInfo));
	if (!(XMatchVisualInfo(dpy, scrnum, 32, TrueColor, visinfo))) {
		common->Printf("Couldn't get a visual\n");
		return false;
	}

	// window attributes
	attr.background_pixel = BlackPixel(dpy, scrnum);
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;

	win = XCreateWindow(dpy, root, 0, 0,
			    actualWidth, actualHeight,
			    0, visinfo->depth, InputOutput,
			    visinfo->visual,
			    CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			    &attr);

	XStoreName(dpy, win, GAME_NAME);

	// don't let the window be resized
	// FIXME: allow resize (win32 does)
	sizehints.flags = PMinSize | PMaxSize;
	sizehints.min_width = sizehints.max_width = actualWidth;
	sizehints.min_height = sizehints.max_height = actualHeight;

	XSetWMNormalHints(dpy, win, &sizehints);

	XMapWindow(dpy, win);

	// Free the visinfo after we're done with it
	XFree(visinfo);

	eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, win, NULL);
	if (eglSurface == EGL_NO_SURFACE) {
		common->Printf("Couldn't get a EGL surface\n");
		return false;
	}

	EGLint ctxattrib[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, ctxattrib);
	if (eglContext == EGL_NO_CONTEXT) {
		common->Printf("Couldn't get a EGL context\n");
		return false;
	}

	eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

	if (cvarSystem->GetCVarInteger("r_swapInterval") > -1) {
		if (eglSwapInterval(eglDisplay, cvarSystem->GetCVarInteger("r_swapInterval")) != EGL_TRUE) {
			common->Printf("Couldn't set EGL swap interval\n");
			return false;
		}
	}

	glstring = (const char *) glGetString(GL_RENDERER);
	common->Printf("GL_RENDERER: %s\n", glstring);

	glstring = (const char *) glGetString(GL_EXTENSIONS);
	common->Printf("GL_EXTENSIONS: %s\n", glstring);

	// FIXME: here, software GL test
	
	if (glConfig.isFullscreen) {
		Sys_GrabMouseCursor(true);

		XEvent xev;
		Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
		Atom fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
		memset(&xev, 0, sizeof(xev));
		xev.type = ClientMessage;
		xev.xclient.window = win;
		xev.xclient.message_type = wm_state;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = 1;
		xev.xclient.data.l[1] = fullscreen;
		xev.xclient.data.l[2] = 0;
		XMapWindow(dpy, win);
		XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
		XFlush(dpy);		
	}

	return true;
}

/*
===================
GLimp_Init

This is the platform specific OpenGL initialization function.  It
is responsible for loading OpenGL, initializing it,
creating a window of the appropriate size, doing
fullscreen manipulations, etc.  Its overall responsibility is
to make sure that a functional OpenGL subsystem is operating
when it returns to the ref.

If there is any failure, the renderer will revert back to safe
parameters and try again.
===================
*/
bool GLimp_Init(glimpParms_t a)
{

	if (!GLimp_OpenDisplay()) {
		return false;
	}

	if (!GLX_Init(a)) {
		return false;
	}

	return true;
}

/*
===================
GLimp_SetScreenParms
===================
*/
bool GLimp_SetScreenParms(glimpParms_t parms)
{
	return true;
}

/*
================
Sys_GetVideoRam
returns in megabytes
open your own display connection for the query and close it
using the one shared with GLimp_Init is not stable
================
*/
int Sys_GetVideoRam(void)
{
	static int run_once = 0;
	int major, minor, value;
	Display *l_dpy;
	int l_scrnum;

	if (run_once) {
		return run_once;
	}

	if (sys_videoRam.GetInteger()) {
		run_once = sys_videoRam.GetInteger();
		return sys_videoRam.GetInteger();
	}

	// try a few strategies to guess the amount of video ram
	common->Printf("guessing video ram ( use +set sys_videoRam to force ) ..\n");

	if (!GLimp_OpenDisplay()) {
		run_once = 64;
		return run_once;
	}

	l_dpy = dpy;
	l_scrnum = scrnum;

	// try ATI /proc read ( for the lack of a better option )
	int fd;

	if ((fd = open("/proc/dri/0/umm", O_RDONLY)) != -1) {
		int len;
		char umm_buf[ 1024 ];
		char *line;

		if ((len = read(fd, umm_buf, 1024)) != -1) {
			// should be way enough to get the full file
			// grab "free  LFB = " line and "free  Inv = " lines
			umm_buf[ len-1 ] = '\0';
			line = umm_buf;
			line = strtok(umm_buf, "\n");
			int total = 0;

			while (line) {
				if (strlen(line) >= 13 && strstr(line, "max   LFB =") == line) {
					total += atoi(line + 12);
				} else if (strlen(line) >= 13 && strstr(line, "max   Inv =") == line) {
					total += atoi(line + 12);
				}

				line = strtok(NULL, "\n");
			}

			if (total) {
				run_once = total / 1048576;
				// round to the lower 16Mb
				run_once &= ~15;
				return run_once;
			}
		} else {
			common->Printf("read /proc/dri/0/umm failed: %s\n", strerror(errno));
		}
	}

	common->Printf("guess failed, return default low-end VRAM setting ( 64MB VRAM )\n");
	run_once = 64;
	return run_once;
}
