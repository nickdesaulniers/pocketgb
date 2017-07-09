default:
	cc cpu.c mmu.c lcd.c main.c -o pocketgb

clean:
	rm -f pocketgb *.o
