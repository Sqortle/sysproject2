# Drone Coordination System

This project implements a drone coordination system for emergency response scenarios. The system manages multiple drones to locate and assist survivors in a given area.

## Repository Information

- Original Repository: https://github.com/Sqortle/sysproject2
- Current Working Directory: `/Users/mirza/Desktop/Code Projects/sysproject2`

## Project Structure

- `server.c`: Main server implementation
- `drone_client.c`: Drone client implementation
- `view.c`: Visualization system using SDL2
- `ai.c`: AI controller for drone coordination
- `headers/`: Header files for the project

## Building and Running

1. Compile the server:
```bash
make server
```

2. Compile the drone client:
```bash
make drone_client
```

3. Run the server:
```bash
./server
```

4. Run drone clients:
```bash
./drone_client
```

## Dependencies

- SDL2
- json-c
- pthread

## License

This project is part of a systems programming course implementation. 