// os345fat.c - file management system
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************
//
//		11/19/2011	moved getNextDirEntry to P6
//
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345fat.h"

// ***********************************************************************
// ***********************************************************************
//	functions to implement in Project 6
//
int fmsCloseFile(int);
int fmsDefineFile(char*, int);
int fmsDeleteFile(char*);
int fmsOpenFile(char*, int);
int fmsReadFile(int, char*, int);
int fmsSeekFile(int, int);
int fmsWriteFile(int, char*, int);

// ***********************************************************************
// ***********************************************************************
//	Support functions available in os345p6.c
//
extern int fmsGetDirEntry(char* fileName, DirEntry* dirEntry);
extern int fmsGetNextDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

extern int fmsMount(char* fileName, void* ramDisk);

extern void setFatEntry(int FATindex, unsigned short FAT12ClusEntryVal, unsigned char* FAT);
extern unsigned short getFatEntry(int FATindex, unsigned char* FATtable);

extern int fmsMask(char* mask, char* name, char* ext);
extern void setDirTimeDate(DirEntry* dir);
extern int isValidFileName(char* fileName);
extern void printDirectoryEntry(DirEntry*);
extern void fmsError(int);

extern int fmsReadSector(void* buffer, int sectorNumber);
extern int fmsWriteSector(void* buffer, int sectorNumber);

// ***********************************************************************
// ***********************************************************************
// fms variables
//
// RAM disk
unsigned char RAMDisk[SECTORS_PER_DISK * BYTES_PER_SECTOR];

// File Allocation Tables (FAT1 & FAT2)
unsigned char FAT1[NUM_FAT_SECTORS * BYTES_PER_SECTOR];
unsigned char FAT2[NUM_FAT_SECTORS * BYTES_PER_SECTOR];

char dirPath[128];							// current directory path
FDEntry OFTable[NFILES];					// open file table
int numOpenFiles;

extern bool diskMounted;					// disk has been mounted
extern TCB tcb[];							// task control block
extern int curTask;							// current task #


// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{

	if (fileDescriptor < 0 || fileDescriptor >= NFILES) {
		return ERR52;
	}

	FDEntry* fd = &OFTable[fileDescriptor];
	int error;

	if (fd->name[0] == '\0') {
		return ERR63;
	}

	if (fd->flags & BUFFER_ALTERED) {
		// write buffer to sector
		if ((error = fmsWriteSector(fd->buffer, C_2_S(fd->currentCluster)))) {
			return error;
		}
		memset(fd->buffer, 0, BYTES_PER_SECTOR);
	}

	if (fd->flags & FILE_ALTERED) {
		// update DirEntry for file
		int dirCluster = fd->directoryCluster;
		char* buffer[BYTES_PER_SECTOR];

		int index;
		DirEntry dirEntry;
		if ((error = fmsGetNextDirEntry(&index, fd->name, &dirEntry, dirCluster))) {
			return error;
		}

		int clustersPast = index / ENTRIES_PER_SECTOR;
		int clusterIndex = index % ENTRIES_PER_SECTOR;
		bool inRootDir = (dirCluster == 0);
		int writeCluster = dirCluster;
		int writeSector;

		if (!inRootDir) {
			while (clustersPast > 0) {
				writeCluster = getFatEntry(writeCluster, FAT1);
				clustersPast--;
			}
			writeSector = C_2_S(writeCluster);
		}
		else {
			writeSector = 19 + clustersPast;
		}

		if (error = fmsReadSector(buffer, writeSector)) {
			return error;
		}

		dirEntry.fileSize = fd->fileSize;
		setDirTimeDate(&dirEntry);

		memcpy(buffer + (clusterIndex * sizeof(DirEntry)), &dirEntry, sizeof(DirEntry));

		if ((error = fmsWriteSector(buffer, writeSector))) {
			return error;
		}

	}

	OFTable[fileDescriptor].name[0] = '\0';
	numOpenFiles--;

	return 0;
} // end fmsCloseFile

int initCluster(int c_num) {
	// initialize sector to 0
	char buffer[BYTES_PER_SECTOR] = {0};
	fmsWriteSector(buffer, C_2_S(c_num));
}

int getNextFreeCluster() {
	int numFatEntries = NUM_FAT_SECTORS * BYTES_PER_SECTOR;
	for (int i = 2; i < numFatEntries; i++) {
		if (getFatEntry(i, FAT1) == 0) {
			return i;
		}
	}

	return ERR65;
}

int linkNewCluster(int curr) {
	int next = getNextFreeCluster();

	// initialize sector to 0
	initCluster(next);

	// set link in FAT1 and FAT2
	setFatEntry(curr, next, FAT1);
	setFatEntry(curr, next, FAT2);
	setFatEntry(next, FAT_EOC, FAT1);
	setFatEntry(next, FAT_EOC, FAT2);

	return next;
}

