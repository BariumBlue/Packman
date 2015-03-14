

#include "boilerplate.h"

SDL_Surface *screen;
SDL_Event event;

/* Load a new image from a filename */
SDL_Surface *load_image( char* filename ) 
{
   //The image that's loaded
   SDL_Surface* loadedImage = NULL;
    
   //The optimized image that will be used
   SDL_Surface* optimizedImage = NULL;
    
   //Load the image
   loadedImage = IMG_Load( filename );
    
   //If the image loaded
   if( loadedImage != NULL )
   {
      //Create an optimized image
      optimizedImage = SDL_DisplayFormat( loadedImage );
       
      //Free the old image
      SDL_FreeSurface( loadedImage );

      //If the image was optimized just fine
      if( optimizedImage != NULL )
      {
            //Map the color key
         Uint32 colorkey = SDL_MapRGB( optimizedImage->format, 0, 0xFF, 0xFF );

         //Set all pixels of color R 0, G 0xFF, B 0xFF to be transparent
         SDL_SetColorKey( optimizedImage, SDL_SRCCOLORKEY, colorkey );
      }
   }
    
   //Return the optimized image
   return optimizedImage;
}

   /* apply surface to either another surface or the screen */
void apply_surface( int x, int y, SDL_Surface* source, SDL_Surface* destination, SDL_Rect* clip )
{
   //Holds offsets
   SDL_Rect offset;
    
   //Get offsets
   offset.x = x;
   offset.y = y;
   
   //Blit
   SDL_BlitSurface( source, clip, destination, &offset );
}

/* initialize SDL and subsystems */
bool init()
{
      //Initialize all SDL subsystems
   if ( SDL_Init( SDL_INIT_EVERYTHING ) == -1 || TTF_Init()==-1 )
      return false;


   screen = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SDL_SWSURFACE );

      //Set up the screen
   if ( screen==NULL )
      return false;

      //if everything went fine
   return true;
 }
