LOG_LEVEL ?= 0

SOURCES = \
	cpu.c \
	mmu.c \
	lcd.c \
	timer.c \
	main.c \

FLAGS = \
	-DLOG_LEVEL=$(LOG_LEVEL) \
	-Wall \
	-Werror \
	-Wextra \
	-g \
	-O3 \
	-march=native \
	`pkg-config --cflags sdl2 --libs sdl2` \
	-fsanitize=address \
	-flto=thin \

default:
	$(CC) $(SOURCES) $(FLAGS) -o pocketgb

disassembler: disassembler.c
	$(CC) disassembler.c $(FLAGS) -o disassembler

clean:
	rm -f pocketgb disassembler *.o

format:
	clang-format-4.0 -style=Chromium -i *.c

test: default
	./pocketgb tests/cpu_instrs/individual/01* || echo
	./pocketgb tests/cpu_instrs/individual/03* || echo
	./pocketgb tests/cpu_instrs/individual/04* || echo
	./pocketgb tests/cpu_instrs/individual/05* || echo
	./pocketgb tests/cpu_instrs/individual/06* || echo
	./pocketgb tests/cpu_instrs/individual/07* || echo
	./pocketgb tests/cpu_instrs/individual/08* || echo
	./pocketgb tests/cpu_instrs/individual/09* || echo
	./pocketgb tests/cpu_instrs/individual/10* || echo
	./pocketgb tests/cpu_instrs/individual/11* || echo
