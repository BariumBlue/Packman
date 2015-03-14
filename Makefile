#Files to compiles
FILES = boilerplate.cpp packman.cpp

#Executeable name
EXE_NAME = Packman

#COMPILER_FLAGS
COMPILER_FLAGS = -g -Wno-write-strings

#LINKER_FLAGS
LINKER_FLAGS = -lSDL -lSDL_image -lSDL_ttf -lSDL_mixer

#CC specifies which compiler we're using
CC = g++

#This is the target that compiles our executable
all : $(FILES)
	$(CC) $(FILES) -o $(EXE_NAME) $(COMPILER_FLAGS) $(LINKER_FLAGS)
