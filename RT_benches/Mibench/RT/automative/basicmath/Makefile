FILE1 = basicmath.c   rad2deg.c  cubic.c   isqrt.c
all: basicmath
basicmath: ${FILE1} Makefile
	g++ -static -O3 ${FILE1} -o basicmath -lm -pthread -w
clean:
	rm -rf basicmath *~
