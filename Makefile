default:
	cc cpu.c mmu.c main.c -o pocketgb

clean:
	rm -f pocketgb *.o
