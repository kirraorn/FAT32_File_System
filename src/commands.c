#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include "structs.h" 
#include "commands.h"
#include "lexer.h" 

// external declarations
extern FS_STATE g_fs_state; 
extern unsigned int load_bpb_and_init_state(const char *image_name, FILE *fp);
extern unsigned int get_total_clusters(); 
extern unsigned int get_cluster_sector(unsigned int cluster_num); 
extern unsigned int read_fat_entry(unsigned int cluster_num);    
extern unsigned int find_cluster_from_offset(unsigned int starting_cluster, long offset);
extern void write_fat_entry(unsigned int cluster_num, unsigned int value);
extern unsigned int get_free_cluster();

extern tokenlist *get_tokens(char *line); 
extern void free_tokens(tokenlist *tokens);


// trims leading and trailing whitespace from a string
void trim_whitespace(char *str) {
    size_t len = strlen(str);
    if (len == 0) return;
    while (len > 0 && (isspace((unsigned char)str[len-1]) || str[len-1] == '\r')) {
        str[--len] = '\0';
    }
    if (len > 0 && isspace((unsigned char)str[0])) {
        size_t start = 0;
        while (start < len && isspace((unsigned char)str[start])) {
            start++;
        }
        if (start > 0) {
            memmove(str, str + start, len - start + 1);
        }
    }
}

//get formatted name for ls
void get_formatted_name(unsigned char *raw_name, char *out_name) {
    char name[9];
    char ext[4];
    
    // clean name
    strncpy(name, (char*)raw_name, 8);
    name[8] = '\0';
    for(int i=7; i>=0; i--) {
        if(name[i] == ' ') name[i] = '\0';
        else break;
    }
    
    // clean extension
    strncpy(ext, (char*)raw_name + 8, 3);
    ext[3] = '\0';
    for(int i=2; i>=0; i--) {
        if(ext[i] == ' ') ext[i] = '\0';
        else break;
    }
    
    //format output
    if (strlen(ext) > 0) {
        sprintf(out_name, "%s.%s", name, ext);
    } else {
        strcpy(out_name, name);
    }
}

//searches to see if there is already an open file with the same name and path
int search_current_directory(char *name, DIR_ENTRY *search_dir_entry, long *slot_offset)
{
    unsigned int current_cluster = g_fs_state.current_cluster;
    unsigned int sector_size = g_fs_state.fs_bpb.BPB_BytsPerSec;
    unsigned int sectors_per_cluster = g_fs_state.fs_bpb.BPB_SecPerClus;
    
    unsigned char *buffer = (unsigned char *)malloc(sector_size);
    if (!buffer) {
        printf("Error: Memory allocation failed during directory search.\n");
        return 0; 
    }

    int found = 0;
    
    while (current_cluster < 0x0FFFFFF8) {
        unsigned int start_sector = get_cluster_sector(current_cluster);
        
        for (unsigned int i = 0; i < sectors_per_cluster; i++) {
            unsigned int sector_to_read = start_sector + i;
            long offset = (long)sector_to_read * sector_size;
            
            //seek/read sector
            if (fseek(g_fs_state.image_fp, offset, SEEK_SET) != 0 ||
                fread(buffer, sector_size, 1, g_fs_state.image_fp) != 1) {
                goto search_end; 
            }
            
            for (unsigned int j = 0; j < sector_size; j += 32) {
                DIR_ENTRY *entry = (DIR_ENTRY *)(buffer + j);
                
                //calc byte offset
                long entry_abs_offset = offset + j;

                // end of directory
                if (entry->DIR_Name[0] == 0x00) {
                    if (slot_offset != NULL) {
                        *slot_offset = entry_abs_offset;
                        found = 0; // slot found, but file not found
                    }
                    goto search_end; // stop searching if 0x00 is encountered
                }
                
                // deleted entry - treat as potential slot for new file
                if (entry->DIR_Name[0] == 0xE5 && slot_offset != NULL && *slot_offset == -1) {
                    *slot_offset = entry_abs_offset; // Found a deleted entry slot
                }
                
                // LFN = skip
                if ((entry->DIR_Attr & ATTR_LFN) == ATTR_LFN) continue;
                if (entry->DIR_Attr & ATTR_VOLUME_ID) continue;

                char entry_name[13];
                get_formatted_name(entry->DIR_Name, entry_name);

                //check for same nae
                if (strcmp(entry_name, name) == 0) {
                    found = 1;
                    if (search_dir_entry != NULL) {
                        memcpy(search_dir_entry, entry, sizeof(DIR_ENTRY));
                    }
                    goto search_end;
                }
            }
        }
        
        //move to next cluster in chain
        current_cluster = read_fat_entry(current_cluster);
    }
    
search_end:
    free(buffer);
    return found;
}

