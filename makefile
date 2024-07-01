CC=gcc
CFLAGS= -g -O0 -std=c99 -Wall -Werror -Wpedantic
LDFLAGS= -lm -lportaudio -lSDL2
PROJECT_NAME=audio_visualizer

build:
	$(CC) $(CFLAGS) $(LDFLAGS) main.c $(SOURCE_FILES) -o $(PROJECT_NAME)

clean:
	rm -f $(PROJECT_NAME)
