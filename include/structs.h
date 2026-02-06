#ifndef STRUCTS_H
#define STRUCTS_H


// BPS struct
typedef struct __attribute__((packed)) {
    // 0x00 - 0x0A: Initial fields
    unsigned char  BS_JmpBoot[3];      // 3 bytes
    unsigned char  BS_OEMName[8];      // 8 bytes
    
    //Standard BPB Fields
    unsigned short BPB_BytsPerSec;     // 2 bytes (Offset 0x0B)
    unsigned char  BPB_SecPerClus;     // 1 byte (Offset 0x0D)
    unsigned short BPB_RsvdSecCnt;     // 2 bytes (Offset 0x0E)
    unsigned char  BPB_NumFATs;        // 1 byte (Offset 0x10)
    unsigned short BPB_RootEntCnt;     // 2 bytes 
    unsigned short BPB_TotSecs16;      // 2 bytes 
    unsigned char  BPB_Media;          // 1 byte
    unsigned short BPB_FATSz16;        // 2 bytes 
    unsigned short BPB_SecPerTrk;      // 2 bytes 
    unsigned short BPB_NumHeads;       // 2 bytes 
    unsigned int   BPB_HiddSec;        // 4 bytes (Offset 0x1C)
    unsigned int   BPB_TotSecs32;      // 4 bytes (Offset 0x20)
    
    //FAT32 Extended BPB Fields (EBPB)
    unsigned int   BPB_FATSz32;        // 4 bytes (Offset 0x24)
    unsigned short BPB_ExtFlags;       // 2 bytes 
    unsigned short BPB_FSVer;          // 2 bytes 
    unsigned int   BPB_RootClus;       // 4 bytes (Offset 0x2C)
    unsigned short BPB_FSInfo;         // 2 bytes 
    unsigned short BPB_BkBootSec;      // 2 bytes 
    unsigned char  BPB_Reserved[12];   // 12 bytes
    unsigned char  BS_DrvNum;          // 1 byte
    unsigned char  BS_Reserved1;       // 1 byte
    unsigned char  BS_BootSig;         // 1 byte
    unsigned int   BS_VolID;           // 4 bytes 
    unsigned char  BS_VolLab[11];      // 11 bytes
    unsigned char  BS_FilSysType[8];   // 8 bytes 
    
    // the rest of the sector (448 bytes of bootstrap code and signature)
    unsigned char  bootstrap_code[420]; // 420 bytes
    unsigned short signature;           // 2 bytes (Offset 0x1FE)
} BPB;

// directory entry struct
typedef struct __attribute__((packed)) {
    unsigned char DIR_Name[11];         // 11 bytes - file name
    unsigned char DIR_Attr;            // 1 byte - file attributes
    unsigned char DIR_NTRes;           // 1 byte - reserved f
    unsigned char DIR_CrtTimeTenth;    // 1 byte
    unsigned short DIR_CrtTime;        // 2 bytes
    unsigned short DIR_CrtDate;        // 2 bytes
    unsigned short DIR_LstAccDate;     // 2 bytes
    unsigned short DIR_FstClusHI;      // 2 bytes
    unsigned short DIR_WrtTime;        // 2 bytes
    unsigned short DIR_WrtDate;        // 2 bytes
    unsigned short DIR_FstClusLO;      // 2 bytes
    unsigned int   DIR_FileSize;       // 4 bytes - size of file in bytes
} DIR_ENTRY;

//defines the allowed access modes
typedef enum {
    MODE_READ,
    MODE_WRITE,
    MODE_READ_WRITE,
    MODE_WRITE_READ
} FILE_ACCESS_MODE;

// struct to hold state of open files
typedef struct {
    int index; // index in the open file table
    int is_used; // flag to indicate if this entry is in use
    char name[13]; // file name
    char path[256]; //full path
    FILE_ACCESS_MODE mode; // access mode
    long offset; // read/write offset
    unsigned int starting_cluster;
    unsigned int file_size;
} OPEN_FILE;

// max number of open files
#define MAX_OPEN_FILES 10 // for our prohect its 10
// global state struct
typedef struct {
    BPB fs_bpb;
    unsigned int current_cluster;
    char current_path[256];
    char image_name[256];
    FILE *image_fp;

    OPEN_FILE open_file_table[MAX_OPEN_FILES]; // array to hold open file states
} FS_STATE;

extern FS_STATE g_fs_state;

//attributes for directory entries
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LFN       0x0F // long file - skip entries

#endif // STRUCTS_H