// PART ONE COMMANDS -----------------------------------------

// info command
void info_command() {
    unsigned int bytes_per_sector = g_fs_state.fs_bpb.BPB_BytsPerSec;
    unsigned int sectors_per_cluster = g_fs_state.fs_bpb.BPB_SecPerClus;
    unsigned int root_cluster = g_fs_state.fs_bpb.BPB_RootClus;

    unsigned int total_data_clusters = get_total_clusters(); 
    unsigned int num_entries_in_fat = (g_fs_state.fs_bpb.BPB_FATSz32 * bytes_per_sector) / 4;
    long size_of_image = (long)g_fs_state.fs_bpb.BPB_TotSecs32 * bytes_per_sector;
    
    printf("Bytes Per Sector: %u\n", bytes_per_sector);
    printf("Sectors Per Cluster: %u\n", sectors_per_cluster);
    printf("Total clusters in Data Region: %u\n", total_data_clusters);
    printf("# of entries in one FAT: %u\n", num_entries_in_fat);
    printf("Size of Image (bytes): %ld\n", size_of_image);
    printf("Root Cluster: %u\n", root_cluster);


}

// exit command
void exit_shell() {
    if (g_fs_state.image_fp != NULL) {
        if (fclose(g_fs_state.image_fp) == EOF) {
            perror("Error closing file image");
        }
        g_fs_state.image_fp = NULL;
    }
    printf("Safely closing program.\n");
    exit(EXIT_SUCCESS); 
}

// PART TWO COMMANDS -----------------------------------------

//ls command
void ls_command() {
    unsigned int current_cluster = g_fs_state.current_cluster;
    unsigned int sector_size = g_fs_state.fs_bpb.BPB_BytsPerSec;
    unsigned int sectors_per_cluster = g_fs_state.fs_bpb.BPB_SecPerClus;
    
    // buffer to hold one sector
    unsigned char *buffer = (unsigned char *)malloc(sector_size);
    if (!buffer) {
        printf("Error: Memory allocation failed for ls.\n");
        return;
    }

    // iterate through the cluster list
    while (current_cluster < 0x0FFFFFF8) {
        unsigned int start_sector = get_cluster_sector(current_cluster);
        
        // iterate through sectors in this cluster
        for (unsigned int i = 0; i < sectors_per_cluster; i++) {
            unsigned int sector_to_read = start_sector + i;
            
            // read sector
            long offset = (long)sector_to_read * sector_size;
            if (fseek(g_fs_state.image_fp, offset, SEEK_SET) != 0) {
                printf("Error: Failed to seek to sector %u\n", sector_to_read);
                free(buffer);
                return;
            }
            if (fread(buffer, sector_size, 1, g_fs_state.image_fp) != 1) {
                printf("Error: Failed to read sector %u\n", sector_to_read);
                free(buffer);
                return;
            }
            
            //iterate through directory entries in this sector
            for (unsigned int j = 0; j < sector_size; j += 32) {
                DIR_ENTRY *entry = (DIR_ENTRY *)(buffer + j);
                
                // end of directory entries
                if (entry->DIR_Name[0] == 0x00) {
                    free(buffer);
                    printf("\n");
                    return; // Stop listing
                }
                
                // delted entry = skip
                if (entry->DIR_Name[0] == 0xE5) continue;
                
                // long file or hidden =  kip
                if ((entry->DIR_Attr & ATTR_LFN) == ATTR_LFN) continue;
                if (entry->DIR_Attr & (ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)) continue;

                // Print Entry
                char name[13]; 
                get_formatted_name(entry->DIR_Name, name);
                printf("%s  ", name);
                
            }
        }
        
        // move to the next cluster
        current_cluster = read_fat_entry(current_cluster);
    }
    
    printf("\n"); 
    free(buffer);
}

