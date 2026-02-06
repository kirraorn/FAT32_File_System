#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include "structs.h"

// PART ONE: 
// funct to read BPB from disk image
unsigned int load_bpb_and_init_state(const char* image_name, FILE *fp);
void exit_shell();
void cmd_info();
// helpers for calcs 
long get_sector_offset(unsigned int sector_num); 
unsigned int get_first_data_sector();
unsigned int get_total_clusters();

// PART TWO:
unsigned int get_cluster_sector(unsigned int cluster_num);
unsigned int read_fat_entry(unsigned int cluster_num);
void ls_command();
void cd_command( char *dirname);

// PART THREE:
unsigned int get_free_cluster(); // helper for creat
void write_fat_entry(unsigned int cluster_num, unsigned int value); // helper for creat
void creat_command(char *filename);

// PART FOUR:
void lsof_command();
void open_command(char *filename, char *flags);
void close_command(char *filename);
void size_command(char *filename);
unsigned int find_cluster_from_offset(unsigned int starting_cluster, long offset); // helper for read
void read_command(char *filename, char *size_str);
void lseek_command(char *filename, char *offset_str);

//program loop
int start_program_shell(int argc, char *argv[]);

// PART FIVE:
void write_command(char *filename, char *size_str);
void mv_command(char *source, char *dest);

// PART SIX:
void rm_command(char *filename);
void rmdir_command(char *dirname);

#endif // COMMANDS_H
