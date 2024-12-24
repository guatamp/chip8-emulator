#pragma once

#include "common.h"

// Emulator State Object
typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

// Config Container Object
typedef struct
{
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_color;                // Foreground color 32 bit RGBA8888
    uint32_t bg_color;                // Background color 32 bit RGBA8888
    uint32_t scale_factor;            // Amount to scale a chip8 pixel by
    bool pixel_outlines;              // Draw pixel outlines yes/no
    uint32_t instructions_per_second; // CHIP8 CPU Clock Rate
} config_t;

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

bool set_config_from_args(config_t *config, const int argc, char** argv);
bool init_chip8(chip8_t *chip8, const char rom_name[]);
void update_timers(chip8_t *chip8);