// cd command
void cd_command(char *dirname) {
    if (dirname == NULL) {
        printf("Error: Directory name not provided.\n");
        return;
    }

    unsigned int sector_size = g_fs_state.fs_bpb.BPB_BytsPerSec;
    unsigned int sectors_per_cluster = g_fs_state.fs_bpb.BPB_SecPerClus;
    unsigned int new_cluster = 0;
    
    unsigned char *buffer = (unsigned char *)malloc(sector_size);
    if (!buffer) {
        printf("Error: Memory allocation failed for cd.\n");
        return;
    }

    //search for the directory entry in the current cluster chain
    unsigned int search_cluster = g_fs_state.current_cluster;
    int found = 0;

    while (search_cluster < 0x0FFFFFF8) {
        unsigned int start_sector = get_cluster_sector(search_cluster);
        
        for (unsigned int i = 0; i < sectors_per_cluster; i++) {
            unsigned int sector_to_read = start_sector + i;
            
            long offset = (long)sector_to_read * sector_size;
            if (fseek(g_fs_state.image_fp, offset, SEEK_SET) != 0 ||
                fread(buffer, sector_size, 1, g_fs_state.image_fp) != 1) {
                break; 
            }
            
            for (unsigned int j = 0; j < sector_size; j += 32) {
                DIR_ENTRY *entry = (DIR_ENTRY *)(buffer + j);
                
                if (entry->DIR_Name[0] == 0x00) goto search_end; 
                if (entry->DIR_Name[0] == 0xE5) continue; 
                if ((entry->DIR_Attr & ATTR_LFN) == ATTR_LFN) continue;
                if (entry->DIR_Attr & ATTR_VOLUME_ID) continue; 

                char entry_name[13];
                get_formatted_name(entry->DIR_Name, entry_name);

                if (strcmp(entry_name, dirname) == 0) {
                    if (!(entry->DIR_Attr & ATTR_DIRECTORY)) {
                        printf("Error: '%s' is not a directory.\n", dirname);
                        free(buffer);
                        return;
                    }
                    
                    // combine high and low words of the cluster number
                    new_cluster = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;
                    found = 1;
                    goto search_end;
                }
            }
        }
        
        search_cluster = read_fat_entry(search_cluster);
    }

search_end:
    free(buffer);

    if (!found) {
        printf("Error: Directory '%s' not found.\n", dirname);
        return;
    }

    //update global state

    // temp buffer to build the new path string safely
    char new_path[sizeof(g_fs_state.current_path)];
    
    // going to parent directory ("..")
    if (strcmp(dirname, "..") == 0) {
        // if the current cluster is the root cluster, attempting to go back should stay at root
        if (g_fs_state.current_cluster == g_fs_state.fs_bpb.BPB_RootClus) {
            strcpy(new_path, "/");
            new_cluster = g_fs_state.current_cluster; // Cluster remains root
        } 
        else 
        {
            //check if path ends with a slash 
            size_t len = strlen(g_fs_state.current_path);
            if (len > 1 && g_fs_state.current_path[len - 1] == '/') {
                g_fs_state.current_path[len - 1] = '\0'; 
            }
            
            //find the last remaining slash (marks the start of the current directory name)
            char *last_slash = strrchr(g_fs_state.current_path, '/');
            
            if (last_slash != NULL) {
                // null-terminate immediately after the slash to chop off the current directory name
                last_slash[1] = '\0'; 
                strcpy(new_path, g_fs_state.current_path);
            } else {
                strcpy(new_path, "/");
            }
        }
    } 
    // going to current directory no change
    else if (strcmp(dirname, ".") == 0) {
        return;
    }
    // reg subdirectory move
    else {
        if (strcmp(g_fs_state.current_path, "/") == 0) {
            snprintf(new_path, sizeof(new_path), "/%s/", dirname);
        } else {
            // append to existing path
            snprintf(new_path, sizeof(new_path), "%s%s", g_fs_state.current_path, dirname);
        }
    }

    //update of the global state variables
    strcpy(g_fs_state.current_path, new_path);
    g_fs_state.current_cluster = new_cluster;
}

// PART THREE COMMANDS -----------------------------------------

