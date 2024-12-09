#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"

// SDL Container Object
typedef struct 
{
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

// Config Container Object
typedef struct
{
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_color;       // Foreground color 32 bit RGBA8888
    uint32_t bg_color;       // Background color 32 bit RGBA8888
    uint32_t scale_factor;   // Amount to scale a chip8 pixel by
 
} config_t;

// Emulator State Object
typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

// CHIP8 Instruction Format
typedef struct {
    uint16_t opcode;
    uint16_t NNN;   // 12 bit address
    uint8_t NN;     // 8 bit constant
    uint8_t N;      // 4 bit constant
    uint8_t X;      // 4 bit register identifier
    uint8_t Y;      // 4 bit register identifier
} instruction_t;

// CHIP8 Machine Object
typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];
    bool display[64*32];
    uint16_t stack[12];       // Subroutine stack
    uint16_t* stack_pointer;  // Pointer to the top of the stack
    uint8_t V[16];            // Data Registers V0-VF
    uint16_t I;                // Index Register
    uint16_t PC;              // Program Counter
    uint8_t delay_timer;      // Decrements at 60Hz when > 0 
    uint8_t sound_timer;      // Decrements at 60Hz and plays a tone when > 0
    bool keypad[16];          // Hex keypad 0x0 - 0xF
    const char* rom_name;     // Currently running ROM
    instruction_t instruction; // Currently executing instruction
} chip8_t;

bool set_config_from_args(config_t *config, const int argc, char** argv)
{
    // Set Defaults: 32x64 default
    *config = (config_t) {
        .window_width = 64,
        .window_height = 32,
        .fg_color = 0xFFFFFFFF, // White
        .bg_color = 0x000000FF, // Black
        .scale_factor = 20,     // Default res of 64x32 times 20 = 1280x640
    };

    // Override Defaults from args, todo
    for(int i = 1; i < argc; i++)
    {
        (void) argv[i];
    }

    return true;
}

// Initialize a new CHIP8 machine
bool init_chip8(chip8_t *chip8, const char rom_name[])
{
    const uint32_t entry_point = 0x200; // CHIP8 ROMs will be loaded at 0x200, fonts loaded at 0x00
    const uint8_t font[] = 
    {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0 
        0x20, 0x60, 0x20, 0x20, 0x70, // 1 
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3 
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4 
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    // Load Font
    memcpy(&chip8->ram[0], font, sizeof(font));

    // Ooen ROM File
    FILE* rom = fopen(rom_name, "rb");
    if(!rom)
    {
        SDL_Log("ROM File %s is invalid or does not exist\n", rom_name);
        return false;
    }

    // Get & Check ROM Size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if(rom_size > max_size)
    {
        SDL_Log("ROM file %s is too large. ROM Size: %zu, Max Size: %zu", rom_name, rom_size, max_size);
    }

    // Load ROM into RAM
    if(fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1)
    {
        SDL_Log("Could not read ROM File %s into CHIP8 Memory", rom_name);
    }

    fclose(rom);

    // Set CHIP8 machine defaults
    chip8->state = RUNNING;
    chip8->PC = entry_point;
    chip8->rom_name = rom_name;
    chip8->stack_pointer = &chip8->stack[0];
    chip8->V[0xF] = 0; // Carry flag initialized to 0
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
                    default: 
                        break;
                }
                break;
            case SDL_KEYUP:
                break;
            default:
                break;
        }
    }
}

#ifdef DEBUG
    void print_debug_info(chip8_t *chip8) 
    {
        printf("Current PC Address: 0x%04X, Opcode: 0x%04X Description: ", chip8->PC - 2, chip8->instruction.opcode);
        switch((chip8->instruction.opcode >> 12) & 0x0F)
        {
            case 0x00:
                if(chip8->instruction.NN == 0xE0)
                {
                    //0x00E0: Clear the screen
                    printf("Clear Screen\n");
                } else if (chip8->instruction.NN == 0xEE) {
                    // 0x00EE: Return from a subroutine
                    // Set PC to last return address which was stored on the subroutine stack, and the pop it off
                    printf("Return from subroutine at address 0x%04X\n", *(chip8->stack_pointer - 1));
                } else {
                    printf("Unimplemented opcode\n");
                }
                break;
            case 0x02:
                // 0x2NNN: Call Subroutine at NNN:
                // Store the return point (PC, which has been incremented by 2 to avoid an infinite loop of calling the subroutine and returning to it)
                // Update the PC to NNN, which is where the subroutine is located
                printf("Call Subroutine located at 0x%04X\n", chip8->instruction.NNN);
                break;
            case 0x0A:
                printf("Set the Instruction Register I to 0x%04X\n", chip8->instruction.NNN);
                break;
            case 0x06:
                // 0x6XNN: Set Register Vx = NN
                printf("Set Register V%01X to 0x%02X\n", chip8->instruction.X, chip8->instruction.NN);
                break;
            case 0x0D:
                // 0xDXYN: Draw pixel Vx, Vy, heignt N and width 8
                printf("Drawing sprite with height N (%u), at coords V%X (%02X), V%X, (%02X), from memory location I (0x%04X)\n",
                        chip8->instruction.N, chip8->instruction.X, chip8->V[chip8->instruction.X], 
                                              chip8->instruction.Y, chip8->V[chip8->instruction.Y], chip8->I);
                break;
            case 0x07:
                // 0x7XNN: Vx += NN. Carry flag is not changed
                printf("Value NN (0x%02X) was added to register V%X, which had initial value 0x%02X\n",
                        chip8->instruction.NN, chip8->instruction.X, chip8->V[chip8->instruction.X]);
                break;
            default:
                printf("Unimplemented opcode\n");
                break; // Uninimplemented / invalid opcode
        }
    }
