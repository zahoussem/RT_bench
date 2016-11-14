FILE1 = qsort_small.c

all: qsort_small
qsort_small: qsort_small.c Makefile
	g++ -static qsort_small.c -O3 -o qsort_small -lm -pthread -w

clean:
	rm qsort_small *~
