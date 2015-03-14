/*
 * Packman, chasing after packets, avoiding the tracers
 *
 *
 *    For entity direction, values are as below
 *         1
 *       2 P 3
 *        4
 *               where 0 is not moving at all
 *
 *
 *  For rendering walls, Tile frames are the sum of it's walled neighbors, as below:
 *
 *            1
 *          2 # 4
 *            8
 *
 *            so a tile with walled tiles below and to the right has number
 *                4 + 8, or 12
 *
 */

#include <unistd.h>

#include "boilerplate.h"

#define PLAYER_SPEED 2
#define ENEMY_SPEED 1
#define SNITCH_SPEED 1

int deaths_to_lose = 3;

/* graphics */
SDL_Surface *background = NULL;
SDL_Surface *player_image;
SDL_Surface *enemy_image;
SDL_Surface *snitch_image;
SDL_Surface *packet;
SDL_Surface *powered_player_image;
TTF_Font *font = NULL;
SDL_Color textColor = { 255, 255, 255 };
SDL_Color poweredColor = {0, 255, 255};
SDL_Surface *redtile = NULL;
SDL_Surface *bluetile = NULL;

struct Tile
{
   char type;

      /* for enemy AI */
   int ent_val;
      /* the last enemy that looked at the tile */
   struct entity *last;
      /* the total value for a square */
   int tvalue;

   bool occupied;
};


   /* linked list of entities */
struct entity
{
   char type;

   int x;
   int y;

   int origx;
   int origy;

   char direction;

   SDL_Surface *image;

   struct entity *next;
};

extern SDL_Surface *screen;
extern SDL_Event event;

/* level stuff */
int width;
int height;

struct entity *entity_list = NULL;

Tile* game_field = NULL;


/* other stuff */
#define INT_MAX 2147483647

int previous_dir=0;

int packets = 0;
int losses = 0;
int has_won = 0;
int level = 0;
bool renderpaths = false;

/* function prototypes */
int interact(entity *ent);
void set_value(int tilex, int tiley, entity *ent, int value, int override);
int follow_value(entity *ent_ptr, int distance);
int update_boardvalues();
int cleanuplvl();
int winlvl();


    //release all held data
void clean_up()
{
   cleanuplvl();

   SDL_FreeSurface(redtile);
   SDL_FreeSurface(bluetile);
   SDL_FreeSurface(packet);
   SDL_FreeSurface(player_image);
   SDL_FreeSurface(enemy_image);
   SDL_FreeSurface(snitch_image);
   SDL_FreeSurface(screen);
   TTF_CloseFont( font );
    
   SDL_Quit();
}

/* /boilerplate.cpp */




 //load relevant files
bool load_files()
{
    font = TTF_OpenFont("assets/arial.ttf",18); //font

    if ((packet = load_image("assets/packet.png"))==NULL){
       printf("Could not find 'assets/packet.png'");
       return false;
    }

    if (!font){
        printf("\nCould not find font\n");
        return false;
    }

    bluetile = load_image("assets/bluetile.png");   //delete me
    redtile = load_image("assets/redtile.png");

   if (redtile==NULL || bluetile==NULL){
      printf("\ntiles did not load\n");
      return false;
   }

    return true;
}

 /* put in a new entity and return it */
struct entity* new_entity()
{

   /* if list is empty */
   if (entity_list == NULL)
   {
      entity_list = (entity *) calloc(1, sizeof(entity));
      return entity_list;
   }

      /* find the end */
   entity *ent_ptr = entity_list;

   while (ent_ptr->next!=NULL)
      ent_ptr = ent_ptr->next;

   ent_ptr->next = (entity *) calloc(1, sizeof(entity));

   return ent_ptr->next;
}


   /* Load the level, render it, add entities */
