#include "chip8.h"

bool set_config_from_args(config_t *config, const int argc, char** argv)
{
    // Set Defaults: 32x64 default
    *config = (config_t) {
        .window_width = 64,
        .window_height = 32,
        .fg_color = 0xFFFFFFFF, // White
        .bg_color = 0x000000FF, // Black
        .scale_factor = 25,     // Default res of 64x32 times 20 = 1280x640
        .pixel_outlines = true,
        .instructions_per_second = 500,
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

void update_timers(chip8_t *chip8)
{
    if(chip8->delay_timer > 0) chip8->delay_timer--;
    
    if(chip8->sound_timer > 0)
    { 
        chip8->sound_timer--;
        // TODO: Play sound
    } else {
        // Stop playing sound
    }
}
