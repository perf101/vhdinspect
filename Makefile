all: parsehelper static-info same-content identical-sectors same-sector \
	 content-histogram

static-info: vhd.c static-info.c
	gcc -Wall -g -O2 -o static-info vhd.c static-info.c `pkg-config --cflags --libs glib-2.0`

identical-sectors: vhd.c identical-sectors.c
	gcc -Wall -g -O2 -o identical-sectors vhd.c identical-sectors.c `pkg-config --cflags --libs glib-2.0`

same-content: vhd.c same-content.c
	gcc -Wall -g -O2 -o same-content vhd.c same-content.c `pkg-config --cflags --libs glib-2.0`

content-histogram: vhd.c content-histogram.c
	gcc -Wall -g -O2 -o content-histogram vhd.c content-histogram.c `pkg-config --cflags --libs glib-2.0`

same-sector: vhd.c same-sector.c
	gcc -Wall -g -O2 -o same-sector vhd.c same-sector.c `pkg-config --cflags --libs glib-2.0`

parsehelper: parsehelper.c
	gcc -Wall -g -O2 -o parsehelper parsehelper.c

clean:
	rm -f parsehelper static-info same-content identical-sectors same-sector

.PHONY: clean
