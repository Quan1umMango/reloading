all:
	gcc -o main main.c -ggdb -fsanitize=address -fno-omit-frame-pointer  