int load_lvl(char* lvl_file, char* walltile_file, char* background_file)
{
   SDL_Surface *walltiles;
   FILE *lvlptr;

      /* load files */
   if ((background = load_image( "assets/background.png")) == NULL){
      printf("\nbackground image not found\n");
      return 0;}
   if ((walltiles = load_image( (char *) walltile_file))==NULL){
      printf("\nwalltile image not found");
      return 0;}
   if ((lvlptr = fopen(lvl_file, "r"))==NULL){
      printf("\nlvl file %s image not found", lvl_file);
      return 0;}

      /* load entity sprites */
   enemy_image = TTF_RenderText_Solid( font, "E", textColor );
   player_image = TTF_RenderText_Solid( font, "P", textColor );
   powered_player_image = TTF_RenderText_Solid( font, "P", poweredColor);
   snitch_image = TTF_RenderText_Solid( font, "*", textColor );

      /* get dimension then get level layout */

   fscanf(lvlptr,"%dx%d\n",&width,&height);

   if (width==0 || height==0){
      printf("\nMust include level dimensions\n");
      return 0;
   }


   int x=0;
   int y=1;
   char ttype;
   Tile *tileptr;

   game_field = (Tile*) calloc(width*height, sizeof(Tile));
   tileptr = game_field;

   while ((ttype=fgetc(lvlptr))!=EOF)
   {

      if (ttype=='\n'){
         if (x!=width){
            printf("\nBad level layout, a row is %d long\n",x);
            return 0;}
         x=0;
         y++;
      }
      else{
         tileptr->type=ttype;
         tileptr++;
         x++;
      }
   }

   if (y!=height){
      printf("\ny dimension does not match, y is %d\n",y);
      return 0;}

   /* render the game board and init entities */

   SDL_Rect clip;
   clip.w=16;
   clip.h=16;

   for (y=0;y<height;y++)
   {
      for (x=0;x<width;x++)
      {

         if (game_field[ y*width + x ].type == '#')
         {
            int frame =
               (y-1>=0 && game_field[ (y-1)*width + x ].type == '#')
               +2*(x-1>=0 && game_field[ y*width + x-1 ].type == '#' )
               +4*(x+1<width && game_field[ y*width + x+1 ].type == '#')
               +8*(y+1<height && game_field[ (y+1)*width + x ].type == '#');

            clip.x = 16*(frame%4);
            clip.y = 16*(frame/4);

            apply_surface(x*16, y*16, walltiles, background, &clip);
         }

         else if (game_field[ y*width + x ].type == 'E') //if the current tile is an enemy
         {
            game_field[ y*width + x ].type = 'o';
            packets++;

            entity *new_ent = new_entity();

            new_ent->type = 'E';
            new_ent->x = new_ent->origx = x*16;
            new_ent->y = new_ent->origy = y*16;
            new_ent->image = enemy_image;
         }
         else if (game_field[ y*width + x ].type == 'P') //if the current tile is a player
         {
            entity *new_ent = new_entity();

            new_ent->type = 'P';
            new_ent->x = new_ent->origx = x*16;
            new_ent->y = new_ent->origy = y*16;
            new_ent->image = player_image;
         }
         else if (game_field[ y*width + x ].type == '*' )   //a snitch
         {
            game_field[ y*width + x ].type = 'o';
            packets++;

            entity *new_ent = new_entity();

            new_ent->type = '*';
            new_ent->x = new_ent->origx = x*16;
            new_ent->y = new_ent->origy = y*16;
            new_ent->image = snitch_image;
         }
         else if (game_field[ y*width + x ].type == 'o') //a packet
            packets++;
      }
   }

   SDL_FreeSurface(walltiles);
   fclose(lvlptr);

   return 1;
}

   /* get rid of the old level stuff */
int cleanuplvl()
{
   free(game_field);
   while (entity_list!=NULL){
      entity *temp = entity_list;
      entity_list = entity_list->next;
      free(temp);
   }
   SDL_FreeSurface(background);

   game_field = NULL;
   entity_list = NULL;
   background = NULL;
}

/* when you die */
void player_death()
{
      /* print death message */
   printf("You have just died. - You have died %d times so far.",losses);
   char text[256];
   snprintf(text,256,"You have just died.\nYou have died %d times so far.",losses);
   SDL_Surface *banner = TTF_RenderText_Solid( font, text, textColor );
   apply_surface((SCREEN_WIDTH-banner->w)/2, (SCREEN_HEIGHT-banner->h)/2,banner,screen);
   snprintf(text,256,"Positions reset in 3 secs. You have %d lives left", deaths_to_lose-losses);
   SDL_Surface *banner2 = TTF_RenderText_Solid( font, text, textColor );
   apply_surface((SCREEN_WIDTH-banner->w)/2, (SCREEN_HEIGHT-banner->h)/2+20,banner2,screen);
   SDL_Flip( screen );
   SDL_FreeSurface(banner);
   SDL_FreeSurface(banner2);

      /* reset player, enemy positions */
   entity **reset_ent = &entity_list;
   while (*reset_ent!=NULL){
      (*reset_ent)->x = (*reset_ent)->origx;
      (*reset_ent)->y = (*reset_ent)->origy;
      reset_ent = &((*reset_ent)->next);
   }

   SDL_Delay(3000);
}