#endif

// Emulate 1 CHIP8 instruction
void emulate_instruction(chip8_t* chip8, const config_t config)
{
    (chip8->instruction).opcode = (chip8->ram[chip8->PC] << 8) | (chip8->ram[chip8->PC + 1]);
    chip8->PC += 2; // Increment PC for the next opcode

    // Fill out the instruction format
    chip8->instruction.NNN = chip8->instruction.opcode & 0x0FFF;
    chip8->instruction.NN = chip8->instruction.opcode & 0x0FF;
    chip8->instruction.N = chip8->instruction.opcode & 0x0F;
    chip8->instruction.X = (chip8->instruction.opcode >> 8) & 0x0F;
    chip8->instruction.Y = (chip8->instruction.opcode >> 4) & 0x0F;

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    // Emulate opcode:
    switch((chip8->instruction.opcode >> 12) & 0x0F)
    {
        case 0x00:
            if(chip8->instruction.NN == 0xE0)
            {
                //0x00E0: Clear the screen
                memset(&chip8->display[0], false, sizeof(chip8->display)); 
            } else if (chip8->instruction.NN == 0xEE) {
                // 0x00EE: Return from a subroutine
                // Set PC to last return address which was stored on the subroutine stack, and the pop it off
                chip8->PC = *--chip8->stack_pointer;
            }
            break;
        case 0x02:
            // 0x2NNN: Call Subroutine at NNN:
            // Store the return point (PC, which has been incremented by 2 to avoid an infinite loop of calling the subroutine and returning to it)
            // Update the PC to NNN, which is where the subroutine is located
            *chip8->stack_pointer++ = chip8->PC;
            chip8->PC = chip8->instruction.NNN;
            break;
        case 0x0A:
            // 0xANNN: Sets I to the address NNN. I: Instruction Register
            chip8->I = chip8->instruction.NNN;
            break;
        case 0x06:
            // 0x6XNN: Set Register Vx = NN
            chip8->V[chip8->instruction.X] = chip8->instruction.NN;
            break;
        case 0x0D:
            // 0xDXYN: Draw a sprite at (Vx, Vy), of height N and width 8 pixels
            // Each row of 8 pixels is read as bit-coded starting from memory location I
            // VF (Carry Flag) is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn
            // Screen Pixels will be XORd with sprite bits
            uint8_t X_coord = chip8->V[chip8->instruction.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->instruction.Y] % config.window_height;
            const uint8_t orig_X = X_coord;
            //chip8->V[0xF] = 0; // Carry flag initialized to 0
            // Loop over the N rows of the sprite to be drawn
            for(uint8_t i = 0; i < chip8->instruction.N; i++)
            {
                // Get row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X; // Reset X for next row
                
                for(int8_t j = 7; j >= 0; j--) 
                {
                    // If the sprite pixel is on AND the display pixel is on, set the carry flag to 1.
                    bool* pixel = &chip8->display[(Y_coord * config.window_width) + X_coord];
                    bool sprite_bit = sprite_data & (1 << j);
                    if(sprite_bit && (*pixel))
                    {
                        chip8->V[0xF] = 1;
                    }
                    // display pixel = (display pixel) xor (sprite pixel)
                    *pixel ^= sprite_bit;  

                    // If the right edge of the screen is hit, stop drawing this row
                    if(++X_coord >= config.window_width) break;
                    
                }

                // If the Y coordinate exceeds the window height, stop drawing the sprite
                if(++Y_coord >= config.window_height) break;
            }
            break;
        case 0x07:
            // 0x7XNN: Vx += NN. Carry flag is not changed
            chip8->V[chip8->instruction.X] += chip8->instruction.NN;
            break;
        default:
            break; // Uninimplemented / invalid opcode
    }

}

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

    // Emulator loop
    while (chip8.state != QUIT)
    {
        // Handle user input
        handle_input(&chip8);

        if(chip8.state == PAUSED) continue;

        // get_time()
        // Emulate CHIP8 Instructions
        emulate_instruction(&chip8, config);
        // time elapsed since get_time()

        // Delay for 60fps (approximatley)
        // SDL_Delay(16 - time elapsed)
        SDL_Delay(16);

        // Update the screen with changes
        update_screen(sdl, config, chip8);
    }

    // Cleanup and Exit
    final_cleanup(sdl);
    exit(EXIT_SUCCESS);
}