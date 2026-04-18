#include<stdio.h>
#include<stdlib.h>
#include<dlfcn.h>
#include<sys/inotify.h>
#include<sys/stat.h>
#include<errno.h>
#include<unistd.h>
#include<limits.h>
#include<string.h>
#include<libgen.h>
#include<pthread.h>
#include<stdatomic.h>

pthread_mutex_t keep_exec_mutex = PTHREAD_MUTEX_INITIALIZER;
atomic_bool keep_exec_b = false; 


typedef void (*hr_main_t)(void*);
typedef void (*hr_setup_t)(void*);
typedef void (*hr_before_reload_t)(void*);

#define FATAL(msg,...) do { printf("[FATAL ERROR] "); printf(msg,##__VA_ARGS__); printf("\n"); exit(0); } while(0)

#ifndef NO_INFO_MSGS 
#define INFO(msg,...) do {\
	printf("[INFO] ");  \
	printf(msg,##__VA_ARGS__);  \
	printf("\n");  \
}while(0);
#else
#define INFO(msg,...) do {} while(0)
#endif

struct HrMainLoopParams {
	void* fn_ptr;
	void* state;
};

void hr_main_loop_do_nun(void*) {}

void* hr_main_loop(void* params) {
	struct HrMainLoopParams* p = params;
	hr_main_t f = p->fn_ptr;
	_Atomic void* state = p->state;
	f(state);
	free(params);
	return NULL;
}

struct ReloadAndExecParams {
	void* handle;
	void* state;
};

void* reload_and_exec(void* params) {
	struct ReloadAndExecParams p = *(struct ReloadAndExecParams*)params;
	void* handle = p.handle;
	void* state = p.state;
	do{
		if(atomic_load(&keep_exec_b)) continue;

		if(handle != NULL) {
			hr_before_reload_t before_reload = dlsym(handle,"hr_before_reload");
			if(before_reload) {
				INFO("Found and running 'hr_before_reload'");
				before_reload(state);
			}else {
				INFO("Did not find 'hr_before_reload'");
			}
			INFO("Waiting for thread to finish");
			break;
		}
	}while(true);

}

void* keep_exec(void* exec_path_) {
	char* exec_path = exec_path_;
	void* handle = NULL;

	void* state = malloc(0);

	pthread_t hr_main_thread;
	struct HrMainLoopParams p = {.fn_ptr = hr_main_loop_do_nun, .state = state};

	handle = dlopen(exec_path,RTLD_NOW);
	if(handle == NULL) FATAL("Could not open executable file as dynamic library %s",dlerror()); 


	size_t state_size = 0;
	size_t *state_size_ptr = dlsym(handle,"HR_STATE_SIZE");
	if(state_size_ptr == NULL) {
		INFO("HR_STATE_SIZE not found, assuming state size as 0"); 
	}else {
		INFO("HR_STATE_SIZE found, setting state size as %d",*state_size_ptr); 
		state_size = *state_size_ptr;
	}

	INFO("Trying to reallocate state of size %d",state_size);
	void* new_state = malloc(state_size);
	free(state);
	state = new_state;
	if(new_state == NULL) {
		FATAL("Reallocing state failed");
	}
	INFO("Successfully reallocated state");

	hr_setup_t setup_fn = dlsym(handle,"hr_setup");
	if(setup_fn == NULL) {
		INFO("No function named 'hr_setup' found. Filling state with 0 value");
	}else {
		INFO("Found and executing 'hr_setup'");
		setup_fn(state);
	}

	dlclose(handle);
	handle = NULL;

	do{
		if(atomic_load(&keep_exec_b)) continue;


		if(handle != NULL) {
			hr_before_reload_t before_reload = dlsym(handle,"hr_before_reload");
			if(before_reload) {
				INFO("Found and running 'hr_before_reload'");
				before_reload(state);
			}else {
				INFO("Did not find 'hr_before_reload'");
			}
			INFO("Waiting for thread to finish");
			pthread_cancel(hr_main_thread);
			pthread_join(hr_main_thread,NULL);

			dlclose(handle);
			handle = NULL;
		}

		handle = dlopen(exec_path,RTLD_NOW);
		if(handle == NULL) FATAL("Could not open executable file as dynamic library."); 
		atomic_store(&keep_exec_b, true);

		size_t state_size = 0;
		size_t *state_size_ptr = dlsym(handle,"HR_STATE_SIZE");
		if(state_size_ptr == NULL) {
			INFO("HR_STATE_SIZE not found, assuming state size as 0"); 
		}else {
			INFO("HR_STATE_SIZE found, setting state size as %d",*state_size_ptr); 
			state_size = *state_size_ptr;
		}

		INFO("Trying to reallocate state");
		void* new_state = malloc(state_size);
		memcpy(new_state, state,state_size);
		free(state);
		state = new_state;
		if(new_state == NULL) {
			FATAL("Reallocing state failed");
		}
		INFO("Successfully reallocated state");

		hr_main_t f = dlsym(handle,"hr_main");
		if(f == NULL) FATAL("Unable to find 'hr_main' in watched file.\n[HELP] 'hr_main' function is the entry point for hot reloading, Similar to the normal 'main' function");


		struct HrMainLoopParams *params = malloc(sizeof(struct HrMainLoopParams));
		params->fn_ptr = f;
		params->state = state;
		int status = pthread_create(&hr_main_thread,NULL,hr_main_loop,params);
		if(status != 0) FATAL("Unable to create hr main thread: %d\n",status); 
		
	}while(true);
	
	return NULL;
}

