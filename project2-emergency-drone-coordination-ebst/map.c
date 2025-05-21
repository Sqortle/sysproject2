#include "headers/map.h"
#include "headers/list.h"
#include <stdlib.h>
#include <stdio.h>

// Global map instance (defined here, declared extern in map.h)
extern Map map;

void init_map(int height, int width) {
    printf("Initializing map with dimensions: height=%d, width=%d\n", height, width);
    map.height = height;
    map.width = width;

    // Allocate rows (height)
    printf("Allocating memory for %d rows...\n", height);
    map.cells = (MapCell**)malloc(sizeof(MapCell*) * height);
    if (!map.cells) {
        perror("Failed to allocate map rows");
        exit(EXIT_FAILURE);
    }
    printf("Successfully allocated memory for rows at %p\n", (void*)map.cells);

    // Allocate columns (width) for each row
    for (int i = 0; i < height; i++) {
        printf("Allocating memory for row %d (%d columns)...\n", i, width);
        map.cells[i] = (MapCell*)malloc(sizeof(MapCell) * width);
        if (!map.cells[i]) {
            perror("Failed to allocate map columns");
            exit(EXIT_FAILURE);
        }
        printf("Successfully allocated memory for row %d at %p\n", i, (void*)map.cells[i]);

        // Initialize each cell
        for (int j = 0; j < width; j++) {
            printf("Initializing cell [%d][%d]...\n", i, j);
            map.cells[i][j].coord.x = j;  // x is the column index
            map.cells[i][j].coord.y = i;  // y is the row index
            
            // Initialize survivors list for each cell
            printf("Creating survivors list for cell [%d][%d]...\n", i, j);
            map.cells[i][j].survivors = create_list(sizeof(Survivor), 10);
            if (!map.cells[i][j].survivors) {
                perror("Failed to create survivors list for cell");
                exit(EXIT_FAILURE);
            }
            printf("Successfully created survivors list for cell [%d][%d] at %p\n", i, j, (void*)map.cells[i][j].survivors);
        }
    }

    printf("Map initialization complete: %dx%d grid created\n", height, width);
}

void freemap() {
    for (int i = 0; i < map.height; i++) {
        for (int j = 0; j < map.width; j++) {
            if (map.cells[i][j].survivors) {
                map.cells[i][j].survivors->destroy(map.cells[i][j].survivors);
            }
        }
        free(map.cells[i]);
    }
    free(map.cells);
    printf("Map destroyed\n");
}