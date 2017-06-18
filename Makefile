default:
	cc cpu.c main.c -o pocketgb

clean:
	rm -f pocketgb *.o
