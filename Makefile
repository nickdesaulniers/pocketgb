LOG_LEVEL ?= 0

SOURCES = \
	cpu.c \
	mmu.c \
	lcd.c \
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

format:
	clang-format-4.0 -style=Chromium -i cpu2.c
