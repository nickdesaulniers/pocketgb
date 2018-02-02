SOURCES = \
	cpu.c \
	mmu.c \
	lcd.c \
	window_list.c \
	main.c

FLAGS = \
	`pkg-config --cflags sdl2 --libs sdl2` \
	-fsanitize=address

default:
	cc $(SOURCES) $(FLAGS) -o pocketgb

clean:
	rm -f pocketgb *.o
