#ifndef AI_H
#define AI_H

#include "drone.h"
#include "coord.h"
#include "list.h"
#include "communication.h"

void assign_mission(Drone *drone, Coord target, const char *mission_id);
Drone *find_closest_idle_drone(Coord target);
void *ai_controller(void *arg);

#endif