/* display all non-static tiles, namely packets */
int display_tiles()
{
   for (int x=0;x<width;x++)
   {
      for (int y=0; y<height;y++)
      {
         if (game_field[y*width+x].type=='o')
            apply_surface(x*16,y*16,packet,screen);
      }
   }

   return 1;
}

int display_entities()
{

   entity *ent_ptr = entity_list;

   while (ent_ptr!=NULL)
   {
      apply_surface(ent_ptr->x, ent_ptr->y, ent_ptr->image, screen);
      ent_ptr = ent_ptr->next;
   }

   return 1;
}

   /* display the ent_val of each tile, graphically */
int display_tilevalues()
{

   update_boardvalues();

   int alpha;

   for (int x=0; x<width; x++)
   {
      for (int y=0; y<height; y++)
      {
         if (game_field[y*width+x].type=='#')
            continue;

            /* to view pathes  */
         alpha = SDL_ALPHA_TRANSPARENT + game_field[y*width+x].tvalue/20000 + 64;

         alpha = (alpha<SDL_ALPHA_OPAQUE) ? alpha : SDL_ALPHA_OPAQUE;
         alpha = (alpha>SDL_ALPHA_TRANSPARENT) ? alpha : SDL_ALPHA_TRANSPARENT;

         SDL_SetAlpha( redtile, SDL_SRCALPHA, alpha );
         apply_surface( x*16, y*16, bluetile, screen );
         apply_surface( x*16, y*16, redtile, screen );
      }
   }
}

   /* the expense value of where to goto  */
int follow_value(entity *ent_ptr, int distance)
{
   if (ent_ptr->type=='E')
      return -6000000/(std::max(distance,1)*std::max(distance,1));
   else if (ent_ptr->type=='*')
      return 0;
   return 15000000/std::max(distance,1);
}

/*
 *   resets the distance values for enemy pathfinding
 *      only necessary when there's only one enemy
 */
void reset_path()
{
   for (int x=0; x<width; x++)
   {
      for (int y=0; y<height; y++)
      {
         if (game_field[y*width+x].type=='#')
            continue;

         game_field[y*width+x].last = NULL;
         game_field[y*width+x].tvalue = 0;
      }
   }

}

   /* update total values for tiles, basically only for viewing with display_tilevalues */
int update_boardvalues()
{
   entity *ent_ptr = entity_list;

   reset_path(); //only necessary if there's only one enemy

         /* set distance values on the board, then set follow values  */
   while (ent_ptr!=NULL)
   {

      int ent_x = ent_ptr->x/16;
      int ent_y = ent_ptr->y/16;

      game_field[ent_y*width + ent_x].last = ent_ptr;
      game_field[ent_y*width + ent_x].ent_val = 0;

      set_value(ent_x,ent_y-1,ent_ptr,1,1);
      set_value(ent_x-1,ent_y,ent_ptr,1,1);
      set_value(ent_x+1,ent_y,ent_ptr,1,1);
      set_value(ent_x,ent_y+1,ent_ptr,1,1);

         /* update the tvalue for every tile*/
      for (int x=0; x<width; x++)
      {
         for (int y=0; y<height; y++)
         {
            if (game_field[y*width+x].type=='#')
               continue;
   
            entity *ent_ptr = game_field[y*width+x].last;
   
            if (ent_ptr==NULL)
               continue;
   
            if (ent_ptr==entity_list)
               game_field[y*width+x].tvalue=0;
   
            game_field[y*width+x].tvalue += follow_value(ent_ptr, game_field[y*width+x].ent_val);
         }
      }

      ent_ptr=ent_ptr->next;
   }

}

/*
 *   If a valid tile, 
 *      and if tile has a smaller recorded distance to ent
 *         set distance to smaller, set_value to surronding
 *   else do nothing
 *
 *      override is to override 
 *
 */
