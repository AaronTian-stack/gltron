#include "video/nebu_renderer_gl.h"
#include "video/nebu_video_system.h"
#include "base/nebu_system.h"

#include "base/nebu_assert.h"

#include "base/nebu_debug_memory.h"

static SDL_Window *window_handle;
static SDL_GLContext gl_context;
static int width = 0;
static int height = 0;
static int bitdepth = 0;
static int flags = 0;
static int video_initialized = 0;
static SDL_WindowID window_id = 0;

void nebu_Video_Init(void) {
	if(!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
		if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
			fprintf(stderr, "Couldn't initialize SDL video: %s\n", SDL_GetError());
			nebu_assert(0); exit(1); /* OK: critical, no visual */
		}
	}
	video_initialized = 1;
}

void nebu_Video_SetWindowMode(int x, int y, int w, int h) {
  fprintf(stderr, "ignoring (%d,%d) initial window position - feature not implemented\n", x, y);
  width = w;
  height = h;
}

void nebu_Video_GetDimension(int *x, int *y)
{
	*x = width;
	*y = height;
}

void nebu_Video_SetDisplayMode(int f) {
  int zdepth;

  flags = f;
  if(!video_initialized) {
		if(!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
			fprintf(stderr, "[system] can't initialize Video: %s\n", SDL_GetError());
			nebu_assert(0); exit(1); /* OK: critical, no visual */
		}
  }
	if(flags & SYSTEM_DOUBLE)
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	else
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

  if(flags & SYSTEM_32_BIT) {
    zdepth = 24;
    bitdepth = 32;
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  } else {
    zdepth = 16;
    bitdepth = 0;
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
  }
  if(flags & SYSTEM_ALPHA)
	  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  if(flags & SYSTEM_DEPTH)
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, zdepth);
  if(flags & SYSTEM_STENCIL)
		 SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  else 
		 SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
  video_initialized = 1;
}

void printOpenGLDebugInfo(void)
{
	int r, g, b, a;

	fprintf(stderr, "GL vendor: %s\n", glGetString(GL_VENDOR));
	fprintf(stderr, "GL renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stderr, "GL version: %s\n", glGetString(GL_VERSION));

	fprintf(stderr, "Bitdepth:\n");
	nebu_Video_GetDisplayDepth(&r, &g, &b, &a);
	
	fprintf(stderr, "  Red: %d\n", r);
	fprintf(stderr, "  Green: %d\n", g);
	fprintf(stderr, "  Blue: %d\n", b);
	fprintf(stderr, "  Alpha: %d\n", a);
}

void SystemSetGamma(float red, float green, float blue) {
  //SDL_SetGamma(red, green, blue);
}

static void createWindow(const char *name)
{
	SDL_WindowFlags sdl_flags = SDL_WINDOW_OPENGL;
	if (flags & SYSTEM_FULLSCREEN)
		sdl_flags |= SDL_WINDOW_FULLSCREEN;

	window_handle = SDL_CreateWindow(name, width, height, sdl_flags);
	if (window_handle == NULL) {
		fprintf(stderr, "[system] Couldn't create window: %s\n", SDL_GetError());
		nebu_assert(0); exit(1); /* OK: critical, no visual */
	}

	gl_context = SDL_GL_CreateContext(window_handle);
	if (gl_context == NULL) {
		fprintf(stderr, "[system] Couldn't create GL context: %s\n", SDL_GetError());
		SDL_DestroyWindow(window_handle);
		window_handle = NULL;
		nebu_assert(0); exit(1);
	}

	if (!SDL_GL_MakeCurrent(window_handle, gl_context)) {
		fprintf(stderr, "[system] Couldn't make GL context current: %s\n", SDL_GetError());
		SDL_GL_DestroyContext(gl_context);
		gl_context = NULL;
		SDL_DestroyWindow(window_handle);
		window_handle = NULL;
		nebu_assert(0); exit(1);
	}

	SDL_SetWindowTitle(window_handle, name);
	window_id = SDL_GetWindowID(window_handle);

	if (flags & SYSTEM_DOUBLE) {
		SDL_GL_SetSwapInterval(1);
	}
}

void nebu_Video_GetDisplayDepth(int *r, int *g, int *b, int *a)
{
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, r);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, g);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, b);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, a);
}

int nebu_Video_Create(char *name) {
	nebu_assert (window_id == 0);  // only single window allowed for now
	nebu_assert (width != 0 && height != 0);

	createWindow(name);
	glewInit();
		
	if(!GLEW_ARB_multitexture)
	{
		printOpenGLDebugInfo();
		nebu_Video_Destroy(window_id);
		nebu_Video_Init();
		nebu_Video_SetDisplayMode(flags);
		// try without alpha
		fprintf(stderr, "trying without destination alpha\n");
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		createWindow(name);
		glewInit();
		if(!GLEW_ARB_multitexture)
		{
			printOpenGLDebugInfo();
			fprintf(stderr, "multitexturing is not available\n");
			nebu_assert(0); exit(1);
		}
	}
	printOpenGLDebugInfo();

	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	nebu_System_SwapBuffers();
	return window_id;
}

void nebu_Video_Destroy(int id) {
  /* quit the video subsytem
	 * otherwise SDL can't create a new context on win32, if the stencil
	 * bits change 
	 */
	/* there used to be some problems (memory leaks, unprober driver unloading)
	 * caused by this, but I can't remember what they where
	 */
  if(window_id != 0 && id == (int)window_id) {
	if(gl_context) {
	  SDL_GL_DestroyContext(gl_context);
	  gl_context = NULL;
	}
	if(window_handle) {
	  SDL_DestroyWindow(window_handle);
	  window_handle = NULL;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	video_initialized = 0;
	window_id = 0;
  } else {
	nebu_assert(0);
  }
}

void SystemReshapeFunc(void(*reshape)(int w, int h)) {
	fprintf(stderr, "can't set reshape function (%p) - feature not supported\n", reshape);
}

void nebu_Video_WarpPointer(int x, int y) {
	if(window_handle)
		SDL_WarpMouseInWindow(window_handle, (float)x, (float)y);
}

void nebu_Video_CheckErrors(const char *where) {
	int error;
	error = glGetError();
	if(error != GL_NO_ERROR)
	{
		fprintf(stderr, "[glError: %s] - %d\n", where, error);
		nebu_assert(0);
	}
}

SDL_Window* nebu_Video_GetSDLWindow(void) {
	return window_handle;
}

void nebu_Video_SwapBuffers(void) {
	if(window_handle)
		SDL_GL_SwapWindow(window_handle);
}