void mkdir_command(char *dirname) {
    if (dirname == NULL) {
        printf("Error: Directory name not provided.\n");
        return;
    }

    long free_slot_offset = -1;
    // Check if directory already exists or find a free slot
    if (search_current_directory(dirname, NULL, &free_slot_offset)) {
        printf("Error: Directory '%s' already exists.\n", dirname);
        return;
    }

    // Check if found a free slot
    if (free_slot_offset == -1) {
        printf("Error: No free directory entry available to create '%s'.\n", dirname);
        return;
    }

    // Find a free cluster for the new directory
    unsigned int new_cluster = get_free_cluster();
    if (new_cluster == 0 || new_cluster >= 0x0FFFFFF8) {
        printf("Error: No free clusters available to create directory.\n");
        return;
    }

    // Mark the cluster as EOF in FAT
    write_fat_entry(new_cluster, 0x0FFFFFFF);

    // Create new directory entry for parent directory
    DIR_ENTRY new_entry;
    memset(&new_entry, 0, sizeof(DIR_ENTRY));

    // Format the name (convert to 8.3 format)
    char name_buf[12];
    memset(name_buf, ' ', 11);
    name_buf[11] = '\0';

    // Copy name into buffer (uppercase)
    for (int i = 0; i < 11 && dirname[i] != '\0'; i++) {
        name_buf[i] = toupper((unsigned char)dirname[i]);
    }
    memcpy(new_entry.DIR_Name, name_buf, 11);

    // Set attributes for directory
    new_entry.DIR_Attr = ATTR_DIRECTORY;
    new_entry.DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;
    new_entry.DIR_FstClusLO = new_cluster & 0xFFFF;
    new_entry.DIR_FileSize = 0; // Directories have size 0
    new_entry.DIR_WrtDate = 0;
    new_entry.DIR_WrtTime = 0;

    // Write the entry to the parent directory
    if (fseek(g_fs_state.image_fp, free_slot_offset, SEEK_SET) != 0) {
        printf("Error: Failed to seek to free directory slot.\n");
        return;
    }
    if (fwrite(&new_entry, sizeof(DIR_ENTRY), 1, g_fs_state.image_fp) != 1) {
        printf("Error: Failed to write directory entry to disk.\n");
        return;
    }

    // Initialize the new directory with "." and ".." entries
    unsigned int cluster_size = g_fs_state.fs_bpb.BPB_BytsPerSec * g_fs_state.fs_bpb.BPB_SecPerClus;
    unsigned char *cluster_buffer = (unsigned char *)malloc(cluster_size);
    if (!cluster_buffer) {
        printf("Error: Memory allocation failed for new directory cluster.\n");
        return;
    }
    memset(cluster_buffer, 0, cluster_size);

    // Create "." entry pointing to itself
    DIR_ENTRY *dot_entry = (DIR_ENTRY *)cluster_buffer;
    memset(dot_entry->DIR_Name, ' ', 11);
    dot_entry->DIR_Name[0] = '.';
    dot_entry->DIR_Attr = ATTR_DIRECTORY;
    dot_entry->DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;
    dot_entry->DIR_FstClusLO = new_cluster & 0xFFFF;
    dot_entry->DIR_FileSize = 0;

    // Create ".." entry (points to parent)
    DIR_ENTRY *dotdot_entry = (DIR_ENTRY *)(cluster_buffer + 32);
    memset(dotdot_entry->DIR_Name, ' ', 11);
    dotdot_entry->DIR_Name[0] = '.';
    dotdot_entry->DIR_Name[1] = '.';
    dotdot_entry->DIR_Attr = ATTR_DIRECTORY;
    
    // Parent cluster handling - if we're in root, ".." should point to root
    unsigned int parent_cluster = g_fs_state.current_cluster;
    if (parent_cluster == g_fs_state.fs_bpb.BPB_RootClus) {
        // For root directory, ".." typically points to cluster 0
        dotdot_entry->DIR_FstClusHI = 0;
        dotdot_entry->DIR_FstClusLO = 0;
    } else {
        dotdot_entry->DIR_FstClusHI = (parent_cluster >> 16) & 0xFFFF;
        dotdot_entry->DIR_FstClusLO = parent_cluster & 0xFFFF;
    }
    dotdot_entry->DIR_FileSize = 0;

    // Write the initialized cluster to disk
    unsigned int cluster_start_sector = get_cluster_sector(new_cluster);
    long cluster_offset = (long)cluster_start_sector * g_fs_state.fs_bpb.BPB_BytsPerSec;
    
    if (fseek(g_fs_state.image_fp, cluster_offset, SEEK_SET) != 0) {
        printf("Error: Failed to seek to new directory cluster.\n");
        free(cluster_buffer);
        return;
    }
    if (fwrite(cluster_buffer, cluster_size, 1, g_fs_state.image_fp) != 1) {
        printf("Error: Failed to write new directory cluster to disk.\n");
        free(cluster_buffer);
        return;
    }

    free(cluster_buffer);
    //printf("Directory '%s' created successfully.\n", dirname);
}



