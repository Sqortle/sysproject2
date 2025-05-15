#include "headers/globals.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/ai.h"
#include "headers/view.h"
#include <stdio.h>

int main() {
    init_map(40, 30);
    survivors = create_list(sizeof(Survivor), 1000);
    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    drones = create_list(sizeof(Drone), 100);

    pthread_t survivor_thread;
    pthread_create(&survivor_thread, NULL, survivor_generator, NULL);

    pthread_t ai_thread;
    pthread_create(&ai_thread, NULL, ai_controller, NULL);

    init_sdl_window();
    while (!check_events()) {
        draw_map();
        SDL_Delay(100);
    }

    freemap();
    survivors->destroy(survivors);
    helpedsurvivors->destroy(helpedsurvivors);
    drones->destroy(drones);
    quit_all();
    return 0;
}