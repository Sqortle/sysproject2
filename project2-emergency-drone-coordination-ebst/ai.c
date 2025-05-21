#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "headers/ai.h"
#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"
#include "headers/globals.h"

void assign_mission(Drone *drone, Coord target, const char *mission_id) {
    pthread_mutex_lock(&drone->lock);
    drone->target = target;
    drone->status = ON_MISSION;
    
    // Create mission assignment message
    struct json_object *mission = json_object_new_object();
    json_object_object_add(mission, "type", json_object_new_string("ASSIGN_MISSION"));
    json_object_object_add(mission, "mission_id", json_object_new_string(mission_id));
    json_object_object_add(mission, "priority", json_object_new_string("high"));
    
    // Add target coordinates
    struct json_object *target_obj = json_object_new_object();
    json_object_object_add(target_obj, "x", json_object_new_int(target.x));
    json_object_object_add(target_obj, "y", json_object_new_int(target.y));
    json_object_object_add(mission, "target", target_obj);
    
    // Add expiry and checksum
    json_object_object_add(mission, "expiry", json_object_new_int64(time(NULL) + 3600));
    json_object_object_add(mission, "checksum", json_object_new_string("a1b2c3"));
    
    // Send mission to drone
    send_json(drone->sock, mission);
    printf("Assigned mission %s to drone %d: target=(%d,%d)\n", 
           mission_id, drone->id, target.x, target.y);
    
    json_object_put(mission);
    pthread_mutex_unlock(&drone->lock);
}

Drone *find_closest_idle_drone(Coord target) {
    Drone *closest = NULL;
    int min_distance = INT_MAX;
    
    pthread_mutex_lock(&drones->lock);
    Node *node = drones->head;
    while (node != NULL) {
        Drone *d = (Drone *)node->data;
        pthread_mutex_lock(&d->lock);
        if (d->status == IDLE) {
            int dist = abs(d->coord.x - target.x) + abs(d->coord.y - target.y);
            if (dist < min_distance) {
                min_distance = dist;
                closest = d;
            }
        }
        pthread_mutex_unlock(&d->lock);
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);
    
    if (closest) {
        printf("Found closest idle drone at (%d,%d) for target (%d,%d)\n",
               closest->coord.x, closest->coord.y, target.x, target.y);
    } else {
        printf("No idle drones available for target (%d,%d)\n",
               target.x, target.y);
    }
    
    return closest;
}

void *ai_controller(void *arg) {
    while (running) {
        // First, get a survivor that needs help
        Survivor *survivor_to_help = NULL;
        Coord survivor_coord;
        char survivor_info[25];
        
        pthread_mutex_lock(&survivors->lock);
        Node *node = survivors->head;
        if (node) {
            Survivor *s = (Survivor *)node->data;
            pthread_mutex_lock(&s->lock);
            if (s->status == WAITING) {
                survivor_to_help = s;
                survivor_coord = s->coord;
                strncpy(survivor_info, s->info, sizeof(survivor_info));
                s->status = ASSIGNED;
            }
            pthread_mutex_unlock(&s->lock);
        }
        pthread_mutex_unlock(&survivors->lock);
        
        if (survivor_to_help) {
            // Then, find the closest idle drone
            Drone *closest_drone = find_closest_idle_drone(survivor_coord);
            
            // If we found a drone, assign the mission
            if (closest_drone) {
                assign_mission(closest_drone, survivor_coord, survivor_info);
                printf("Assigned drone to survivor at (%d, %d)\n", 
                       survivor_coord.x, survivor_coord.y);
            } else {
                // If no drone available, set survivor back to waiting
                pthread_mutex_lock(&survivor_to_help->lock);
                survivor_to_help->status = WAITING;
                pthread_mutex_unlock(&survivor_to_help->lock);
            }
        }
        
        // Sleep to prevent excessive CPU usage
        usleep(100000); // 100ms
    }
    return NULL;
}