void creat_command(char *filename){
    if (filename == NULL){
        printf("Error: Filename not provided.\n");
        return;
    }

    long free_slot_offset = -1;
    // check if file exists or find a free slot
    if (search_current_directory(filename, NULL, &free_slot_offset)) {
        printf("Error: File '%s' already exists.\n", filename);
        return;
    }

    // check if found a free slot
    if (free_slot_offset == -1) {
        printf("Error: No free directory entry available to create '%s'.\n", filename);
        return;
    }

    //make new directory entry
    DIR_ENTRY new_entry;
    memset(&new_entry, 0, sizeof(DIR_ENTRY));

    // format the name
    char name_buf[12];
    memset(name_buf, ' ', 11);
    name_buf[11] = '\0';

    //copy into buffer
    for (int i = 0; i < 11 && filename[i] != '\0'; i++) {
        name_buf[i] = toupper((unsigned char)filename[i]);
    }
    memcpy(new_entry.DIR_Name, name_buf, 11);

    // set attributes
    new_entry.DIR_Attr = ATTR_ARCHIVE;
    new_entry.DIR_FstClusHI = 0;
    new_entry.DIR_FstClusLO = 0;
    new_entry.DIR_FileSize = 0;
    new_entry.DIR_WrtDate = 0; 
    new_entry.DIR_WrtTime = 0;
    
    // write the entry to the disk
    if (fseek(g_fs_state.image_fp, free_slot_offset, SEEK_SET) != 0) {
        printf("Error: Failed to seek to free directory slot.\n");
        return;
    }
    if (fwrite(&new_entry, sizeof(DIR_ENTRY), 1, g_fs_state.image_fp) != 1) {
        printf("Error: Failed to write directory entry to disk.\n");
        return;
    }
}

// PART FOUR COMMANDS -----------------------------------------

