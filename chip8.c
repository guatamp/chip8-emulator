#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
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
    bool pixel_outlines;     // Draw pixel outlines yes/no
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
        .pixel_outlines = true,
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
            case 0x01:
                // 1NNN: Jump to address at NNN.
                printf("PC set to NNN: (0x%04X)\n", chip8->instruction.NNN);
                break;
            case 0x02:
                // 0x2NNN: Call Subroutine at NNN:
                // Store the return point (PC, which has been incremented by 2 to avoid an infinite loop of calling the subroutine and returning to it)
                // Update the PC to NNN, which is where the subroutine is located
                printf("Call Subroutine located at 0x%04X\n", chip8->instruction.NNN);
                break;
            case 0x03:
                // 0x3XNN: Skip the next instruction if value in Vx == NN
                printf("If V%X (0x%02X) is is equal to NN (0x%02X), then skip the next instruction.\n", chip8->instruction.X, 
                        chip8->V[chip8->instruction.X], chip8->instruction.NN);
                break;
            case 0x04:
                // 0x4XNN: Skips the next instruction if VX does not equal NN
                printf("If V%X  (0x%02X)is is not equal to NN (0x%02X), then skip the next instruction.\n", chip8->instruction.X, 
                        chip8->V[chip8->instruction.X], chip8->instruction.NN);
                break;
            case 0x05:
                // 0x5XY0: Skips the next instruction if VX equals VY
                printf("If V%X  (0x%02X) is equal to V%X (0x%02X), then skip the next instruction.\n", 
                        chip8->instruction.X, chip8->V[chip8->instruction.X],
                        chip8->instruction.Y, chip8->V[chip8->instruction.Y]);
                break;
            case 0x06:
                // 0x6XNN: Set Register Vx = NN
                printf("Set Register V%01X to 0x%02X\n", chip8->instruction.X, chip8->instruction.NN);
                break;
            case 0x07:
                // 0x7XNN: Vx += NN. Carry flag is not changed
                printf("Value NN (0x%02X) was added to register V%X, which had initial value 0x%02X\n",
                        chip8->instruction.NN, chip8->instruction.X, chip8->V[chip8->instruction.X]);
                break;
            case 0x08:
                switch(chip8->instruction.N)
                {
                    case 0:
                        // 0x8XY0: Set the value of Vx equal to the value of Vy
                        printf("Value of V%X set to value of V%X (0x%02X)\n", chip8->instruction.X, chip8->instruction.Y,
                                chip8->V[chip8->instruction.Y]); 
                        break;
                    case 1:
                        // 0x8XY1: Set Vx to Vx | Vy (Bitwise OR)
                        printf("Set V%X to bitwise OR with V%X (0x%02X)\n", chip8->instruction.X, chip8->instruction.Y,
                                chip8->V[chip8->instruction.Y]);
                        break;
                    case 2:
                        // 0x8XY1: Set Vx to Vx & Vy (Bitwise AND)
                        printf("Set V%X to bitwise AND with V%X (0x%02X)\n", chip8->instruction.X, chip8->instruction.Y,
                                chip8->V[chip8->instruction.Y]);
                        break;
                    case 3:
                        // 0x8XY3: Set Vx to Vx ^ Vy (Bitwise XOR)
                        printf("Set V%X to bitwise XOR with V%X (0x%02X)\n", chip8->instruction.X, chip8->instruction.Y,
                                chip8->V[chip8->instruction.Y]);
                        break;
                    case 4:
                        // 08XY4: Add Vy to Vx. Set VF to 1 if overflow occurs, else set it to 0
                        printf("V%X (0x%02X) += V%X (0x%02X)\n"
                                , chip8->instruction.X, chip8->V[chip8->instruction.X]
                                , chip8->instruction.Y, chip8->V[chip8->instruction.Y]);
                        break;
                    case 5:
                        // 0x8XY5: Subtract Vy from Vx. Set VF to 1 when no underflow occurs, and 0 when there is underflow
                        printf("V%X (0x%02X) -= V%X (0x%02X)\n"
                                , chip8->instruction.X, chip8->V[chip8->instruction.X]
                                , chip8->instruction.Y, chip8->V[chip8->instruction.Y]);
                        break;
                    case 6:
                        // 0x8XY6: Shift VX to the right by 1, then store the least significant bit of VX prior to the shift into VF
                        printf("Right shift V%X (0x%02X) by 1 bit\n", chip8->instruction.X, chip8->V[chip8->instruction.X]);
                        break;
                    case 7:
                        // 0x8XY7: Set VX to VY minus VX. VF is set to 0 when there is an underflow, and 1 when there is not.
                        printf("V%X (0x%02X) = V%X (0x%02X) - V%X\n"
                                , chip8->instruction.X, chip8->V[chip8->instruction.X]
                                , chip8->instruction.Y, chip8->V[chip8->instruction.Y]
                                , chip8->instruction.X);
                        break;
                    case 0xE: 
                        // 0x8XYE: Shift VX to the left by 1. Set VF to 1 if the MSB of VX prior to that shift was set, or to 0 if it was unset.
                        printf("Left shift V%X (0x%02X) by 1 bit\n", chip8->instruction.X, chip8->V[chip8->instruction.X]);
                        break;
                    default:
                        printf("Unimplemented Opcode");
                        break; // unimplemented or invalid opcode
                }
                break;
            case 0x09:
                // 0x9XY0: Skips the next instruction if VX != VY
                printf("If V%X  (0x%02X) is not equal to V%X (0x%02X), then skip the next instruction.\n", 
                        chip8->instruction.X, chip8->V[chip8->instruction.X],
                        chip8->instruction.Y, chip8->V[chip8->instruction.Y]);
                break;
            case 0x0A:
                printf("Set the Instruction Register I to 0x%04X\n", chip8->instruction.NNN);
                break;
            case 0x0B:
                // 0xBNNN: Jump to the address at V0 + NNN
                printf("Set PC to V0 (0x%02X) + NNN (0x%04X) = 0x%04X\n", chip8->V[0], chip8->instruction.NNN, 
                                                                        chip8->V[0] + chip8->instruction.NNN);
                break;
            case 0x0C:
                // 0xCXNN: Set register Vx to NN & rand(0, 255)
                printf("Set register V%X to NN(0x%02X) & (rand() mod 256)\n", chip8->instruction.X, chip8->instruction.NN);
                break;
            case 0x0D:
                // 0xDXYN: Draw pixel Vx, Vy, heignt N and width 8
                printf("Drawing sprite with height N (%u), at coords V%X (%02X), V%X, (%02X), from memory location I (0x%04X)\n",
                        chip8->instruction.N, chip8->instruction.X, chip8->V[chip8->instruction.X], 
                                              chip8->instruction.Y, chip8->V[chip8->instruction.Y], chip8->I);
                break;
            case 0x0E:
                if(chip8->instruction.NN == 0x9E) 
                {
                    //0xEX9E: Skip the next instruction if the key is pressed:
                    printf("Skip next instruction if key stored in V%X (%X) is being pressed %d\n", 
                            chip8->instruction.X, chip8->V[chip8->instruction.X], chip8->keypad[chip8->V[chip8->instruction.X]]);

                } else if (chip8->instruction.NN == 0xA1) {
                    //0xEXA1: Skip the next instruction if the key is not pressed:
                    printf("Skip next instruction if key stored in V%X (%X) is  not being pressed %d\n", 
                            chip8->instruction.X, chip8->V[chip8->instruction.X], chip8->keypad[chip8->V[chip8->instruction.X]]);
                }
                break;
            case 0x0F:
                switch(chip8->instruction.NN)
                {
                    case 0x0A:
                        // 0xFX0A: A key press is awaited, and then stored in in Vx (Blocking operation, all instructions halted until next key event)
                        printf("Wait until a key is pressed, store key in V%X\n", chip8->instruction.X);
                        break;
                    case 0x07:
                        // 0xFX07: Set Vx to the value of the delay timer
                        printf("Set value of V%X to the value of the delay timer (0x%X)\n", chip8->instruction.X, chip8->delay_timer);
                        break;
                    case 0x15:
                        // 0xFX15: Set the delay timer to value of Vx
                        printf("Set the value of the delay timer to V%X (0x%02X)\n", chip8->instruction.X, chip8->V[chip8->instruction.X]);
                        break;
                    case 0x18:
                        // 0xFX18: Set the sound timer to the value of Vx
                        printf("Set the value of the sound timer to V%X (0x%02X)\n", chip8->instruction.X, chip8->V[chip8->instruction.X]);
                        break;
                    case 0x1E:
                        // 0xFX1E: Add Vx to I ie I += Vx
                        printf("Increment I (0x%04X) by V%X (%02X) = 0x%04X\n",
                                chip8->I, chip8->instruction.X, chip8->V[chip8->instruction.X], chip8->I + chip8->V[chip8->instruction.X]);
                        break;
                    case 0x29:
                        // 0xFX29: Set I to the location of the sprite for the character in Vx
                        // Vx has 0x0 - 0xF so it is the sprite for one of those characters
                        // Font is stored at start of RAM.
                        // So offset into RAM by 5 * Vx
                        printf("Set I to sprite location in memory for character in V%X =  (0x%02X) * 5\n", 
                                chip8->instruction.X, chip8->V[chip8->instruction.X]);
                        break;
                    case 0x33:
                        // 0xFX33: Stores the binary-coded decimal representation of VX, 
                        // with the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
                        printf("Store BCD representation of V%X (%02X) at memory offset in I (0x%04X)\n",
                                chip8->instruction.X, chip8->V[chip8->instruction.X], chip8->I );
                        break;
                    case 0x55:
                        // 0xFX55: Store V0 to VX in ram location starting at location in I.
                        printf("Reg dump V0 to V%X (inclusive) in ram location starting at I (0x%04X)\n", 
                                chip8->instruction.X, chip8->I);
                        break;
                    case 0x65:
                        // 0xFX65: Store V0 to VX in ram location starting at location in I.
                        printf("Reg load ram location starting at I (0x%04X) into V0 to V%X (inclusive)\n", 
                                chip8->I, chip8->instruction.X);
                        break;
                    default:
                        printf("Unimplemented opcode\n");
                        break;
                }
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
    bool carry;

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
        case 0x01:
            // 1NNN: Jump to address at NNN.
            chip8->PC = chip8->instruction.NNN;
            break;
        case 0x02:
            // 0x2NNN: Call Subroutine at NNN:
            // Store the return point (PC, which has been incremented by 2 to avoid an infinite loop of calling the subroutine and returning to it)
            // Update the PC to NNN, which is where the subroutine is located
            *chip8->stack_pointer++ = chip8->PC;
            chip8->PC = chip8->instruction.NNN;
            break;
        case 0x03:
            // 0x3XNN: Skip the next instruction if value in Vx == NN
            if(chip8->V[chip8->instruction.X] == chip8->instruction.NN)
            {
                chip8->PC += 2;
            }
            break;
        case 0x04:
            // 0x4XNN: Skips the next instruction if VX does not equal NN
            if(chip8->V[chip8->instruction.X] != chip8->instruction.NN)
            {
                chip8->PC += 2;
            }
            break;
        case 0x05:
            // 0x5XY0: Skips the next instruction if VX equals VY
            if(chip8->V[chip8->instruction.X] == chip8->V[chip8->instruction.Y])
            {
                chip8->PC += 2;
            }
            break;
        case 0x06:
            // 0x6XNN: Set Register Vx = NN
            chip8->V[chip8->instruction.X] = chip8->instruction.NN;
            break;
        case 0x07:
            // 0x7XNN: Vx += NN. Carry flag is not changed
            chip8->V[chip8->instruction.X] += chip8->instruction.NN;
            break;
        case 0x08:
            switch(chip8->instruction.N)
            {
                case 0:
                    // 0x8XY0: Set the value of Vx equal to the value of Vy
                    chip8->V[chip8->instruction.X] = chip8->V[chip8->instruction.Y]; 
                    break;
                case 1:
                    // 0x8XY1: Set Vx to Vx | Vy (Bitwise OR)
                    chip8->V[chip8->instruction.X] |= chip8->V[chip8->instruction.Y];
                    break;
                case 2:
                    // 0x8XY1: Set Vx to Vx & Vy (Bitwise AND)
                    chip8->V[chip8->instruction.X] &= chip8->V[chip8->instruction.Y];
                    break;
                case 3:
                    // 0x8XY3: Set Vx to Vx ^ Vy (Bitwise XOR)
                    chip8->V[chip8->instruction.X] ^= chip8->V[chip8->instruction.Y];
                    break;
                case 4:
                    // 08XY4: Add Vy to Vx. Set VF to 1 if overflow occurs, else set it to 0
                    chip8->V[chip8->instruction.X] += chip8->V[chip8->instruction.Y];
                    carry = (chip8->V[chip8->instruction.X] + chip8->V[chip8->instruction.Y]) > 255;
                    chip8->V[0xF] = carry;
                    break;
                case 5:
                    // 0x8XY5: Subtract Vy from Vx. Set VF to 1 when no underflow occurs, and 0 when there is underflow
                    chip8->V[chip8->instruction.X] -= chip8->V[chip8->instruction.Y];
                    carry = chip8->V[chip8->instruction.Y] >= chip8->V[chip8->instruction.X]; // No underflow
                    chip8->V[0xF] = carry;
                    break;
                case 6:
                    // 0x8XY6: Shift VX to the right by 1, then store the least significant bit of VX prior to the shift into VF
                    carry = chip8->V[chip8->instruction.X] & 1;
                    chip8->V[chip8->instruction.X] >>= 1;
                    chip8->V[0xF] = carry;
                    break;
                case 7:
                    // 0x8XY7: Set VX to VY minus VX. VF is set to 0 when there is an underflow, and 1 when there is not.
                    chip8->V[chip8->instruction.X] = chip8->V[chip8->instruction.Y] - chip8->V[chip8->instruction.X];
                    carry = chip8->V[chip8->instruction.X] <= chip8->V[chip8->instruction.Y]; // No underflow
                    chip8->V[0xF] = carry;
                    break;
                case 0xE: 
                    // 0x8XYE: Shift VX to the left by 1. Set VF to 1 if the MSB of VX prior to that shift was set, or to 0 if it was unset.
                    carry = chip8->V[chip8->instruction.X] >> 7;
                    chip8->V[0xF] = carry;
                    chip8->V[chip8->instruction.X] <<= 1;
                    break;
                default:
                    break; // unimplemented or invalid opcode
            }
            break;
        case 0x09:
            // 0x9XY0: Skips the next instruction if VX != VY
            if(chip8->V[chip8->instruction.X] != chip8->V[chip8->instruction.Y])
            {
                chip8->PC += 2;
            }
            break;
        case 0x0A:
            // 0xANNN: Sets I to the address NNN. I: Instruction Register
            chip8->I = chip8->instruction.NNN;
            break;
        case 0x0B:
            // 0xBNNN: Jump to the address at V0 + NNN
            chip8->PC = chip8->V[0] + chip8->instruction.NNN;
            break;
        case 0x0C:
            // 0xCXNN: Set register Vx to NN & rand(0, 255)
            chip8->V[chip8->instruction.X] = (rand() % 256) & chip8->instruction.NN;
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
        case 0x0E:
            if(chip8->instruction.NN == 0x9E) 
            {
                //0xEX9E: Skip the next instruction if the key is pressed:
                if(chip8->keypad[chip8->V[chip8->instruction.X]])
                {
                    chip8->PC += 2;
                }

            } else if (chip8->instruction.NN == 0xA1) {
                //0xEXA1: Skip the next instruction if the key is not pressed:
                if(!chip8->keypad[chip8->V[chip8->instruction.X]])
                {
                    chip8->PC += 2;
                }
            }
            break;
        case 0x0F:
            switch(chip8->instruction.NN)
            {
                case 0x0A:
                    // 0xFX0A: A key press is awaited, and then stored in in Vx (Blocking operation, all instructions halted until next key event)
                    bool any_key_pressed = false;
                    for(uint8_t i = 0; i < sizeof chip8->keypad; i++)
                    {
                        if(chip8->keypad[i])
                        {
                            chip8->V[chip8->instruction.X] = i;
                            any_key_pressed = true;
                            break;
                        }

                        if(!any_key_pressed)
                        {
                            chip8->PC -= 2; // Repeat this instruction
                        }
                    }
                    break;
                case 0x07:
                    // 0xFX07: Set Vx to the value of the delay timer
                    chip8->V[chip8->instruction.X] = chip8->delay_timer;
                    break;
                case 0x15:
                    // 0xFX15: Set the delay timer to value of Vx
                    chip8->delay_timer = chip8->V[chip8->instruction.X];
                    break;
                case 0x18:
                    // 0xFX18: Set the sound timer to the value of Vx
                    chip8->sound_timer = chip8->V[chip8->instruction.X];
                    break;
                case 0x1E:
                    // 0xFX1E: Add Vx to I ie I += Vx
                    chip8->I += chip8->V[chip8->instruction.X];
                    break;
                case 0x29:
                    // 0xFX29: Set I to the location of the sprite for the character in Vx
                    // Vx has 0x0 - 0xF so it is the sprite for one of those characters
                    // Font is stored at start of RAM.
                    // So offset into RAM by 5 * Vx
                    chip8->I = chip8->V[chip8->instruction.X] * 5;
                    break;
                case 0x33:
                    // 0xFX33: Stores the binary-coded decimal representation of VX, 
                    // with the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
                    uint8_t bcd = chip8->V[chip8->instruction.X]; 
                    chip8->ram[chip8->I+2] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I+1] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I] = bcd;
                    break;
                case 0x55:
                    // 0xFX55: Reg dump V0 to VX in ram location starting at location in I.
                    for(uint8_t i = 0; i <= chip8->instruction.X; i++)
                    {
                        chip8->ram[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x65:
                    // 0xFX65: Reg load starting from ram location in I into V0 to VX in.
                    for(uint8_t i = 0; i <= chip8->instruction.X; i++)
                    {
                        chip8->V[i] = chip8->ram[chip8->I + i];
                    }
                    break;
                default: 
                    break;
            }
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

    // Set seed for rand()
    srand(time(NULL));

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