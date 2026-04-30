# hr - Hot reloading module in C
> [!WARNING]
> This project is still in development

https://github.com/user-attachments/assets/da507856-0df3-48cc-be3e-52fe2f307e04

https://github.com/user-attachments/assets/6244dce8-5587-4f71-9ae3-520954349c8e

A (currently) bare-bones implementation of a hot-reloading system in C.

## Build
Clone this repo, ``cd`` into it and run ``make``

## Running examples
``cd`` into the ``examples/`` directory and run ``make`` initially. Then ``cd`` into the main directory and run:
```sh
    ./main ./examples/<example name> ./examples/<example name>.c ./example 
```
Then you can change and save the ``<example name>.c`` file and watch it auto reload.

## Usage
- Declare a constant named ``HR_STATE_SIZE`` (of type size_t or similar) and set it to the size of the state you want to save (optional, default value is 0)
- Define a function ``void hr_setup(void* state)``. This will be run only once, when the hot-reloading program first loads your program. The parameter is a pointer to the memory which holds the state. (optional, defaults to do nothing)
- Define a function ``void hr_main(void* state)``. This will be called every time your code is reloaded.
- Define a function ``void hr_before_recomp(void* state)``. This will be called before every recompilation (optional)
- Define a function ``void hr_before_reload(void* state)``. This will be called before every reload and after every recompilation (optional)

## TODOS
- [x] Raylib example
- [ ] Watch over directories
- [ ] Watch over multiple files
- [ ] Add option for custom build commands
- [ ] Better preview video
