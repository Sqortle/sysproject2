#include "headers/globals.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/ai.h"
#include "headers/view.h"
#include <stdio.h>

int main() {
    // Initialize SDL first
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // Set up SDL hints before creating any windows
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    init_map(40, 30);
    survivors = create_list(sizeof(Survivor), 1000);
    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    drones = create_list(sizeof(Drone), 100);

    pthread_t survivor_thread;
    pthread_create(&survivor_thread, NULL, survivor_generator, NULL);

    pthread_t ai_thread;
    pthread_create(&ai_thread, NULL, ai_controller, NULL);

    // Initialize SDL window in the main thread
    if (init_sdl_window() != 0) {
        printf("Failed to initialize SDL window\n");
        freemap();
        survivors->destroy(survivors);
        helpedsurvivors->destroy(helpedsurvivors);
        drones->destroy(drones);
        SDL_Quit();
        return 1;
    }

    // Main loop for SDL events and drawing
    SDL_Event event;
    Uint32 lastDrawTime = SDL_GetTicks();
    const int TARGET_FPS = 60;
    const int FRAME_TIME = 1000 / TARGET_FPS;

    while (running) {
        // Handle SDL events in the main thread
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                printf("Received quit event\n");
                running = 0;
                break;
            }
        }

        // Update display at target FPS
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastDrawTime >= FRAME_TIME) {
            draw_map();
            lastDrawTime = currentTime;
        } else {
            // Sleep for a short time to prevent excessive CPU usage
            SDL_Delay(1);
        }
    }

    // Cleanup and exit
    freemap();
    survivors->destroy(survivors);
    helpedsurvivors->destroy(helpedsurvivors);
    drones->destroy(drones);
    cleanup_sdl();
    return 0;
}