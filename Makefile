SOURCES = \
	cpu.c \
	mmu.c \
	lcd.c \
	window_list.c \
	main.c

default:
	cc $(SOURCES) `pkg-config --cflags sdl2 --libs sdl2` -o pocketgb

clean:
	rm -f pocketgb *.o