DirEntry createDirEntry(char* fileName, int attributes, int startCluster, int fileSize) {
	DirEntry entry;

	memset(entry.name, 0x20, 8*sizeof(uint8));
	memset(entry.extension, 0x20, 3*sizeof(uint8));

	int length = strlen(fileName);
	int nameSize = 0;
	int extensionStart = -1;

	for (int i = 0; i < length; i++) {
		char c = toupper(fileName[i]);
		if (c == '.') {
			// '.' or '..'
			if (nameSize == 0) {
				entry.name[i] = c;
			}
			// normal file name
			else {
				extensionStart = i + 1;
			}
		}
		else {
			// char is in name
			if (extensionStart == -1) {
				nameSize++;
				entry.name[i] = c;
			}
			// char is in ext
			else {
				entry.extension[i-extensionStart] = c;
			}
		}
	}

	entry.attributes = attributes;
	memset(entry.reserved, 0, 10);
	setDirTimeDate(&entry);
	entry.startCluster = startCluster;
	entry.fileSize = fileSize;

	return entry;
}

// ***********************************************************************
// ***********************************************************************
// If attribute=DIRECTORY, this function creates a new directory
// file directoryName in the current directory.
// The directory entries "." and ".." are also defined.
// It is an error to try and create a directory that already exists.
//
// else, this function creates a new file fileName in the current directory.
// It is an error to try and create a file that already exists.
// The start cluster field should be initialized to cluster 0.  In FAT-12,
// files of size 0 should point to cluster 0 (otherwise chkdsk should report an error).
// Remember to change the start cluster field from 0 to a free cluster when writing to the
// file.
//
// Return 0 for success, otherwise, return the error number.
//
int fmsDefineFile(char* fileName, int attribute)
{
	if (isValidFileName(fileName) != 1) {
		return ERR50;
	}

	int cdir = CDIR;
	int writeSector;
	int index = 0;
	DirEntry dirEntry;
	int error = fmsGetNextDirEntry(&index, fileName, &dirEntry, cdir);
	if (error == 0) {
		return ERR60; // file already defined
	}

	if (error != ERR67) { // end of file
		return error;
	}

	// printf("\nindex: %d\n", index);

	/* Go to end of cluster chain */
	int clustersPast = index / ENTRIES_PER_SECTOR;
	int clusterIndex = index % ENTRIES_PER_SECTOR;
	bool inRootDir = (CDIR == 0);

	if (inRootDir) {
		// printf("\ncdir: %d  clustersPast: %d\n", cdir, clustersPast);
		cdir += clustersPast;
		writeSector = cdir + 19;
		// printf("\nwriteSector: %d\n", writeSector);
	}
	else {
		while (clustersPast > 0) {
			int next = getFatEntry(cdir, FAT1);
			if (next == FAT_EOC) {
				cdir = linkNewCluster(cdir);
			}
			else {
				cdir = next;
			}
			clustersPast--;
		}

		writeSector = C_2_S(cdir);
	}

	// check for end of root directory
	if (cdir == 0 && index == 224) {
		return ERR64;
	}

	// printf("\n*****Write to cluster****\n");
	/* Write to clusters */
	int startCluster = 0;
	bool isDir = (attribute & DIRECTORY);
	char buffer[BYTES_PER_SECTOR] = {0};

	// new file is a directory
	if (isDir) {
		startCluster = getNextFreeCluster();
		setFatEntry(startCluster, FAT_EOC, FAT1);
		setFatEntry(startCluster, FAT_EOC, FAT2);
		initCluster(startCluster);
		char dot[2] = {'.', 0};
		char dotdot[3] = {'.', '.', 0};
		DirEntry dotEntry = createDirEntry(dot, DIRECTORY, startCluster, 0);
		DirEntry dotdotEntry = createDirEntry(dotdot, DIRECTORY, CDIR, 0);

		memcpy(buffer, &dotEntry, sizeof(DirEntry));
		memcpy(buffer + sizeof(DirEntry), &dotdotEntry, sizeof(DirEntry));

		if ((error = fmsWriteSector(buffer, C_2_S(startCluster)))) {
			return error;
		}

	}
	// printf("\n*****Writing to sector %d*****\n", writeSector);
	dirEntry = createDirEntry(fileName, attribute, startCluster, 0);

	// printDirectoryEntry(&dirEntry);
	if ((error = fmsReadSector(buffer, writeSector))) {
		return error;
	}

	memcpy(buffer + (clusterIndex * sizeof(DirEntry)), &dirEntry, sizeof(DirEntry));

	if ((error = fmsWriteSector(buffer, writeSector))) {
		return error;
	}

	return 0;
} // end fmsDefineFile

// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current director.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	if (isValidFileName(fileName) != 1) {
		return ERR50;
	}

	int error;
	int cdir = CDIR;
	int index = 0;
	DirEntry dirEntry;
	if ((error = fmsGetNextDirEntry(&index, fileName, &dirEntry, cdir))) {
		return error;
	}

	index--;

	int startCluster = dirEntry.startCluster;

	/* Handle directory */
	if (dirEntry.attributes & DIRECTORY) {
		
		DirEntry innerDirEntry;
		int innerIndex = 2;
		error = fmsGetNextDirEntry(&innerIndex, "*.*", &innerDirEntry, startCluster);

		if (error != ERR67) {
			return ERR69;
		}
	}

	/* Free cluster chain */
	if (startCluster != 0) {
		int tempCluster = startCluster;
		while (tempCluster != FAT_EOC) {
			int nextCluster = getFatEntry(tempCluster, FAT1);
			setFatEntry(tempCluster, 0, FAT1);
			tempCluster = nextCluster;
		}
	}

	/* Delete entry from directory cluster */
	dirEntry.name[0] = 0xe5;

	/* Go to end of cluster chain */
	int clustersPast = index / ENTRIES_PER_SECTOR;
	int clusterIndex = index % ENTRIES_PER_SECTOR;
	bool inRootDir = (CDIR == 0);
	int writeSector;
	char buffer[BYTES_PER_SECTOR] = {0};

	if (inRootDir) {
		cdir += clustersPast;
		writeSector = cdir + 19;
	}
	else {
		while (clustersPast > 0) {
			int next = getFatEntry(cdir, FAT1);
			if (next == FAT_EOC) {
				cdir = linkNewCluster(cdir);
			}
			else {
				cdir = next;
			}
			clustersPast--;
		}

		writeSector = C_2_S(cdir);
	}

	if ((error = fmsReadSector(buffer, writeSector))) {
		return error;
	}

	memcpy(buffer + (clusterIndex * sizeof(DirEntry)), &dirEntry, sizeof(DirEntry));

	if ((error = fmsWriteSector(buffer, writeSector))) {
		return error;
	}

	return 0;
} // end fmsDeleteFile



// ***********************************************************************
// ***********************************************************************
// This function opens the file fileName for access as specified by rwMode.
// It is an error to try to open a file that does not exist.
// The open mode rwMode is defined as follows:
//    0 - Read access only.
//       The file pointer is initialized to the beginning of the file.
//       Writing to this file is not allowed.
//    1 - Write access only.
//       The file pointer is initialized to the beginning of the file.
//       Reading from this file is not allowed.
//    2 - Append access.
//       The file pointer is moved to the end of the file.
//       Reading from this file is not allowed.
//    3 - Read/Write access.
//       The file pointer is initialized to the beginning of the file.
//       Both read and writing to the file is allowed.
// A maximum of 32 files may be open at any one time.
// If successful, return a file descriptor that is used in calling subsequent file
// handling functions; otherwise, return the error number.
//
int fmsOpenFile(char* fileName, int rwMode)
{
	DirEntry curDirEntry;
	int error;
	if ((error = fmsGetDirEntry(fileName, &curDirEntry))) {
		return error;
	}

	if (numOpenFiles == NFILES) {
		return ERR70;
	}

	for (int i = 0; i < NFILES; i++) {
		if (OFTable[i].name[0] == '\0') {
			memcpy(OFTable[i].name, curDirEntry.name, 8);
			memcpy(OFTable[i].extension, curDirEntry.extension, 3);
			OFTable[i].attributes = curDirEntry.attributes;
			OFTable[i].directoryCluster = CDIR;
			OFTable[i].startCluster = curDirEntry.startCluster;
			OFTable[i].currentCluster = 0;
			OFTable[i].fileSize = (rwMode == OPEN_WRITE) ? 0 : curDirEntry.fileSize;
			OFTable[i].pid = curTask;
			OFTable[i].mode = rwMode;
			OFTable[i].flags = 0;
			OFTable[i].fileIndex = (rwMode != OPEN_RDWR) ? 0 : curDirEntry.fileSize;
			numOpenFiles++;
			return i;
		}
	}

	return ERR61;
} // end fmsOpenFile



