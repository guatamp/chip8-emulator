#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"

typedef struct 
{
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

typedef struct
{
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_color; // Foreground color 32 bit RGBA8888
    uint32_t bg_color; // Background color 32 bit RGBA8888
    uint32_t scale_factor; // Amount to scale a chip8 pixel by
 
} config_t;

bool set_config_from_args(config_t *config, const int argc, char** argv)
{
    // Set Defaults: 32x64 default
    *config = (config_t) {
        .window_width = 64,
        .window_height = 32,
        .fg_color = 0xFFFFFFFF, // White
        .bg_color = 0xFFFF00FF, // Yellow
        .scale_factor = 20, // Default res of 64x32 times 20 = 1280x640
    };

    // Override Defaults from args
    for(int i = 1; i < argc; i++)
    {
        (void) argv[i];
    }

    return true;
}

bool init_sdl(sdl_t *sdl, const config_t config) 
{
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) 
    {
        SDL_Log("Could not initialize SDL subsystem %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("CHIP8 EMULATOR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                    config.window_width * config.scale_factor, 
                                    config.window_height * config.scale_factor,
                                    0);

    if(!sdl->window)
    {
        SDL_Log("Could not create SDL window, %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);

    if(!sdl->renderer) {
        SDL_Log("Could not create Renderer, %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void final_cleanup(const sdl_t sdl)
{
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit(); // Clean up all initialized subsystems
}

// Clear Screen / Set Window color to background color
void clear_screen(const sdl_t sdl, const config_t config)
{
    uint8_t r = (config.bg_color >> 24) & 0xFF;
    uint8_t g = (config.bg_color >> 16) & 0xFF;
    uint8_t b = (config.bg_color >> 8) & 0xFF;
    uint8_t a = (config.bg_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void update_screen(sdl_t sdl)
{
    // Updating the background color updates the backbuffer, not the screen. To update the screen, use the RenderPresent function.
    SDL_RenderPresent(sdl.renderer);
}

int main(int argc, char** argv) 
{
    // Initialize emulator config
    config_t config = {0};
    if(!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);
    
    // Initialize SDL
    sdl_t sdl = {0};
    if(!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

    // Initial Screen Clear
    clear_screen(sdl, config);

    // Emulator loop
    while (true)
    {
        // get_time()
        // Emulate CHIP8 Instructions
        // time elapsed since get_time()

        // Delay for 60fps (approximatley)
        // SDL_Delay(16 - time elapsed)
        SDL_Delay(16);

        // Update the screen with changes
        update_screen(sdl);
    }

    // Cleanup and Exit
    final_cleanup(sdl);
    exit(EXIT_SUCCESS);
}