void set_value(int tilex, int tiley, entity *ent, int value, int override)
{
   // printf("{%d,%d,%d,%d}, ",tilex,tiley,value,game_field[tiley*width + tilex].ent_val);

    // override=1;

   if (tilex>=0 && tilex<width && tiley>=0 && tiley<height 
      && game_field[tiley*width + tilex].type!='#' 
      && (game_field[tiley*width + tilex].last!=ent 
         || value<game_field[tiley*width + tilex].ent_val)
      && (!game_field[tiley*width+tilex].occupied || value>2|| override))
   {
      // printf(" tile was {%d,{%d,%d},%p},\n",game_field[tiley*width + tilex].ent_val,tilex,tiley,game_field[tiley*width + tilex].last);

      game_field[tiley*width + tilex].last = ent;
      game_field[tiley*width + tilex].ent_val = value;

      // printf(" is now {%d,%p}\n",game_field[tiley*width + tilex].ent_val,game_field[tiley*width + tilex].last);

      set_value(tilex,tiley-1,ent,value+1,0);
      set_value(tilex-1,tiley,ent,value+1,0);
      set_value(tilex+1,tiley,ent,value+1,0);
      set_value(tilex,tiley+1,ent,value+1,0);
   }
}



   
/* For enemies. Calculate best path to take.
 *
 * if there are only two ways to go [forward & backward], continue along path
 *
 *   cycle through all other active entities:
 *      for every entity, go through tiles and calculate min distance to it for each path
 */
int calc_path(entity *ent)
{
   int x=ent->x/16;
   int y=ent->y/16;

      /* continue along path */
   if (((game_field[(y-1)*width+x].type!='#')
      +(game_field[y*width+x-1].type!='#')
      +(game_field[y*width+x+1].type!='#')
      +(game_field[(y+1)*width+x].type!='#'))==2)
   {
      if (ent->direction!=4 && (game_field[(y-1)*width+x].type!='#')){
         ent->direction=1;}
      else if (ent->direction!=3 && (game_field[y*width+x-1].type!='#')){
         ent->direction=2;}
      else if (ent->direction!=2 && (game_field[y*width+x+1].type!='#')){
         ent->direction=3;}
      else if (ent->direction!=1 && game_field[(y+1)*width+x].type!='#'){
         ent->direction=4;}

      return 1;
   }

      /* for every entity, calculate distance. Then add it's inverse distance to
         total move value */

   entity *ent_ptr = entity_list;

         //the total move value for tiles in pos 1,2,3,4. 
   int v0=0,v1=0,v2=0,v3=0;

   // reset_path(); //only necessary if there's only one enemy

   while (ent_ptr!=NULL)
   {


      if (ent_ptr==ent){
         ent_ptr=ent_ptr->next;
         continue;
      }

      int ent_x = ent_ptr->x/16;
      int ent_y = ent_ptr->y/16;

      game_field[ent_y*width + ent_x].last = ent_ptr;
      game_field[ent_y*width + ent_x].ent_val = 0;

      set_value(ent_x,ent_y-1,ent_ptr,1,1);
      set_value(ent_x-1,ent_y,ent_ptr,1,1);
      set_value(ent_x+1,ent_y,ent_ptr,1,1);
      set_value(ent_x,ent_y+1,ent_ptr,1,1);

      if (game_field[(y-1)*width+x].type!='#'){
         v0 += follow_value(ent_ptr, game_field[(y-1)*width+x].ent_val);
      }
      if (game_field[y*width+x-1].type!='#'){
         v1 += follow_value(ent_ptr, game_field[y*width+x-1].ent_val);
      }
      if (game_field[y*width+x+1].type!='#'){
         v2 += follow_value(ent_ptr, game_field[y*width+x+1].ent_val);
      }
      if (game_field[(y+1)*width+x].type!='#'){
         v3 += follow_value(ent_ptr, game_field[(y+1)*width+x].ent_val);
      }

      ent_ptr=ent_ptr->next;
   }
   if (game_field[(y-1)*width+x].type=='#')
      v0 = -INT_MAX;
   if (game_field[y*width+x-1].type=='#')
      v1 = -INT_MAX;
   if (game_field[y*width+x+1].type=='#')
      v2 = -INT_MAX;
   if (game_field[(y+1)*width+x].type=='#')
      v3 = -INT_MAX;

   // printf("{%d,%d,%d,%d}:",v0,v1,v2,v3);

   if (ent->type=='E'){
      if (v0>=v1 && v0>=v2 && v0>=v3)
         ent->direction=1;
      else if (v1>=v0 && v1>=v2 && v1>=v3)
         ent->direction=2;
      else if (v2>=v0 && v2>=v1 && v2>=v3)
         ent->direction=3;
      else if (v3>=v0 && v3>=v1 && v3>=v2)
         ent->direction=4;
   }
   if (ent->type=='*'){
      v0 = (v0==-INT_MAX)? INT_MAX : v0;
      v1 = (v1==-INT_MAX)? INT_MAX : v1;
      v2 = (v2==-INT_MAX)? INT_MAX : v2;
      v3 = (v3==-INT_MAX)? INT_MAX : v3;
      // printf("choosing dir for snitch");
      if (v0<=v1 && v0<=v2 && v0<=v3)
         ent->direction=1;
      else if (v0!=-INT_MAX && v1<=v0 && v1<=v2 && v1<=v3)
         ent->direction=2;
      else if (v0!=-INT_MAX && v2<=v0 && v2<=v1 && v2<=v3)
         ent->direction=3;
      else if (v0!=-INT_MAX && v3<=v0 && v3<=v1 && v3<=v2)
         ent->direction=4;
   }

   // printf("%d\n",ent->direction);
}