struct WARParams {
	char* fname;
	char* exec_loc;
	char* makefile_dir;
	int fd;
};

void* watch_and_auto_recompile(void* p) {
	struct WARParams* params = p;
	char* fname = params->fname;
	char* exec_loc = params->exec_loc;
	char* makefile_dir = params->makefile_dir;
	int fd = params->fd;
	size_t buf_size = sizeof(struct inotify_event) * 10;
	char buf[buf_size];
	struct inotify_event *event = {0};

	while (true) {
		ssize_t read_size = read(fd, buf, buf_size);
		if (read_size <= 0) {
			perror("read");
			break;
		}

		int recompile = 0;

		for (char *ptr = buf; ptr < buf + read_size; ptr += sizeof(struct inotify_event) + event->len) {
			event = (struct inotify_event *)ptr;

			if(strcmp(event->name,fname) != 0) continue;
			printf("Event mask %X\n", event->mask);
			recompile = 1;
			break;
		}

		if(!recompile) continue;
		INFO("Recompiling...");
		char* cmd_buf;
		asprintf(&cmd_buf,"cd %s && make",makefile_dir);
		INFO("running command %s\n",cmd_buf);
		system(cmd_buf);	
		free(cmd_buf);
		atomic_store(&keep_exec_b,false);
		INFO("Recomplication successful");
	}



}

/*
 * 
 * Usage: ./main file_to_watch executable_location [Makefile dir] 
 * 
 */
int main(int argc, char** argv) {

	// TODO check if file is shared module or not

	if(argc < 3) {
		FATAL("Usage: ./main file_to_watch exectuable_location [makefile dir]");
	}

	char *fpath = argv[1];
	char *fordir = strndup(fpath,strlen(fpath));
	char *forbase = strndup(fpath,strlen(fpath));

	char *exec_loc = argv[2]; // TODO: add validation

	char *makefile_dir = strndup(fpath,strlen(fpath));

	if(argc > 3) {
		free(makefile_dir);
		makefile_dir = argv[3]; // TODO: add validation
	}

	char *dirname_ = dirname(fordir);
	char *fname = basename(forbase);

	if(strcmp(fname,".") == 0) {
		FATAL("Cannot get file name from input '%s'",fpath);
	}

	if(strcmp(dirname_,".") == 0) {
		size_t size = 256;
		dirname_ = malloc(sizeof(char) * size);
		if(getcwd(dirname_,size) == NULL) FATAL("Unable to get cwd");	
	}

	INFO("Trying to watch over '%s/%s'",dirname_,fname);

	int fd = inotify_init(); 
	if(fd == -1) {
		FATAL("Something went wrong in inotify_init(): %d\n",errno);
		return 1;
	}

	int wd = inotify_add_watch(fd, dirname_, IN_MODIFY  | IN_MOVED_TO | IN_CLOSE_WRITE);
	if(wd == -1) {
		FATAL("Something went wrong in inotify_init(): %d\n",errno);
		return 1;
	}


	struct WARParams wp = {.fname = fname, .makefile_dir = makefile_dir, .exec_loc=exec_loc, .fd = fd};

	INFO("Watching '%s/%s'\n",dirname_,fname);
	pthread_t thread;
	int status = pthread_create(&thread,NULL,watch_and_auto_recompile,&wp);
	if(status != 0) FATAL("Error creating thread: %d",status);

	keep_exec(exec_loc);

	free(forbase);
	free(fordir);
}
