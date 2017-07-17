default:
	cc cpu.c mmu.c lcd.c main.c `pkg-config --cflags sdl2 --libs sdl2` -o pocketgb

clean:
	rm -f pocketgb *.o