// open command
void open_command(char *filename, char *flags) {
    //validate input
    if (filename == NULL || flags == NULL) {
        printf("Error: Missing filename or flags.\n");
        return;
    }
    
    // find flags
    FILE_ACCESS_MODE mode;
    if (strcmp(flags, "-r") == 0) {
        mode = MODE_READ;
    } else if (strcmp(flags, "-w") == 0){
        mode = MODE_WRITE;
    } else if (strcmp(flags, "-wr") == 0) {
        mode = MODE_WRITE_READ;
    } else if (strcmp(flags, "-rw") == 0) {
        mode = MODE_READ_WRITE;
    } else {
        printf("Error: Invalid flag used. Must be -r, -w, -rw, or -wr.\n");
        return;
    }

    // search for the file in the current directory
    unsigned int sector_size = g_fs_state.fs_bpb.BPB_BytsPerSec;
    unsigned int sectors_per_cluster = g_fs_state.fs_bpb.BPB_SecPerClus;
    
    unsigned char *buffer = (unsigned char *)malloc(sector_size);
    if (!buffer) {
        printf("Error: Memory allocation failed for open.\n");
        return;
    }

    // variables to hold found file info
    DIR_ENTRY found_entry;
    int file_found = 0;
    
    unsigned int search_cluster = g_fs_state.current_cluster;

    // same for loop as ls and cd to find the file
    while (search_cluster < 0x0FFFFFF8) {
        unsigned int start_sector = get_cluster_sector(search_cluster);
        
        for (unsigned int i = 0; i < sectors_per_cluster; i++) {
            unsigned int sector_to_read = start_sector + i;
            
            long offset = (long)sector_to_read * sector_size;
            if (fseek(g_fs_state.image_fp, offset, SEEK_SET) != 0 ||
                fread(buffer, sector_size, 1, g_fs_state.image_fp) != 1) {
                goto search_end; 
            }
            
            for (unsigned int j = 0; j < sector_size; j += 32) {
                DIR_ENTRY *entry = (DIR_ENTRY *)(buffer + j);
                
                if (entry->DIR_Name[0] == 0x00) goto search_end; 
                if (entry->DIR_Name[0] == 0xE5) continue; 
                if ((entry->DIR_Attr & ATTR_LFN) == ATTR_LFN) continue;
                if (entry->DIR_Attr & ATTR_VOLUME_ID) continue; 

                char entry_name[13];
                get_formatted_name(entry->DIR_Name, entry_name);

                if (strcmp(entry_name, filename) == 0) {
                    if (entry->DIR_Attr & ATTR_DIRECTORY) {
                        printf("Error: Cannot open '%s' - it is a directory.\n", filename);
                        free(buffer);
                        return;
                    }
                    
                    memcpy(&found_entry, entry, sizeof(DIR_ENTRY));
                    file_found = 1;
                    goto search_end;
                }
            }
        }
        
        search_cluster = read_fat_entry(search_cluster);
    }

search_end:
    free(buffer);

    if (!file_found) {
        printf("Error: File '%s' not found.\n", filename);
        return;
    }
    
    //check the open file table
    
    int free_slot = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        OPEN_FILE *of = &g_fs_state.open_file_table[i];
        
        if (of->is_used) {
            // check if file is already open
            if (strcmp(of->name, filename) == 0 && strcmp(of->path, g_fs_state.current_path) == 0) {
                printf("Error: File '%s' is already open.\n", filename);
                return;
            }
        } else if (free_slot == -1) {
            free_slot = i;
        }
    }
    
    if (free_slot == -1) {
        printf("Error: Maximum number of open files reached (%d).\n", MAX_OPEN_FILES);
        return;
    }
    
    //commit the file to the oft
    
    OPEN_FILE *new_file = &g_fs_state.open_file_table[free_slot];
    
    // combine high and low words of the cluster number
    unsigned int starting_cluster = found_entry.DIR_FstClusHI << 16 | found_entry.DIR_FstClusLO;
    
    new_file->is_used = 1;
    new_file->mode = mode;
    new_file->offset = 0; // initialize offset at 0
    new_file->file_size = found_entry.DIR_FileSize;
    new_file->starting_cluster = starting_cluster;
    
    // copy the name and path
    strncpy(new_file->name, filename, sizeof(new_file->name) - 1);
    strncpy(new_file->path, g_fs_state.current_path, sizeof(new_file->path) - 1);
    
    printf("opened %s\n", filename);
}

// lsof command - fix path display
void lsof_command() {
    int count = 0;
    printf("INDEX  NAME          MODE    OFFSET     PATH\n");
   
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        OPEN_FILE *of = &g_fs_state.open_file_table[i];
        if (of->is_used) {
            count++;
            char mode_str[5];
            // convert enum mode to string for display
            if (of->mode == MODE_READ) strcpy(mode_str, "r");
            else if (of->mode == MODE_WRITE) strcpy(mode_str, "w");
            else if (of->mode == MODE_WRITE_READ) strcpy(mode_str, "wr");
            else strcpy(mode_str, "rw");

            printf("%-5d  %-12s  %-6s  %-9ld  %s%s\n",
                of->index, of->name, mode_str, of->offset, g_fs_state.image_name, of->path);
        }  
    }
    if (count == 0) {
        printf("No files currently open\n"); }
}


// close command
void close_command(char *filename) {
    if (filename == NULL) {
        printf("Error: Missing file name for 'close'.\n");
        return;
    }

    int found = 0;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        OPEN_FILE *of = &g_fs_state.open_file_table[i];
        if (of->is_used && strcmp(of->name, filename) == 0) {
            of->is_used = 0; // Mark the slot as free
            printf("closed %s\n", filename);
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("Error: File '%s' is not currently open or does not exist.\n", filename);
    }
}