/*    For entity direction, values are as below
 *         1
 *       2 P 3
 *         4
 *               where 0 is not moving at all
 */
int choosedir(entity *ent_ptr)
{

   if (ent_ptr->x%16!=0 || ent_ptr->y%16!=0){
      printf("\nbad arg to choosedir\n");
      ent_ptr->direction = 0;
      return 0;
   }

   int x = ent_ptr->x/16;
   int y = ent_ptr->y/16;

   if (ent_ptr->type=='P')   //TODO: replace previous_dir with plain old ->direction
   {

        Uint8 *keystates = SDL_GetKeyState( NULL );

           /*if 2 keys down*/
      if (previous_dir && ((keystates[ SDLK_UP ])+(keystates[ SDLK_LEFT ])+(keystates[ SDLK_RIGHT ])
         +(keystates[ SDLK_DOWN ])>1))
      {
         if (keystates[ SDLK_UP ] && previous_dir!=1 && game_field[(y-1)*width+x].type!='#')
            ent_ptr->direction = 1;
         else if (keystates[ SDLK_LEFT ] && previous_dir!=2 && game_field[y*width+x-1].type!='#')
            ent_ptr->direction = 2;
         else if (keystates[ SDLK_RIGHT ] && previous_dir!=3 && game_field[y*width+x+1].type!='#')
            ent_ptr->direction = 3;
         else if (keystates[ SDLK_DOWN ] && previous_dir!=4 && game_field[(y+1)*width+x].type!='#')
            ent_ptr->direction = 4;
         else
            ent_ptr->direction = 0;

      }
      else
      {
         if (keystates[ SDLK_UP ] && game_field[(y-1)*width+x].type!='#')
            ent_ptr->direction = 1;
         else if (keystates[ SDLK_LEFT ] && game_field[y*width+x-1].type!='#')
            ent_ptr->direction = 2;
         else if (keystates[ SDLK_RIGHT ] && game_field[y*width+x+1].type!='#')
            ent_ptr->direction = 3;
         else if (keystates[ SDLK_DOWN ] && game_field[(y+1)*width+x].type!='#')
            ent_ptr->direction = 4;
         else
            ent_ptr->direction = 0;
      }
      previous_dir = ent_ptr->direction;
   }
   else
   {
      calc_path(ent_ptr);
   }

   return 1;
}

