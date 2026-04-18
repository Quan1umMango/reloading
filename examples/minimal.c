/*
 *
 * A simple counter program which saves state on reloading. The state in question is the counter value.
 * Before running, compile this program by running make while being in the 'examples' directory
 * Navigate into the main folder of the project and run (after building it)
 * 	./main ../example/minimal.c ../example/minimal ../example
 *
 */

#include<stdio.h>
#include<unistd.h>

// Prefix HR and hr mean `hotreloading` and are used by the hot-reloading program 
// Size of our state, the hot-reloading program will malloc these many bytes and saves it in between reloads
const size_t HR_STATE_SIZE = sizeof(int);


// This will be only once, when the hot-reloading code is first run. Use this to give default values to your state
void hr_setup(void* state) {
	*(int* )state = 1; // start form 1
}


// This function is the entry point for your hot-reloading code. This is run every time we reload
void hr_main(void* state) {
	int* counter = state;
	while(true) {
		printf("Count = %d\n",(*counter)++); 
		sleep(1);
	}
}
