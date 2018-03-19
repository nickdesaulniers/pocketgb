LOG_LEVEL ?= 0

SOURCES = \
	cpu2.c \
	mmu.c \
	lcd.c \
	window_list.c \
	main.c \

FLAGS = \
	-DLOG_LEVEL=$(LOG_LEVEL) \
	-Wall \
	-Werror \
	-Wextra \
	-g \
	-O3 \
	`pkg-config --cflags sdl2 --libs sdl2` \
	-fsanitize=address \
	-flto=thin \

default:
	$(CC) $(SOURCES) $(FLAGS) -o pocketgb

disassembler: disassembler.c
	$(CC) disassembler.c $(FLAGS) -o disassembler

clean:
	rm -f pocketgb *.o
