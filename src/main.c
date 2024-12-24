#include "common.h"
#include "sdl_config.h"
#include "chip8.h"
#include "emulator.h"

int main(int argc, char** argv) 
{
    // Default usage message for args
    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s <rom_path>", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize emulator config
    config_t config = {0};
    if(!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);
    
    // Initialize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

    // Initialize CHIP8 machine
    chip8_t chip8 = {0};
    const char* rom_name = argv[1];
    if(!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

    // Initial Screen Clear
    clear_screen(sdl, config);

    // Set seed for rand()
    srand(time(NULL));

    // Emulator loop
    while (chip8.state != QUIT)
    {
        // Handle user input
        handle_input(&chip8);

        if(chip8.state == PAUSED) continue;

        // get_time()
        const uint64_t start = SDL_GetPerformanceCounter();

        // Emulate CHIP8 Instructions for this Emulator "Frame"
        // Number of instructions per frame = number of instructions per second / frame rate
        for(uint32_t i = 0; i < (config.instructions_per_second) / 60; i++)
        {
            emulate_instruction(&chip8, config);
        }

        // time elapsed since get_time()
        const uint64_t end = SDL_GetPerformanceCounter();
        
        // Delay for 60fps (approximatley)
        const uint64_t time_elapsed = (double) ((end - start) * 1000) / (SDL_GetPerformanceCounter());
        // SDL_Delay(16 - time elapsed)
        SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0);

        // Update the screen with changes
        update_screen(sdl, config, chip8);
        update_timers(&chip8);
    }

    // Cleanup and Exit
    final_cleanup(sdl);
    exit(EXIT_SUCCESS);
}