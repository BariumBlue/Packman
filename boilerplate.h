#ifndef BOILERPLATE_H
#define BOILERPLATE_H

#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_ttf.h"
#include "SDL/SDL_mixer.h"
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 1000;
const int SCREEN_BPP = 32;

bool init();   /* initialize SDL and subsystems */
SDL_Surface *load_image( char* filename );   /* Load a new image from a filename */
   /* apply surface to either another surface or the screen */
void apply_surface( int x, int y, SDL_Surface* source, SDL_Surface* destination, SDL_Rect* clip = NULL );

#endif


/*
 * Notes:
 *
 *  Use to compile: g++ -o program source.cpp -lSDL -lSDL_image -lSDL_ttf -lSDL_mixer 
 *
 */