CC = clang
CFLAGS = -Wall -Wextra -O2

SDL_FLAGS = $(shell sdl2-config --cflags --libs)

TARGET = cpu

SRC = mos6502.c
GPU_SRC = gpu.c
FONT_SRC = font.c
SDL_SRC = main_sdl.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

sdl:
	$(CC) $(CFLAGS) \
		$(SRC) \
		$(GPU_SRC) \
		$(FONT_SRC) \
		$(SDL_SRC) \
		-o emu \
		$(SDL_FLAGS)

clean:
	rm -f $(TARGET) emu test_runner

test:
	$(CC) $(CFLAGS) \
		tests/test_cpu.c \
		mos6502.c \
		libs/Unity/src/unity.c \
		-Ilibs/Unity/src \
		-I. \
		-o test_runner

	./test_runner

run: all
	./$(TARGET)

.PHONY: all clean test run sdl
