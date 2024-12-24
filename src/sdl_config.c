#include "sdl_config.h"

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

void update_screen(sdl_t sdl, config_t config, chip8_t chip8)
{
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    // Grab bg and fg colours to draw:
    uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    uint8_t bg_b = (config.bg_color >> 8) & 0xFF;
    uint8_t bg_a = (config.bg_color >> 0) & 0xFF;

    uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    uint8_t fg_b = (config.fg_color >> 8) & 0xFF;
    uint8_t fg_a = (config.fg_color >> 0) & 0xFF;

    // Loop through the pixels, draw a rectangle for each pixel to the SDL Window
    for(uint32_t i = 0; i < sizeof(chip8.display); i++)
    {
        // X = i % window_width
        // Y = floor(i / window_width)
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if(chip8.display[i])
        {
            // If pixel is on, draw the foreground color
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            // Drawing pixel outlines
            if(config.pixel_outlines)
            {
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);    
            }
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderDrawRect(sdl.renderer, &rect);
        } else {
            // Else, draw the background color
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    // Updating the background color updates the backbuffer, not the screen. To update the screen, use the RenderPresent function.
    SDL_RenderPresent(sdl.renderer);
}

// Handle user input
void handle_input(chip8_t* chip8)
{
    SDL_Event event;

    while(SDL_PollEvent(&event))
    {
        switch(event.type) 
        {
            case SDL_QUIT:
                // Exit window & end program
                chip8->state = QUIT; // Will exit main emulator loop
                return;
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        // Escape key, exit main emulator loop
                        chip8->state = QUIT;
                        return;
                    case SDLK_SPACE:
                        if(chip8->state == RUNNING)
                        {
                            chip8->state = PAUSED;
                            puts("==== PAUSED ====");
                        } else {
                            chip8->state = RUNNING;
                        }
                        return;
                    
                    case SDLK_1: chip8->keypad[0x1] = true; break; // 1
                    case SDLK_2: chip8->keypad[0x2] = true; break; // 2
                    case SDLK_3: chip8->keypad[0x3] = true; break; // 3
                    case SDLK_4: chip8->keypad[0xC] = true; break; // C
                    case SDLK_q: chip8->keypad[0x4] = true; break; // 4
                    case SDLK_w: chip8->keypad[0x5] = true; break; // 5
                    case SDLK_e: chip8->keypad[0x6] = true; break; // 6
                    case SDLK_r: chip8->keypad[0xC] = true; break; // D
                    case SDLK_a: chip8->keypad[0x7] = true; break; // 7
                    case SDLK_s: chip8->keypad[0x8] = true; break; // 8
                    case SDLK_d: chip8->keypad[0x9] = true; break; // 9
                    case SDLK_f: chip8->keypad[0xE] = true; break; // E
                    case SDLK_z: chip8->keypad[0xA] = true; break; // A
                    case SDLK_x: chip8->keypad[0x0] = true; break; // 0
                    case SDLK_c: chip8->keypad[0xB] = true; break; // B
                    case SDLK_v: chip8->keypad[0xF] = true; break; // F
                    default: break;
                }
                break;
            case SDL_KEYUP:
                switch(event.key.keysym.sym)
                {
                    case SDLK_1: chip8->keypad[0x1] = false; break; // 1
                    case SDLK_2: chip8->keypad[0x2] = false; break; // 2
                    case SDLK_3: chip8->keypad[0x3] = false; break; // 3
                    case SDLK_4: chip8->keypad[0xC] = false; break; // C
                    case SDLK_q: chip8->keypad[0x4] = false; break; // 4
                    case SDLK_w: chip8->keypad[0x5] = false; break; // 5
                    case SDLK_e: chip8->keypad[0x6] = false; break; // 6
                    case SDLK_r: chip8->keypad[0xC] = false; break; // D
                    case SDLK_a: chip8->keypad[0x7] = false; break; // 7
                    case SDLK_s: chip8->keypad[0x8] = false; break; // 8
                    case SDLK_d: chip8->keypad[0x9] = false; break; // 9
                    case SDLK_f: chip8->keypad[0xE] = false; break; // E
                    case SDLK_z: chip8->keypad[0xA] = false; break; // A
                    case SDLK_x: chip8->keypad[0x0] = false; break; // 0
                    case SDLK_c: chip8->keypad[0xB] = false; break; // B
                    case SDLK_v: chip8->keypad[0xF] = false; break; // F
                    default: break;
                }
                break;
            default:
                break;
        }
    }
}