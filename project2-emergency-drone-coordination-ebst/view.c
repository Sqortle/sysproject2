#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "headers/view.h"
#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"
#include "headers/globals.h"
#include <pthread.h>
#include <time.h>

#define CELL_SIZE 30  // Make cells bigger

// SDL variables
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Event event;
int window_width, window_height;

// Thread safety
pthread_mutex_t view_mutex = PTHREAD_MUTEX_INITIALIZER;

// Colors - Making them MUCH more visible
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color RED = {255, 0, 0, 255};       // Pure bright red
const SDL_Color GREEN = {0, 255, 0, 255};     // Pure bright green
const SDL_Color BLUE = {0, 0, 255, 255};      // Pure bright blue
const SDL_Color YELLOW = {255, 255, 0, 255};  // Pure bright yellow
const SDL_Color GRID_COLOR = {128, 128, 128, 255}; // Brighter grid

void draw_cell(int x, int y, SDL_Color color) {
    if (!renderer) {
        printf("Cannot draw cell: renderer is NULL\n");
        return;
    }

    // Make cells MUCH bigger and more visible
    SDL_Rect rect = {
        .x = x * CELL_SIZE + 2,         // Add padding
        .y = y * CELL_SIZE + 2,         // Add padding
        .w = CELL_SIZE - 4,             // Slightly smaller for visibility
        .h = CELL_SIZE - 4              // Slightly smaller for visibility
    };
    
    // Draw the colored cell
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
    
    // Draw thick white border
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);
    
    // Draw multiple lines for thicker border
    for (int i = 0; i < 2; i++) {
        SDL_Rect border = {
            .x = rect.x - i,
            .y = rect.y - i,
            .w = rect.w + (2 * i),
            .h = rect.h + (2 * i)
        };
        SDL_RenderDrawRect(renderer, &border);
    }
}

void draw_grid() {
    if (!renderer) {
        printf("Cannot draw grid: renderer is NULL\n");
        return;
    }

    printf("Drawing grid: map dimensions %dx%d, window dimensions %dx%d\n", 
           map.width, map.height, window_width, window_height);
    
    SDL_SetRenderDrawColor(renderer, GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, GRID_COLOR.a);
    
    // Draw vertical lines
    for (int x = 0; x <= map.width; x++) {
        printf("Drawing vertical line at x=%d (screen_x=%d)\n", x, x * CELL_SIZE);
        SDL_RenderDrawLine(renderer, x * CELL_SIZE, 0, x * CELL_SIZE, window_height);
    }
    
    // Draw horizontal lines
    for (int y = 0; y <= map.height; y++) {
        printf("Drawing horizontal line at y=%d (screen_y=%d)\n", y, y * CELL_SIZE);
        SDL_RenderDrawLine(renderer, 0, y * CELL_SIZE, window_width, y * CELL_SIZE);
    }
    
    printf("Grid drawing complete\n");
}

void draw_drones() {
    if (!renderer) {
        printf("Cannot draw drones: renderer is NULL\n");
        return;
    }
    
    printf("\n=== Drawing Drones ===\n");
    
    // First, collect all drone data under lock
    typedef struct {
        Coord coord;
        Coord target;
        int status;
    } DroneSnapshot;
    
    DroneSnapshot* snapshots = NULL;
    int count = 0;
    
    // Lock the drones list for the entire duration of data collection
    pthread_mutex_lock(&drones->lock);
    
    // Create a local copy of all drones
    Node* node = drones->head;
    while (node != NULL) {
        snapshots = realloc(snapshots, (count + 1) * sizeof(DroneSnapshot));
        if (!snapshots) {
            printf("Failed to allocate memory for drone snapshot\n");
            pthread_mutex_unlock(&drones->lock);
            return;
        }
        
        Drone* d = (Drone*)node->data;
        snapshots[count].coord = d->coord;
        snapshots[count].target = d->target;
        snapshots[count].status = d->status;
        count++;
        node = node->next;
    }
    
    // Release the lock after collecting all data
    pthread_mutex_unlock(&drones->lock);
    
    printf("Found %d drones\n", count);
    
    // Now draw without holding any locks
    printf("\nDrawing all drones...\n");
    for (int i = 0; i < count; i++) {
        // Draw drone with appropriate color
        SDL_Color color = (snapshots[i].status == IDLE) ? BLUE : GREEN;
        draw_cell(snapshots[i].coord.x, snapshots[i].coord.y, color);
        
        // Draw mission line if on mission
        if (snapshots[i].status == ON_MISSION) {
            SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, GREEN.a);
            SDL_RenderDrawLine(
                renderer,
                snapshots[i].coord.x * CELL_SIZE + CELL_SIZE / 2,
                snapshots[i].coord.y * CELL_SIZE + CELL_SIZE / 2,
                snapshots[i].target.x * CELL_SIZE + CELL_SIZE / 2,
                snapshots[i].target.y * CELL_SIZE + CELL_SIZE / 2
            );
        }
    }
    
    free(snapshots);
    printf("Finished drawing drones\n");
    printf("=== Drones Complete ===\n\n");
}

