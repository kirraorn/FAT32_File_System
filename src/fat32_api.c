#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h" 
#include "commands.h" 

//global state variable
FS_STATE g_fs_state; 

//PART ONE:

unsigned int load_bpb_and_init_state(const char *image_name, FILE *fp) {
    //store initial state info
    g_fs_state.image_fp = fp;
    strncpy(g_fs_state.image_name, image_name, sizeof(g_fs_state.image_name) - 1);
    
    //initialize current path to root
    g_fs_state.current_path[0] = '/';
    g_fs_state.current_path[1] = '\0'; 

    //read the BPB
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Fatal Error: Failed to seek to start of image.\n");
        return 1; // Error
    }

    if (fread(&g_fs_state.fs_bpb, sizeof(BPB), 1, fp) != 1) {
        fprintf(stderr, "Error: Failed to read the entire BPB structure.\n");
        return 1;
    }

    //verify the BPB signature
    if (g_fs_state.fs_bpb.signature != 0xAA55) {
        fprintf(stderr, "Error: Invalid FAT32 signature (0x%X).\n", g_fs_state.fs_bpb.signature);
        return 1;
    }
    
    //init current cluster to root
    g_fs_state.current_cluster = g_fs_state.fs_bpb.BPB_RootClus; 

    //init open file table
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        g_fs_state.open_file_table[i].is_used = 0;
        g_fs_state.open_file_table[i].index = i;
    }

    return 0;
}

//calculates the byte offset for any given sector number
long get_sector_offset(unsigned int sector_num) {
    // Cast to long long for robust multiplication, then cast to long for fseek
    return (long long)sector_num * g_fs_state.fs_bpb.BPB_BytsPerSec;
}

unsigned int get_first_data_sector() {
    // FirstDataSector = ReservedSectors + (Number of FATs * SectorsPerFAT)
    return (unsigned int)g_fs_state.fs_bpb.BPB_RsvdSecCnt + 
           ((unsigned int)g_fs_state.fs_bpb.BPB_NumFATs * g_fs_state.fs_bpb.BPB_FATSz32);
}

unsigned int get_total_clusters() {
    unsigned int TotalSectors = g_fs_state.fs_bpb.BPB_TotSecs32;
    unsigned int FirstDataSector = get_first_data_sector();
    unsigned int DataSectors = TotalSectors - FirstDataSector;
    
    return DataSectors / (unsigned int)g_fs_state.fs_bpb.BPB_SecPerClus;
}

//PART TWO:

// translates a logical cluster number (N) to the physical first  sector
unsigned int get_cluster_sector(unsigned int cluster_num) {
    unsigned int first_data_sector = get_first_data_sector();
    
    if (cluster_num < 2) {
        return first_data_sector; 
    }

    return ((cluster_num - 2) * g_fs_state.fs_bpb.BPB_SecPerClus) + first_data_sector;
}

// looks up the given cluster number in the FAT to find the NEXT cluster in the chain
unsigned int read_fat_entry(unsigned int cluster_num) {
    unsigned int fat_offset, fat_sector, ent_offset;
    unsigned int next_cluster;

    //calc fat offset
    fat_offset = cluster_num * 4;

    //calc which sector of the FAT contains this entry
    fat_sector = g_fs_state.fs_bpb.BPB_RsvdSecCnt + (fat_offset / g_fs_state.fs_bpb.BPB_BytsPerSec);

    //calc byte offset within the sector
    ent_offset = fat_offset % g_fs_state.fs_bpb.BPB_BytsPerSec;

    //calc the exact byte offset in the image
    long long file_offset_ll = (long long)fat_sector * g_fs_state.fs_bpb.BPB_BytsPerSec + ent_offset;

    //read entry 
    if (fseek(g_fs_state.image_fp, (long)file_offset_ll, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Failed to seek to FAT entry for cluster %u\n", cluster_num);
        return 0x0FFFFFFF;
    }

    if (fread(&next_cluster, sizeof(unsigned int), 1, g_fs_state.image_fp) != 1) {
        fprintf(stderr, "Error: Failed to read FAT entry for cluster %u\n", cluster_num);
        return 0x0FFFFFFF;
    }

    //mask to get actual cluster number
    return next_cluster & 0x0FFFFFFF;
}

// PART THREE:

//writes 32 bit value to cluster number in FAT
void write_fat_entry(unsigned int cluster_num, unsigned int value) {
    unsigned int fat_offset, fat_sector, ent_offset;
    unsigned int fat_size_sectors = g_fs_state.fs_bpb.BPB_FATSz32;

    //only need 28 
    unsigned int masked_value = value & 0x0FFFFFFF;

    // calc byte offeset
    fat_offset = cluster_num * 4;

    //loop through fat copies:
    for (unsigned int i = 0; i < g_fs_state.fs_bpb.BPB_NumFATs; i++) {
        //calc which sector of the FAT contains this entry
        fat_sector = g_fs_state.fs_bpb.BPB_RsvdSecCnt + (i * fat_size_sectors) + 
        (fat_offset / g_fs_state.fs_bpb.BPB_BytsPerSec);

        //calc byte offset within the sector
        ent_offset = fat_offset % g_fs_state.fs_bpb.BPB_BytsPerSec;

        //calc the exact byte offset in the image
        long long file_offset_ll = (long long)fat_sector * g_fs_state.fs_bpb.BPB_BytsPerSec + ent_offset;

        //write the value
        if (fseek(g_fs_state.image_fp, (long)file_offset_ll, SEEK_SET) != 0) {
            fprintf(stderr, "Error: Failed to seek to FAT entry for cluster %u\n", cluster_num);
            return;
        }

        if (fwrite(&masked_value, sizeof(unsigned int), 1, g_fs_state.image_fp) != 1) {
            fprintf(stderr, "Error: Failed to write FAT entry for cluster %u\n", cluster_num);
            return;
        }
    }
}

//finds the first free cluster in the FAT
unsigned int get_free_cluster() {
    unsigned int total_clusters = get_total_clusters();

    for (unsigned int cluster_num = 2; cluster_num < total_clusters + 2; cluster_num++) {
        if (read_fat_entry(cluster_num) == 0) {
            return cluster_num;
        }
    }

    return 0x0FFFFFFF;
}

//PART FOUR:

// helper for read command
unsigned int find_cluster_from_offset(unsigned int starting_cluster, long offset) {
    unsigned int cluster_size = g_fs_state.fs_bpb.BPB_BytsPerSec * g_fs_state.fs_bpb.BPB_SecPerClus;
    
    // Determine how many full clusters precede the offset
    unsigned int clusters_to_skip = offset / cluster_size;
    
    unsigned int current_cluster = starting_cluster;

    // Traverse the cluster chain, skipping the required number of clusters
    for (unsigned int i = 0; i < clusters_to_skip; i++) {
        current_cluster = read_fat_entry(current_cluster);
        
        // If we hit EOC before reaching the target, the offset is invalid
        if (current_cluster >= 0x0FFFFFF8) {
            return 0x0FFFFFFF;
        }
    }
    
    return current_cluster;
}