// ***********************************************************************
// ***********************************************************************
// This function reads nBytes bytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (if > 0) or return an error number.
// (If you are already at the end of the file, return EOF error.  ie. you should never
// return a 0.)
//
int fmsReadFile(int fileDescriptor, char* buffer, int nBytesToRead)
{
	int error, nextCluster;
	FDEntry* fdEntry;
	int numBytesRead = 0;
	unsigned int bytesLeft, bufferIndex;
	fdEntry = &OFTable[fileDescriptor];

	if (fdEntry->name[0] == 0) {
		return ERR63;
	}

	if ((fdEntry->mode == 1) || (fdEntry->mode == 2)) {
		return ERR85;
	}

	while (nBytesToRead > 0) {
		if (fdEntry->fileSize == fdEntry->fileIndex) {
			return (numBytesRead ? numBytesRead : ERR66); // EOF
		}

		bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;

		if ((bufferIndex == 0) && (fdEntry->fileIndex || !fdEntry->currentCluster)) {
			if (fdEntry->currentCluster == 0) {
				if (fdEntry->startCluster == 0) {
					return ERR66;
				}
				nextCluster = fdEntry->startCluster;
				fdEntry->fileIndex = 0;
			}
			else {
				nextCluster = getFatEntry(fdEntry->currentCluster, FAT1);
				if (nextCluster == FAT_EOC) {
					return numBytesRead;
				}
			}
			if (fdEntry->flags & BUFFER_ALTERED) {
				if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) {
					return error;
				}
				fdEntry->flags &= ~BUFFER_ALTERED;
			}
			if ((error = fmsReadSector(fdEntry->buffer, C_2_S(nextCluster)))) {
				return error;
			}
			fdEntry->currentCluster = nextCluster;
		}

		bytesLeft = BYTES_PER_SECTOR - bufferIndex;
		if (bytesLeft > nBytesToRead) {
			bytesLeft = nBytesToRead;
		}
		if (bytesLeft > (fdEntry->fileSize - fdEntry->fileIndex)) {
			bytesLeft = fdEntry->fileSize - fdEntry->fileIndex;
		}

		memcpy(buffer, &fdEntry->buffer[bufferIndex], bytesLeft);
		fdEntry->fileIndex += bytesLeft;
		numBytesRead += bytesLeft;
		buffer += bytesLeft;
		nBytesToRead -= bytesLeft;
	}

	return numBytesRead;
} // end fmsReadFile



// ***********************************************************************
// ***********************************************************************
// This function changes the current file pointer of the open file specified by
// fileDescriptor to the new file position specified by index.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// The file position may not be positioned beyond the end of the file.
// Return the new position in the file if successful; otherwise, return the error number.
//
int fmsSeekFile(int fileDescriptor, int index)
{
	// ?? add code here
	printf("\nfmsSeekFile Not Implemented");

	return ERR63;
} // end fmsSeekFile



// ***********************************************************************
// ***********************************************************************
// This function writes nBytes bytes to the open file specified by fileDescriptor from
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Writing is always "overwriting" not "inserting" in the file and always writes forward
// from the current file pointer position.
// Return the number of bytes successfully written; otherwise, return the error number.
//
int fmsWriteFile(int fileDescriptor, char* buffer, int nBytesToWrite)
{
	if (fileDescriptor < 0 || fileDescriptor >= NFILES) {
		return ERR52;
	}

	if (OFTable[fileDescriptor].name[0] == '\0') {
		return ERR63;
	}

	if (OFTable[fileDescriptor].mode == OPEN_READ) {
		return ERR84;
	}

	int numBytesWritten;

	// while (nBytesToWrite > 0) {

	// 	bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;

	// 	if ((bufferIndex == 0) && (fdEntry->fileIndex || !fdEntry->currentCluster)) {
	// 		if (fdEntry->currentCluster == 0) {
	// 			if (fdEntry->startCluster == 0) {
	// 				return ERR66;
	// 			}
	// 			nextCluster = fdEntry->startCluster;
	// 			fdEntry->fileIndex = 0;
	// 		}
	// 		else {
	// 			nextCluster = getFatEntry(fdEntry->currentCluster, FAT1);
	// 			if (nextCluster == FAT_EOC) {
	// 				return numBytesWritten;
	// 			}
	// 		}
	// 		if (fdEntry->flags & BUFFER_ALTERED) {
	// 			if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) {
	// 				return error;
	// 			}
	// 			fdEntry->flags &= ~BUFFER_ALTERED;
	// 		}
	// 		if ((error = fmsReadSector(fdEntry->buffer, C_2_S(nextCluster)))) {
	// 			return error;
	// 		}
	// 		fdEntry->currentCluster = nextCluster;
	// 	}

	// 	bytesLeft = BYTES_PER_SECTOR - bufferIndex;
	// 	if (bytesLeft > nBytesToWrite) {
	// 		bytesLeft = nBytesToWrite;
	// 	}
	// 	if (bytesLeft > (fdEntry->fileSize - fdEntry->fileIndex)) {
	// 		bytesLeft = fdEntry->fileSize - fdEntry->fileIndex;
	// 	}

	// 	memcpy(buffer, &fdEntry->buffer[bufferIndex], bytesLeft);
	// 	fdEntry->fileIndex += bytesLeft;
	// 	numBytesWritten += bytesLeft;
	// 	buffer += bytesLeft;
	// 	nBytesToWrite -= bytesLeft;
	// }

	return numBytesWritten;
} // end fmsWriteFile
















