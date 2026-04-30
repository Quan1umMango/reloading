/*
 *
 * Program to demonstrate dynamic reloading with Raylib. 
 * 	
 */

#include<stddef.h>
#include<stdio.h>
#include "raylib.h"

struct AppState {
	size_t counter;
	int ext_win_close; // flag for external functions to signal to close the window
};

size_t HR_STATE_SIZE = sizeof(struct AppState);

void hr_setup(void*p) {
	(*(struct AppState*)p) = (struct AppState){0,0};
	// For some reason, we need to create a dummy window which will never close throughout the life-cycle of the program.
	// If we close all windows, then InitWindow will segfault. Therefore we create this window which will never been seen (because of its dimensions, this window can be anything)
	InitWindow(1,1,"dummy window, wont be seen");
}


// This function will run before recompiling. So we close the current active window here
void hr_before_recomp(void*p) {
	((struct AppState*)p)->ext_win_close = true;
	CloseWindow();
}

// This function will be run before running hr_main and after recompilation
void hr_before_reload(void*p) {
	((struct AppState*)p)->ext_win_close = false;
}


void hr_main(void* p) {
	// Disable logging for Raylib
	SetTraceLogLevel(LOG_ERROR);

	InitWindow(900,500,"Hot Reloading Test");
	struct AppState *state = p;
	size_t *counter = &state->counter;

	while(!WindowShouldClose() && !state->ext_win_close){
		if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) (*counter)++;

		BeginDrawing();
		ClearBackground(GREEN);
		DrawText(TextFormat("Counter value: %d",*counter), 350, 280, 20, WHITE);
		DrawText("Some more text !!",350,300,20,WHITE);
		EndDrawing();
	}
}