int move_entity(unsigned int distance, entity *ent_ptr)
{

   if (distance == 0)
      return 1;

      //tile location
   int x = (ent_ptr->x)/16;
   int y = (ent_ptr->y)/16;
   int lowx = (ent_ptr->x-1)/16;
   int lowy = (ent_ptr->y-1)/16;

   if (x<0 || x>=width || y<0 || y>=height){
      printf(".");
      return 0;
   }

   if (ent_ptr->x%16==0 && ent_ptr->y%16==0)
   {
      choosedir(ent_ptr);
   }

      //move
   if (ent_ptr->direction==1){
      if ( lowy*16 >= ent_ptr->y - distance)
      {
         distance -= (ent_ptr->y - lowy*16);
         ent_ptr->y = 16*lowy;
         interact(ent_ptr);
         game_field[(lowy+1)*width+x].occupied=0;
         move_entity(distance, ent_ptr);
      }
      else
         ent_ptr->y-=distance;
   }
   else if (ent_ptr->direction==2){\
      if (  lowx*16 >= ent_ptr->x - distance )
      {
         distance -= (ent_ptr->x - lowx*16);
         ent_ptr->x = 16*lowx;
         interact(ent_ptr);
         game_field[y*width+lowx+1].occupied=0;
         move_entity(distance, ent_ptr);
      }
      else
         ent_ptr->x-=distance;
   }
   else if (ent_ptr->direction==3){
      if ( (x+1)*16 <= ent_ptr->x + distance)
      {
         distance -= ((x+1)*16 - ent_ptr->x);
         ent_ptr->x = 16*(x+1);
         interact(ent_ptr);
         game_field[y*width+x].occupied=0;
         move_entity(distance, ent_ptr);
      }
      else
         ent_ptr->x+=distance;
   }
   else if (ent_ptr->direction==4){
      if ( (y+1)*16 <= ent_ptr->y + distance)
      {
         distance -= ((y+1)*16 - ent_ptr->y);
         ent_ptr->y = 16*(y+1);
         interact(ent_ptr);
         game_field[y*width+x].occupied=0;
         move_entity(distance, ent_ptr);
      }
      else{
         ent_ptr->y+=distance;
      }
   }
}

/*
 * Move entities until they reach a tile, then move in new direction
 *
 */
int move_entities(unsigned int dtime)
{
   entity *ent_ptr = entity_list;

   while (ent_ptr!=NULL)
   {

      if (ent_ptr->type=='P'){
         move_entity(dtime*PLAYER_SPEED,ent_ptr);
      }
      else if (ent_ptr->type=='E'){
         move_entity(dtime*ENEMY_SPEED,ent_ptr);
      }
      else if (ent_ptr->type=='*'){
         move_entity(dtime*SNITCH_SPEED,ent_ptr);
      }
      ent_ptr = ent_ptr->next;
   }

   return 1;
}

/*
 *   If an entity goes onto a tile and interacts with any entities on tht tile
 */

int interact(entity *ent)
{

   if (ent->type=='E')
   {
      entity *ent_ptr = entity_list;

      int x = ent->x/16;
      int y = ent->y/16;

      if (game_field[y*width+x].occupied)
      {
         while (ent_ptr!=NULL)
         {
            if (ent_ptr->type=='P' 
               && ent_ptr->x/16+(ent_ptr->direction==2 && ent_ptr->x%16!=0)==x 
               && ent_ptr->y/16+(ent_ptr->direction==1 && ent_ptr->y%16!=0)==y){
               losses+=1;
               printf("YOU LOST %d times\n",losses);
               player_death();
            }
   
            ent_ptr = ent_ptr->next;
         }
      }

      game_field[y*width+x].occupied = true;
   }
   else if (ent->type=='P')
   {

      int x = ent->x/16;
      int y = ent->y/16;

      entity *ent_ptr = entity_list;

      if (ent->x%16!=0 || ent->y%16!=0 || x<0 || x>=width || y<0 || y>=height){
         printf("\nbad values to interact\n");
         return 0;
      }
   
      if (game_field[y*width + x].type=='o'){
         packets--;
         game_field[y*width + x].type='_';
      }

      if (game_field[y*width+x].occupied)
      {
         while (ent_ptr!=NULL)
         {
            if (ent_ptr->type=='E'                
               && ent_ptr->x/16+(ent_ptr->direction==2 && ent_ptr->x%16!=0)==x 
               && ent_ptr->y/16+(ent_ptr->direction==1 && ent_ptr->y%16!=0)==y){
               losses+=1;
               printf("YOU LOST %d times\n",losses);
               player_death();
            }
            else if (ent_ptr->type=='*'                
               && ent_ptr->x/16+(ent_ptr->direction==2 && ent_ptr->x%16!=0)==x 
               && ent_ptr->y/16+(ent_ptr->direction==1 && ent_ptr->y%16!=0)==y){
               printf("P interacted with *\n");
               has_won=1;
            }
   
            ent_ptr = ent_ptr->next;
         }
      }

      game_field[y*width+x].occupied = true;

   }
   else if (ent->type=='*')
   {
      entity *ent_ptr = entity_list;

      int x = ent->x/16;
      int y = ent->y/16;

      if (game_field[y*width+x].occupied)
      {
         while (ent_ptr!=NULL)
         {
            if (ent_ptr->type=='P' 
               && ent_ptr->x/16+(ent_ptr->direction==2 && ent_ptr->x%16!=0)==x 
               && ent_ptr->y/16+(ent_ptr->direction==1 && ent_ptr->y%16!=0)==y){
               printf("* interacted with P\n");
               has_won=1;
            }
   
            ent_ptr = ent_ptr->next;
         }
      }

      game_field[y*width+x].occupied = true;
   }

}


   /* render the game */
