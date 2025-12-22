// mgl_generator.h
// MGL file generator for MiSTer graphical frontend
// 2024

#ifndef __MGL_GENERATOR_H__
#define __MGL_GENERATOR_H__

// Generate MGL files for a specific system
// Returns: number of MGLs created (0 if all already exist, -1 on error)
int mgl_generate_for_system(const char *system_name);

// Generate MGL files for all known systems
// Returns: total number of MGLs created
int mgl_generate_all(void);

// Get number of supported systems
int mgl_get_system_count(void);

// Get system name by index (0-based)
const char* mgl_get_system_name(int index);

// Get system core path by index
const char* mgl_get_system_core(int index);

// Check if a system has any ROMs
int mgl_system_has_roms(const char *system_name);

// Get ROM count for a system
int mgl_get_rom_count(const char *system_name);

#endif // __MGL_GENERATOR_H__
