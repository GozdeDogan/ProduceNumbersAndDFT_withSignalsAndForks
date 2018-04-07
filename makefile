all: multiprocess_DFT

multiprocess_DFT: 131044019_main.o
	gcc 131044019_main.o -o multiprocess_DFT -lm

131044019_main.o: 131044019_main.c
	gcc -c 131044019_main.c

clean:
	rm *.o multiprocess_DFT