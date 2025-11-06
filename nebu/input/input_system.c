#include "input/nebu_input_system.h"
#include "input/nebu_system_keynames.h"
#include "base/nebu_system.h"
#include "video/nebu_video_system.h"
#include "scripting/nebu_scripting.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/nebu_debug_memory.h"

static float joystick_threshold = 0;
static int mouse_x = -1;
static int mouse_y = -1;
static int mouse_rel_x = 0;
static int mouse_rel_y = 0;

#define MAX_SUPPORTED_JOYSTICKS 4

static SDL_Joystick *open_joysticks[MAX_SUPPORTED_JOYSTICKS];
static SDL_JoystickID open_joystick_ids[MAX_SUPPORTED_JOYSTICKS];
static int open_joystick_count = 0;

enum { eMaxKeyState = 1024 };
static int keyState[eMaxKeyState];

static void setKeyState(int key, int state)
{
	if(key < eMaxKeyState)
		keyState[key] = state;
}

static int findJoystickIndex(SDL_JoystickID instance_id)
{
	for(int i = 0; i < open_joystick_count; i++) {
		if(open_joystick_ids[i] == instance_id)
			return i;
	}
	return -1;
}

void nebu_Input_Init(void) {
	int i;

	/* keyboard */
	/* SDL3 handles key repeat configuration through events; nothing to do here. */

	/* joystick */
	if(!(SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK)) {
		if(!SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
			const char *s = SDL_GetError();
			fprintf(stderr, "[init] couldn't initialize joysticks: %s\n", s);
		}
	}

	memset(open_joysticks, 0, sizeof(open_joysticks));
	memset(open_joystick_ids, 0, sizeof(open_joystick_ids));
	open_joystick_count = 0;

	if(SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK) {
		int joysticks = 0;
		SDL_JoystickID *joystick_ids = SDL_GetJoysticks(&joysticks);

		/* FIXME: why only two joysticks? */
		int max_joy = 2; /* default... override by setting NEBU_MAX_JOY */
		char *NEBU_MAX_JOY = getenv("NEBU_MAX_JOY");
		if(NEBU_MAX_JOY)
		{
			int n;
			char *endptr;
			errno = 0;
			n = strtol(NEBU_MAX_JOY, &endptr, 10);
			if(n < 0)
				n = 0;
			if(n > MAX_SUPPORTED_JOYSTICKS)
				n = MAX_SUPPORTED_JOYSTICKS; /* this is the max we can handle! */
			if(!*endptr && !errno)
				max_joy = n;
		}

		if(joysticks > max_joy)
			joysticks = max_joy;
		if(joysticks > MAX_SUPPORTED_JOYSTICKS)
			joysticks = MAX_SUPPORTED_JOYSTICKS;

		if(joystick_ids) {
			for(i = 0; i < joysticks; i++) {
				SDL_Joystick *joy = SDL_OpenJoystick(joystick_ids[i]);
				if(joy) {
					open_joysticks[open_joystick_count] = joy;
					open_joystick_ids[open_joystick_count] = SDL_GetJoystickID(joy);
					open_joystick_count++;
				}
			}
			SDL_free(joystick_ids);
		}

		if(open_joystick_count > 0)
			SDL_SetJoystickEventsEnabled(true);
	} else {
		const char *s = SDL_GetError();
		fprintf(stderr, "[init] joystick subsystem not available: %s\n", s);
	}

	for(i = 0; i < eMaxKeyState; i++)
	{
		keyState[i] = NEBU_INPUT_KEYSTATE_UP;
	}
}

void nebu_Input_Grab(void) {
	SDL_Window *window = nebu_Video_GetSDLWindow();
	if(window)
		(void)SDL_SetWindowMouseGrab(window, true);
}

void nebu_Input_Ungrab(void) {
	SDL_Window *window = nebu_Video_GetSDLWindow();
	if(window)
		(void)SDL_SetWindowMouseGrab(window, false);
}

void nebu_Input_HidePointer(void) {
	SDL_HideCursor();
}

void nebu_Input_UnhidePointer(void) {
	SDL_ShowCursor();
}

void nebu_Input_SetRelativeMouseMode(int enabled) {
	SDL_Window *window = nebu_Video_GetSDLWindow();
	if(window) {
		SDL_SetWindowRelativeMouseMode(window, enabled ? true : false);
		(void)SDL_SetWindowMouseGrab(window, enabled ? true : false);
	}
}

void SystemMouse(int buttons, int state, int x, int y) {
	if(current && current->mouse)
		current->mouse(buttons, state, x, y);
}


