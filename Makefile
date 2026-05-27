CC = clang
TARGET = cpu
SRC = mos6502.c

all:
	$(CC) $(SRC) -o $(TARGET)

clear:
	rm -f $(TARGET)

test: 
	gcc tests/test_cpu.c cpu/cpu.c libs/Unity/src/unity.c \
	-Ilibs/Unity/src -Icpu \
	-o test_runner


.PHONY: all clean