int render()
{
   apply_surface( 0, 0, background, screen );

   if (renderpaths)
      display_tilevalues();

   if (display_entities() ==0 || display_tiles() ==0)
      printf("bad display");

         //Update the screen
   if( SDL_Flip( screen ) == -1 )
      return 1;
}

/* When you win a level */
int winlvl(void)
{
   SDL_Surface *banner;
   char text[256];

   snprintf(text,256,"You've won level %d! You've died %d times so far",level,losses);

   banner = TTF_RenderText_Solid( font, text, textColor );

   if (display_entities() ==0 || display_tiles() ==0)
      printf("bad display");

   apply_surface( (
      SCREEN_WIDTH-
      banner->w)/2, (SCREEN_HEIGHT-banner->h)/2-32, banner, screen, NULL );

         //Update the screen
   if( SDL_Flip( screen ) == -1 )
      return 1;

   SDL_FreeSurface(banner);

   cleanuplvl();

   SDL_Delay(2500);

   level++;
   snprintf(text,256,"levels/level%d",level);

   /* if we've read every level there is */
   // if( access( text, F_OK ) == -1 ) {
   //    printf("Congratulations you've won, and with %d lives to go!", deaths_to_lose-losses);
   //    char text[256];
   //    snprintf(text,256,"Congratulations you've won, and with %d lives to go!", deaths_to_lose-losses);
   //    SDL_Surface *banner = TTF_RenderText_Solid( font, text, textColor );
   //    apply_surface((SCREEN_WIDTH-banner->w)/2, (SCREEN_HEIGHT-banner->h)/2-64,banner,screen);
   //    SDL_Flip( screen );
   //    SDL_FreeSurface(banner);
   //    SDL_Delay(6000);
   //    return 0;
   // }

   if (load_lvl(text,(char *)"assets/walls_small.png",(char *)"assets/background.png")==0)
      return 0;
   return 1;
}


//g++ packman.cpp -g -lSDL -lSDL_image -lSDL_ttf -lSDL_mixer -Wno-write-strings
int main( int argc, char* args[] )
{
   renderpaths = false;
   if (argc>=2){
      if (args[1][0] == 'v')
         renderpaths = true;
      else{
         printf("Packman: unrecognized argument. Only argument is 'v', for 'visualizations'\n");
         return 0;
      }
   }

   bool quit=false;

   if (init()==false){
      printf("\nSDL did not load correctly [????]\n");
      return 1;
   }

   if (load_files()==false){
      printf("\nCould not load files\n");
      return 1;
   }

   SDL_WM_SetCaption( "Packman, Saviour of the Universe", NULL );

   if ( !(load_lvl((char *)"levels/level0",(char *)"assets/walls_small.png",(char *)"assets/background.png")) ){
      printf("\nbad level load, quitting\n");
      return 1;
   }

      //make sure this has same factor as below it
   unsigned int last_frame = SDL_GetTicks()/20;

   while (quit==false)
   {
      //Wait .2 seconds
      SDL_Delay( 10 );

      move_entities(SDL_GetTicks()/20-last_frame);
      last_frame = SDL_GetTicks()/20;

      if (packets<=0 || has_won){
         has_won=0;
         packets=0;
         if (winlvl()==0){
            printf("\nbad level load, quitting\n");
            break;
         }
         last_frame = SDL_GetTicks()/20;
      }
      else if (losses>=deaths_to_lose){
        printf("You died %d times and lost.",losses);
        char text[32];
        snprintf(text,32,"You died %d times and lost.",losses);
        SDL_Surface *banner = TTF_RenderText_Solid( font, text, textColor );
        apply_surface((SCREEN_WIDTH-banner->w)/2, (SCREEN_HEIGHT-banner->h)/2-32,banner,screen);
        SDL_Flip( screen );
        SDL_FreeSurface(banner);
        SDL_Delay(6000);
        break;
      }

      if( SDL_Flip( screen ) == -1 )
         return 1;

      while (SDL_PollEvent( &event ))
      {
         if (event.type==SDL_QUIT)
            quit=true;
      }

      render();
   }

   clean_up();

}