int nebu_Input_GetKeyState(int key)
{
	if(key > eMaxKeyState)
		return NEBU_INPUT_KEYSTATE_UP;
	else
		return keyState[key];
}

void nebu_Input_Mouse_GetDelta(int *x, int *y)
{
	*x = mouse_rel_x;
	*y = mouse_rel_y;
}

void nebu_Input_Mouse_WarpToOrigin(void)
{
	mouse_rel_x = 0;
	mouse_rel_y = 0;
}

void SystemMouseMotion(int x, int y) {
	// save mouse position
	// printf("[input] mouse motion to %d, %d\n", x, y);
	mouse_x = x;
	mouse_y = y;
	if(current && current->mouseMotion)
		current->mouseMotion(x, y);
}

const char* nebu_Input_GetKeyname(int key) {
	if(key < SYSTEM_CUSTOM_KEYS)
		return SDL_GetKeyName(key);
	else {
		int i;
		
		for(i = 0; i < CUSTOM_KEY_COUNT; i++) {
			if(custom_keys.key[i].key == key)
				return custom_keys.key[i].name;
		}
		return "unknown custom key";
	}
}  

void nebu_Intern_HandleInput(SDL_Event *event) {
	int key, state;
	static int joy_axis_state[MAX_SUPPORTED_JOYSTICKS] = { 0 };
	static int joy_lastaxis[MAX_SUPPORTED_JOYSTICKS] = { 0 };

	switch(event->type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		state = (event->type == SDL_EVENT_KEY_DOWN) ?
			NEBU_INPUT_KEYSTATE_DOWN : NEBU_INPUT_KEYSTATE_UP;

		key = event->key.key;
		setKeyState(key, state);
		if(current && current->keyboard)
			current->keyboard(state, key, 0, 0);
		break;
	case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
		int joy_index = findJoystickIndex(event->jaxis.which);
		if(joy_index < 0 || joy_index >= MAX_SUPPORTED_JOYSTICKS)
			break;

		if(abs(event->jaxis.value) <= joystick_threshold * SYSTEM_JOY_AXIS_MAX) {
			if(joy_axis_state[joy_index] & (1 << event->jaxis.axis)) {
				joy_axis_state[joy_index] &= ~(1 << event->jaxis.axis);
				key = SYSTEM_JOY_LEFT + joy_index * SYSTEM_JOY_OFFSET;
				if(event->jaxis.axis == 1)
					key += 2;
				if(joy_lastaxis[joy_index] & (1 << event->jaxis.axis))
					key++;
				setKeyState(key, NEBU_INPUT_KEYSTATE_UP);
				if(current && current->keyboard)
					current->keyboard(NEBU_INPUT_KEYSTATE_UP, key, 0, 0);
			}
		} else {
			if(!(joy_axis_state[joy_index] & (1 << event->jaxis.axis))) {
				joy_axis_state[joy_index] |= (1 << event->jaxis.axis);
				key = SYSTEM_JOY_LEFT + joy_index * SYSTEM_JOY_OFFSET;
				if(event->jaxis.axis == 1)
					key += 2;
				if(event->jaxis.value > 0) {
					key++;
					joy_lastaxis[joy_index] |= (1 << event->jaxis.axis);
				} else {
					joy_lastaxis[joy_index] &= ~(1 << event->jaxis.axis);
				}
				setKeyState(key, NEBU_INPUT_KEYSTATE_DOWN);
				if(current && current->keyboard)
					current->keyboard(NEBU_INPUT_KEYSTATE_DOWN, key, 0, 0);
			}
		}
		break;
	}
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP: {
		int joy_index = findJoystickIndex(event->jbutton.which);
		if(joy_index < 0 || joy_index >= MAX_SUPPORTED_JOYSTICKS)
			break;
		state = event->jbutton.down ? NEBU_INPUT_KEYSTATE_DOWN : NEBU_INPUT_KEYSTATE_UP;
		key = SYSTEM_JOY_BUTTON_0 + event->jbutton.button +
			SYSTEM_JOY_OFFSET * joy_index;
		setKeyState(key, state);
		if(current && current->keyboard)
			current->keyboard(state, key, 0, 0);
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		SystemMouse(event->button.button,
			event->button.down ? SYSTEM_MOUSEPRESSED : SYSTEM_MOUSERELEASED,
			(int)event->button.x,
			(int)event->button.y);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		SystemMouseMotion((int)event->motion.x, (int)event->motion.y);
		mouse_rel_x += (int)event->motion.xrel;
		mouse_rel_y += (int)event->motion.yrel;
		break;
	default:
		break;
	}
}

void SystemSetJoyThreshold(float f) { 
	joystick_threshold = f;
}