// lseek command
void lseek_command(char *filename, char *offset_str) {
    if (filename == NULL || offset_str == NULL) {
        printf("Error: Missing filename or offset.\n");
        return;
    }
    
    //get offset
    long new_offset = atol(offset_str);

    //find file 
    OPEN_FILE *of = NULL;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (g_fs_state.open_file_table[i].is_used && strcmp(g_fs_state.open_file_table[i].name, filename) == 0) {
            of = &g_fs_state.open_file_table[i];
            break;
        }
    }
    if (of == NULL) {
        printf("Error: File '%s' is not open.\n", filename);
        return;
    }

    //check bounds
    if (new_offset < 0 || new_offset > of->file_size) {
        printf("Error: Offset %ld is outside the file size boundaries (0 to %u).\n", new_offset, of->file_size);
        return;
    }

    //update offset
    of->offset = new_offset;
    
}

// read command
void read_command(char *filename, char *size_str) {
    if (filename == NULL || size_str == NULL) {
        printf("Error: Missing filename or size.\n");
        return;
    }

    //get read size
    long bytes_to_read = atol(size_str);
    if (bytes_to_read <= 0) {
        printf("Error: Read size must be positive.\n");
        return;
    }

    //find file
    OPEN_FILE *of = NULL;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (g_fs_state.open_file_table[i].is_used && strcmp(g_fs_state.open_file_table[i].name, filename) == 0) {
            of = &g_fs_state.open_file_table[i];
            break;
        }
    }
    if (of == NULL) {
        printf("Error: File '%s' is not open.\n", filename);
        return;
    }

    //make sure its open for reading
    if (of->mode == MODE_WRITE) {
        printf("Error: File '%s' is not open for reading (-r or -rw).\n", filename);
        return;
    }

    //find read length
    long remaining_bytes_in_file = (long)of->file_size - of->offset;
    if (remaining_bytes_in_file <= 0) {
        printf("End of file reached. Read 0 bytes.\n");
        return;
    }
    
    //adjust read size if it exceeds the file boundary
    if (bytes_to_read > remaining_bytes_in_file) {
        bytes_to_read = remaining_bytes_in_file;
        printf("Warning: Reading only %ld bytes until EOF.\n", bytes_to_read);
    }
    
    //set up cluster/sector variables
    unsigned int cluster_size = g_fs_state.fs_bpb.BPB_BytsPerSec * g_fs_state.fs_bpb.BPB_SecPerClus;
    unsigned int current_cluster = of->starting_cluster;
    
    // find the cluster that = the starting offset
    current_cluster = find_cluster_from_offset(of->starting_cluster, of->offset);
    if (current_cluster >= 0x0FFFFFF8) {
        printf("Error: error while locating starting cluster.\n");
        return;
    }
    
    // calc the byte offset in the starting cluster
    long offset_in_cluster = of->offset % cluster_size;
    
    long bytes_read_total = 0;
    
    // allocate a buffer large enough for the entire read AND a NULL terminator for printing
    char *read_buffer = (char *)malloc(bytes_to_read + 1);
    if (!read_buffer) {
        printf("Error: Memory allocation failed for read buffer.\n");
        return;
    }
    read_buffer[bytes_to_read] = '\0'; // Null-terminate 

    while (bytes_read_total < bytes_to_read && current_cluster < 0x0FFFFFF8) {
        // calc the physical starting sector and offset for the read
        unsigned int cluster_start_sector = get_cluster_sector(current_cluster);
        
        // calcu space remaining in the current cluster
        long space_in_cluster = cluster_size - offset_in_cluster;
        
        //find the actual chunk size to read (min of bytes_to_read and cluster space)
        long chunk_size = bytes_to_read - bytes_read_total;
        if (chunk_size > space_in_cluster) {
            chunk_size = space_in_cluster;
        }

        // calc final physical file offset for fseek
        long file_offset = (long)cluster_start_sector * g_fs_state.fs_bpb.BPB_BytsPerSec + offset_in_cluster;
        
        //then seek and read
        if (fseek(g_fs_state.image_fp, file_offset, SEEK_SET) != 0) {
            printf("Error: Failed to seek to data cluster %u.\n", current_cluster);
            free(read_buffer);
            return;
        }

        long actual_read = fread(read_buffer + bytes_read_total, 1, chunk_size, g_fs_state.image_fp);
        
        if (actual_read <= 0) {
            //reached end of cluster chain or read error
            current_cluster = 0x0FFFFFFF; 
            break;
        }

        bytes_read_total += actual_read;
        
        // if we haven't read enough, move to the next cluster
        if (bytes_read_total < bytes_to_read) {
            current_cluster = read_fat_entry(current_cluster);
            offset_in_cluster = 0;
        }
    }

    //print and update
    read_buffer[bytes_read_total] = '\0';
    printf("%s\n", read_buffer);
 

    of->offset += bytes_read_total;

    free(read_buffer);
}