void draw_survivors() {
    if (!renderer) {
        printf("Cannot draw survivors: renderer is NULL\n");
        return;
    }
    
    printf("\n=== Drawing Survivors ===\n");
    printf("Renderer status: %p\n", (void*)renderer);
    
    // First, collect all survivor data under lock
    typedef struct {
        Coord coord;
        int status;
    } SurvivorSnapshot;
    
    SurvivorSnapshot* snapshots = NULL;
    int count = 0;
    
    // Get waiting survivors
    printf("\nCollecting waiting survivors...\n");
    printf("Survivors list address: %p\n", (void*)survivors);
    
    // Lock the survivors list for the entire duration of data collection
    pthread_mutex_lock(&survivors->lock);
    
    // Create a local copy of all survivors
    Node* node = survivors->head;
    while (node != NULL) {
        snapshots = realloc(snapshots, (count + 1) * sizeof(SurvivorSnapshot));
        if (!snapshots) {
            printf("Failed to allocate memory for survivor snapshot\n");
            pthread_mutex_unlock(&survivors->lock);
            return;
        }
        
        Survivor* s = (Survivor*)node->data;
        snapshots[count].coord = s->coord;
        snapshots[count].status = s->status;
        count++;
        node = node->next;
    }
    
    // Release the lock after collecting all data
    pthread_mutex_unlock(&survivors->lock);
    
    printf("Found %d survivors\n", count);
    
    // Now draw without holding any locks
    printf("\nDrawing all survivors...\n");
    for (int i = 0; i < count; i++) {
        SDL_Color color;
        if (snapshots[i].status == WAITING) {
            color = RED;
            printf("Drawing waiting survivor at (%d,%d) in RED\n", 
                   snapshots[i].coord.x, snapshots[i].coord.y);
        } else if (snapshots[i].status == ASSIGNED) {
            color = YELLOW;
            printf("Drawing assigned survivor at (%d,%d) in YELLOW\n", 
                   snapshots[i].coord.x, snapshots[i].coord.y);
        } else { // HELPED
            color = GREEN;
            printf("Drawing helped survivor at (%d,%d) in GREEN\n", 
                   snapshots[i].coord.x, snapshots[i].coord.y);
        }
        
        draw_cell(snapshots[i].coord.x, snapshots[i].coord.y, color);
    }
    
    free(snapshots);
    printf("Finished drawing survivors\n");
    printf("=== Survivors Complete ===\n\n");
}

int draw_map() {
    if (!renderer) {
        printf("Cannot draw map: renderer is NULL\n");
        return 1;
    }
    
    printf("\n=== Drawing Map Frame ===\n");
    
    // Clear the screen with black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (SDL_RenderClear(renderer) != 0) {
        printf("Failed to clear renderer: %s\n", SDL_GetError());
        return 1;
    }
    printf("Screen cleared with black\n");
    
    // Draw grid first
    printf("Drawing grid...\n");
    draw_grid();
    printf("Grid drawn\n");
    
    // Draw survivors and drones
    printf("\n=== Drawing Game Objects ===\n");
    printf("Drawing survivors...\n");
    draw_survivors();
    printf("Survivors drawn\n");
    
    printf("Drawing drones...\n");
    draw_drones();
    printf("Drones drawn\n");
    
    // Present the final frame only once at the end
    printf("Presenting final frame...\n");
    SDL_RenderPresent(renderer);
    printf("Frame presented\n");
    
    return 0;
}

