#pragma once

#include "common.h"
#include "chip8.h"

void audio_callback(void* userdata, uint8_t *stream, int len);
bool init_sdl(sdl_t *sdl, config_t *config);
void final_cleanup(const sdl_t sdl);
void clear_screen(const sdl_t sdl, const config_t config);
void update_screen(sdl_t sdl, config_t config, chip8_t chip8);
void handle_input(chip8_t* chip8);