// PART FIVE COMMANDS -----------------------------------------
// PART SIX COMMANDS -----------------------------------------

// MAIN PROGRAM LOOP -----------------------------------------
int start_program_shell(int argc, char *argv[]) {
    FILE *image_fp = NULL;

    if (argc != 2) {
        fprintf(stderr, "Error: Incorrect number of arguments.\n");
        fprintf(stderr, "Usage: %s [FAT32 ISO]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char *image_path = argv[1];
    image_fp = fopen(image_path, "r+");

    if (image_fp == NULL) {
        fprintf(stderr, "Error: Could not open file image '%s'.\n", image_path);
        exit(EXIT_FAILURE); 
    }

    if (load_bpb_and_init_state(image_path, image_fp) != 0) {
        fprintf(stderr, "Initialization failed.\n");
        exit_shell(); 
    }
    
    char line[1024];
    while (1) {
        printf("%s%s>", g_fs_state.image_name, g_fs_state.current_path);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            exit_shell(); 
        }
        
        trim_whitespace(line); 
        
        tokenlist *tokens_list = get_tokens(line); 
        if (tokens_list == NULL || tokens_list->size == 0 || tokens_list->items[0] == NULL) {
            if (tokens_list) free_tokens(tokens_list); 
            continue; 
        }

        char *command = tokens_list->items[0];

        // part one commands
        if (strcmp(command, "exit") == 0) {
             free_tokens(tokens_list); 
             exit_shell(); 
        } 
        else if (strcmp(command, "info") == 0) {
             info_command(); 
        }
        // part two commands
        else if (strcmp(command, "ls") == 0) {
             ls_command(); 
        }
        else if (strcmp(command, "cd") == 0) {
            if (tokens_list->size < 2 || tokens_list->items[1] == NULL) {
                printf("Error: 'cd' command requires a directory name.\n");
            } else {
                cd_command(tokens_list->items[1]);
            }
        }
        // part three commands
        else if (strcmp(command, "creat") == 0) {
            if (tokens_list->size < 2 || tokens_list->items[1] == NULL) {
                printf("Error: 'creat' command requires a file name.\n");
            } else {
                creat_command(tokens_list->items[1]);
            }
        }
        else if (strcmp(command, "mkdir") == 0) {  
            if (tokens_list->size < 2 || tokens_list->items[1] == NULL) {
                printf("Error: 'mkdir' command requires a directory name.\n");
            } else {
                mkdir_command(tokens_list->items[1]);
            }
        }
        // part four commands
        else if (strcmp(command, "lsof") == 0) {
             lsof_command(); 
        }
        else if (strcmp(command, "close") == 0) {
            if (tokens_list->size < 2) {
                printf("Error: Missing file name for 'close'.\n");
            } else {
                close_command(tokens_list->items[1]);
            }
        }
        else if (strcmp(command, "open") == 0) {
            if (tokens_list->size < 3) {
                printf("Error: 'open' command requires a file name and access mode.\n");
            } else {
                open_command(tokens_list->items[1], tokens_list->items[2]);
            }
        }
        else if (strcmp(command, "lseek") == 0) {
            if (tokens_list->size < 3) {
                printf("Error: 'lseek' command requires a file name and offset.\n");
            } else {
                lseek_command(tokens_list->items[1], tokens_list->items[2]);
            }
        }
        else if (strcmp(command, "read") == 0) {
            if (tokens_list->size < 3) {
                printf("Error: 'read' command requires a file name and size.\n");
            } else {
                read_command(tokens_list->items[1], tokens_list->items[2]);
            }
        }
        // part five commands (ill add under here)
        //part six commands (ill add under here)
        // unrecognized command
        else {
            printf("Error: Command '%s' not implemented or recognized.\n", command);
        }
        
        free_tokens(tokens_list); 
    }
    
    return 0; 
} 