// Add test survivors at fixed positions for debugging
void add_test_survivors() {
    printf("Adding test survivors...\n");
    
    // Add survivors in each corner and center
    Survivor *s1 = malloc(sizeof(Survivor));
    s1->coord.x = 1;
    s1->coord.y = 1;
    s1->status = WAITING;
    snprintf(s1->info, sizeof(s1->info), "M%d", 1);
    pthread_mutex_init(&s1->lock, NULL);
    survivors->add(survivors, s1);
    printf("Added test survivor at (1,1) with ID %s\n", s1->info);

    Survivor *s2 = malloc(sizeof(Survivor));
    s2->coord.x = map.width - 2;
    s2->coord.y = 1;
    s2->status = WAITING;
    snprintf(s2->info, sizeof(s2->info), "M%d", 2);
    pthread_mutex_init(&s2->lock, NULL);
    survivors->add(survivors, s2);
    printf("Added test survivor at (%d,1) with ID %s\n", map.width - 2, s2->info);

    Survivor *s3 = malloc(sizeof(Survivor));
    s3->coord.x = 1;
    s3->coord.y = map.height - 2;
    s3->status = WAITING;
    snprintf(s3->info, sizeof(s3->info), "M%d", 3);
    pthread_mutex_init(&s3->lock, NULL);
    survivors->add(survivors, s3);
    printf("Added test survivor at (1,%d) with ID %s\n", map.height - 2, s3->info);

    Survivor *s4 = malloc(sizeof(Survivor));
    s4->coord.x = map.width - 2;
    s4->coord.y = map.height - 2;
    s4->status = WAITING;
    snprintf(s4->info, sizeof(s4->info), "M%d", 4);
    pthread_mutex_init(&s4->lock, NULL);
    survivors->add(survivors, s4);
    printf("Added test survivor at (%d,%d) with ID %s\n", map.width - 2, map.height - 2, s4->info);

    Survivor *s5 = malloc(sizeof(Survivor));
    s5->coord.x = map.width / 2;
    s5->coord.y = map.height / 2;
    s5->status = WAITING;
    snprintf(s5->info, sizeof(s5->info), "M%d", 5);
    pthread_mutex_init(&s5->lock, NULL);
    survivors->add(survivors, s5);
    printf("Added test survivor at center (%d,%d) with ID %s\n", map.width / 2, map.height / 2, s5->info);

    printf("Finished adding test survivors\n");
}

int init_sdl_window() {
    printf("Starting SDL initialization...\n");
    
    // Initialize SDL with video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL video subsystem initialized\n");
    
    // Calculate window dimensions based on map size
    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;
    printf("Initial window dimensions: %dx%d\n", window_width, window_height);
    
    // Set up SDL hints for better rendering
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    printf("SDL hints set for optimal rendering\n");
    
    printf("Creating window...\n");
    window = SDL_CreateWindow("Drone Coordination System",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            window_width, window_height,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created successfully\n");

    printf("Creating renderer...\n");
    renderer = SDL_CreateRenderer(window, -1, 
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Renderer created successfully\n");

    // Clear the renderer to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (SDL_RenderClear(renderer) != 0) {
        printf("Failed to clear renderer: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderPresent(renderer);

    // Handle high DPI displays
    int render_width, render_height;
    SDL_GetRendererOutputSize(renderer, &render_width, &render_height);
    float scale_x = (float)render_width / window_width;
    float scale_y = (float)render_height / window_height;
    printf("Set render scale to %f based on display DPI %f\n", scale_x, scale_x * 96.0);
    SDL_RenderSetScale(renderer, scale_x, scale_y);

    // Set blend mode for transparency
    printf("Setting blend mode...\n");
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) != 0) {
        printf("Failed to set blend mode: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Blend mode set successfully\n");
    
    // Add test survivors
    printf("Adding test survivors...\n");
    add_test_survivors();
    printf("Test survivors added successfully\n");
    
    // Perform initial render
    printf("Performing initial render...\n");
    if (draw_map() != 0) {
        printf("Initial render failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Initial render completed\n");
    
    printf("SDL initialization complete\n");
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

