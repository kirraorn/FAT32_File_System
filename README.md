# FAT32 File System Utility

    Creating a robust file system for Fat32 

## Group Members
- **Kirra Orndorff**
- **Kate**
- **Ludginie**
  
## Division of Labor

### Part 1: Mounting the Image
- **Responsibilities**: The user will need to mount the image file through command line arguments. Need to implement info and exit commands
- **Assigned to**: Kirra Orndorff

### Part 2: Navigation
- **Responsibilities**: Need to implement cd and ls commands
- **Assigned to**: Kirra Orndorff, Ludginie 

### Part 3: Create
- **Responsibilities**: implement mkdir and creat
- **Assigned to**: Kirra Orndorff, Kate 

### Part 4: Read
- **Responsibilities**: implement open, close, lsof, lseek, and read 
- **Assigned to**: Kirra Orndorff, Kate

### Part 5: Update
- **Responsibilities**: implement write and mv
- **Assigned to**: Ludginie, Kate 

### Part 6: Delete
- **Responsibilities**: implement rm and rmdir
- **Assigned to**: Ludginie 

### Extra Credit
- **Responsibilities**: 
- **Assigned to**: All members

## File Listing
```
filesys/
│
├── src/
│ ├── lexer.c
│ └── commands.c
│ └── fat32_api.c
│
├── include/
│ └── lexer.h
│ └── structs.h
| └── commands.h
│
├── README.md
└── Makefile
```
## How to Compile & Execute

### Requirements
- **Compiler**: e.g., `gcc` for C/C++, `rustc` for Rust.
- **Dependencies**: List any libraries or frameworks necessary (rust only).

### Compilation
For a C/C++ example:
```bash
make
```
This will run the program ...
### Execution
```bash
./bin/filesys fat32.img
```

## Development Log
Each member records their contributions here.

### Kirra Orndorff

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2025-11-16 | Completed Part One     |
| 2025-11-17 | Began work on part 2   |
| 2025-11-18 | finished part 2/did read command   |
| 2025-11-19 | continued read/lsof commands  |
| 2025-11-20 | finished read/lsof   |
| 2025-11-21 | began creat command  |
| 2025-11-212| completed creat command  |


## Meetings
Document in-person meetings, their purpose, and what was discussed.

| Date       | Attendees            | Topics Discussed | Outcomes / Decisions |
|------------|----------------------|------------------|-----------------------|
| 2025-11-10 | Kirra, Ludginie, Kate  | Determine DOL   | DOL completed, each member knows parts |
| 2025-11-19 | Kirra, Ludginie, Kate  | discuss commands  | decided what work to complete before break |
| 2025-12-1 | Kirra, Ludginie, Kate  | talk about commands that still need to be implemented | determined timeline of completion/testing |


## Considerations
- Only allowed to use the standard C library and standard Rust library to implement Project 3.
- Assume that at most 10 files will be opened.
- Assume that FILENAME will be less than 11 characters (7 for name, 3 for extension).
- Do not need to worry about “/” expansions into deeper directories.
- Makefile must produce an executable named “filesys”.
- File and directory names will not contain spaces or file extensions (ie .txt).
 - File and directory names will be names (not paths) within the current working directory.
- Do not need to support long directory names, but these entries may exist in the image; you can safely skip over these entries.
- [STRING] will always be contained within “” quotation marks.
- Unless specified in command description, if the arguments are of incorrect size, print out an error.
- Expect to receive the path of the image file in the main program’s first argument (argv[1]).
