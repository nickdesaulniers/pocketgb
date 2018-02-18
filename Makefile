LOG_LEVEL ?= 0

SOURCES = \
	cpu.c \
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

default:
	cc $(SOURCES) $(FLAGS) -o pocketgb

disassembler: disassembler.c
	cc disassembler.c $(FLAGS) -o disassembler

clean:
	rm -f pocketgb *.o
