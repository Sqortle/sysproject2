#ifndef VIEW_H
#define VIEW_H
#include <SDL2/SDL.h>
#include "map.h"
#include "drone.h"
#include "survivor.h"
#include "globals.h"

// SDL variables
extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Event event;
extern int running;
extern int window_width;
extern int window_height;

// Thread safety
extern pthread_mutex_t view_mutex;

// Colors
extern const SDL_Color BLACK;
extern const SDL_Color WHITE;
extern const SDL_Color RED;
extern const SDL_Color GREEN;
extern const SDL_Color BLUE;
extern const SDL_Color YELLOW;

void draw_cell(int x, int y, SDL_Color color);
void draw_grid();
void draw_survivors();
void draw_drones();
int draw_map();
int init_sdl_window();
void cleanup_sdl();
void *view_thread_func(void *arg);

#endif