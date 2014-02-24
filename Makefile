test: vhd.c test.c
	gcc -Wall -g -O2 -o test vhd.c test.c `pkg-config --cflags --libs glib-2.0`

parsehelper: parsehelper.c
	gcc -Wall -g -O2 -o parsehelper parsehelper.c
