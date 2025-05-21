#include "headers/survivor.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "headers/globals.h"
#include "headers/map.h"

Survivor *create_survivor(Coord *coord, char *info, struct tm *discovery_time) {
    Survivor *s = malloc(sizeof(Survivor));
    if (!s) return NULL;
    memset(s, 0, sizeof(Survivor));
    s->coord = *coord;
    memcpy(&s->discovery_time, discovery_time, sizeof(struct tm));
    strncpy(s->info, info, sizeof(s->info) - 1);
    s->info[sizeof(s->info) - 1] = '\0';
    s->status = WAITING;
    pthread_mutex_init(&s->lock, NULL);
    return s;
}

void *survivor_generator(void *args) {
    (void)args;
    time_t t;
    struct tm discovery_time;
    srand(time(NULL));
    printf("\n=== Survivor Generator Started ===\n");
    printf("Map dimensions: %dx%d\n", map.width, map.height);
    printf("Survivors list address: %p\n", (void*)survivors);
    printf("Survivors list capacity: %d\n", survivors->capacity);
    printf("Survivors list current elements: %d\n", survivors->number_of_elements);

    while (running) {
        printf("\n=== Generating new survivor ===\n");
        // Create coordinates within map bounds
        Coord coord = {
            .x = rand() % map.width,
            .y = rand() % map.height
        };
        
        // Generate unique survivor ID
        char info[25];
        snprintf(info, sizeof(info), "SURV-%04d", rand() % 10000);
        printf("Generated survivor ID: %s at position (%d,%d)\n", info, coord.x, coord.y);
        
        // Get current time
        time(&t);
        localtime_r(&t, &discovery_time);

        // Create survivor
        printf("Creating survivor object...\n");
        Survivor *s = create_survivor(&coord, info, &discovery_time);
        if (!s) {
            printf("Failed to create survivor\n");
            continue;
        }
        printf("Survivor object created at %p\n", (void*)s);
        printf("Survivor status: %d\n", s->status);
        printf("Survivor coordinates: (%d,%d)\n", s->coord.x, s->coord.y);

        // Add to global survivors list
        printf("Adding survivor to global list...\n");
        pthread_mutex_lock(&survivors->lock);
        printf("Global list locked, current count: %d\n", survivors->number_of_elements);
        printf("Global list head before add: %p\n", (void*)survivors->head);
        Node *node = survivors->add(survivors, s);
        printf("Add operation completed, new count: %d\n", survivors->number_of_elements);
        printf("Global list head after add: %p\n", (void*)survivors->head);
        printf("Added node address: %p\n", (void*)node);
        pthread_mutex_unlock(&survivors->lock);
        
        if (!node) {
            printf("Failed to add survivor to global list\n");
            free(s);
            continue;
        }
        printf("Successfully added to global list at node %p\n", (void*)node);

        // Add to map cell's survivors list
        printf("Adding survivor to map cell [%d][%d]...\n", coord.y, coord.x);
        pthread_mutex_lock(&map.cells[coord.y][coord.x].survivors->lock);
        printf("Map cell list locked, current count: %d\n", 
               map.cells[coord.y][coord.x].survivors->number_of_elements);
        node = map.cells[coord.y][coord.x].survivors->add(map.cells[coord.y][coord.x].survivors, s);
        printf("Add operation completed, new count: %d\n", 
               map.cells[coord.y][coord.x].survivors->number_of_elements);
        pthread_mutex_unlock(&map.cells[coord.y][coord.x].survivors->lock);

        if (!node) {
            printf("Failed to add survivor to map cell\n");
            pthread_mutex_lock(&survivors->lock);
            survivors->removedata(survivors, s);
            pthread_mutex_unlock(&survivors->lock);
            free(s);
            continue;
        }
        printf("Successfully added to map cell at node %p\n", (void*)node);

        printf("Successfully created new survivor at (%d,%d): %s\n", coord.x, coord.y, info);
        
        // Sleep for 2-4 seconds before generating next survivor
        int sleep_time = rand() % 3 + 2;
        printf("Sleeping for %d seconds before next survivor...\n\n", sleep_time);
        sleep(sleep_time);
    }
    printf("Survivor generator thread exiting\n");
    return NULL;
}

void survivor_cleanup(Survivor *s) {
    if (!s) return;
    
    pthread_mutex_lock(&map.cells[s->coord.x][s->coord.y].survivors->lock);
    map.cells[s->coord.x][s->coord.y].survivors->removedata(map.cells[s->coord.x][s->coord.y].survivors, s);
    pthread_mutex_unlock(&map.cells[s->coord.x][s->coord.y].survivors->lock);
    
    pthread_mutex_destroy(&s->lock);
    free(s);
}