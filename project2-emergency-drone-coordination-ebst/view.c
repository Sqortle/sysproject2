#include <SDL2/SDL.h>
#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"
#include "headers/view.h"
#include "headers/globals.h"
#include <stdio.h>
#include <pthread.h>

#define CELL_SIZE 20

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Event event;
int window_width, window_height;
int running = 1;  // Add running flag

// Thread safety
pthread_mutex_t view_mutex = PTHREAD_MUTEX_INITIALIZER;

const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color YELLOW = {255, 255, 0, 255};  // For assigned survivors
const SDL_Color WHITE = {255, 255, 255, 255};

int init_sdl_window() {
    printf("Initializing SDL window with dimensions: %dx%d\n", map.width * CELL_SIZE, map.height * CELL_SIZE);
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized successfully\n");

    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;

    window = SDL_CreateWindow("Drone Coordination System",
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            window_width, window_height,
                            SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("SDL window created successfully\n");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("SDL renderer created successfully\n");

    return 0;
}

void draw_cell(int x, int y, SDL_Color color) {
    SDL_Rect rect = {
        .x = x * CELL_SIZE,
        .y = y * CELL_SIZE,
                     .w = CELL_SIZE,
        .h = CELL_SIZE
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void draw_drones() {
    pthread_mutex_lock(&view_mutex);
    pthread_mutex_lock(&drones->lock);
    Node *node = drones->head;
    int count = 0;
    while (node != NULL) {
        Drone *d = (Drone *)node->data;
        pthread_mutex_lock(&d->lock);
        printf("Drawing drone at (%d, %d) with status %d\n", d->coord.x, d->coord.y, d->status);
        // Draw drone with appropriate color
        SDL_Color color = (d->status == IDLE) ? BLUE : GREEN;
        draw_cell(d->coord.x, d->coord.y, color);
        
        // Draw mission line if on mission
        if (d->status == ON_MISSION) {
            SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, GREEN.a);
            SDL_RenderDrawLine(
                renderer,
                d->coord.y * CELL_SIZE + CELL_SIZE / 2,
                d->coord.x * CELL_SIZE + CELL_SIZE / 2,
                d->target.y * CELL_SIZE + CELL_SIZE / 2,
                d->target.x * CELL_SIZE + CELL_SIZE / 2);
        }
        
        pthread_mutex_unlock(&d->lock);
        node = node->next;
        count++;
    }
    pthread_mutex_unlock(&drones->lock);
    printf("Drew %d drones\n", count);
    pthread_mutex_unlock(&view_mutex);
}

void draw_survivors() {
    pthread_mutex_lock(&view_mutex);
    pthread_mutex_lock(&survivors->lock);
    Node *node = survivors->head;
    int count = 0;
    while (node != NULL) {
        Survivor *s = (Survivor *)node->data;
        pthread_mutex_lock(&s->lock);
        printf("Drawing survivor at (%d, %d) with status %d\n", s->coord.x, s->coord.y, s->status);
        // Draw survivor with appropriate color
        SDL_Color color = (s->status == WAITING) ? RED : YELLOW;
        draw_cell(s->coord.x, s->coord.y, color);
        pthread_mutex_unlock(&s->lock);
        node = node->next;
        count++;
    }
    pthread_mutex_unlock(&survivors->lock);
    printf("Drew %d survivors\n", count);

    // Draw helped survivors
    pthread_mutex_lock(&helpedsurvivors->lock);
    node = helpedsurvivors->head;
    count = 0;
    while (node != NULL) {
        Survivor *s = (Survivor *)node->data;
        pthread_mutex_lock(&s->lock);
        printf("Drawing helped survivor at (%d, %d)\n", s->coord.x, s->coord.y);
        draw_cell(s->coord.x, s->coord.y, YELLOW);
        pthread_mutex_unlock(&s->lock);
        count++;
        node = node->next;
    }
    printf("Drew %d helped survivors\n", count);
    pthread_mutex_unlock(&helpedsurvivors->lock);
    pthread_mutex_unlock(&view_mutex);
}

void draw_grid() {
    if (!renderer) {
        printf("Renderer is NULL!\n");
        return;
    }
    printf("Drawing grid: map.width=%d, map.height=%d, window_width=%d, window_height=%d\n", map.width, map.height, window_width, window_height);
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);
    for (int i = 0; i <= map.height; i++) {
        int y = i * CELL_SIZE;
        printf("Horizontal line at y=%d\n", y);
        SDL_RenderDrawLine(renderer, 0, y, window_width, y);
    }
    for (int j = 0; j <= map.width; j++) {
        int x = j * CELL_SIZE;
        printf("Vertical line at x=%d\n", x);
        SDL_RenderDrawLine(renderer, x, 0, x, window_height);
    }
}

int draw_map() {
    printf("draw_map: window_width=%d, window_height=%d, map.width=%d, map.height=%d\n", window_width, window_height, map.width, map.height);
    printf("Drawing map...\n");
    
    // Handle SDL events
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = 0;
            return 1;
        }
    }
    
    // Clear the screen with black
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
    SDL_RenderClear(renderer);
    printf("Screen cleared with black\n");
    
    // Draw grid first
    printf("Drawing grid...\n");
    draw_grid();
    
    // Draw survivors
    printf("Drawing survivors...\n");
    draw_survivors();
    
    // Draw drones
    printf("Drawing drones...\n");
    draw_drones();
    
    // Present the rendered frame
    SDL_RenderPresent(renderer);
    printf("Frame presented\n");
    
    return 0;
}

void cleanup_sdl() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}

void *view_thread_func(void *arg) {
    (void)arg;
    printf("View thread started\n");
    while (running) {
        if (draw_map() != 0) {
            break;
        }
        SDL_Delay(100);  // Cap at 10 FPS
    }
    printf("View thread ending\n");
    cleanup_sdl();
    return NULL;
}