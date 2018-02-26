#pragma once
#ifndef _DISKMANAGEMENT_H_
#define _DISKMANAGEMENT_H_
#include "global.h"
#include "z502.h"
#include "syscalls.h"
#include "global.h"
#include "z502.h"
#include <math.h>
#include "protos.h"

typedef struct
{
	int DiskID;           //1 bytes in field
	int BitmapSize;      //1 bytes in field 
	int RootDirSize;    //1 bytes in field
	int SwapSize;         //1 bytes in field
	short DiskLength;      //2 bytes in field
	short BitmapLocation;  //2 bytes in field
	short RootDirLocation; //2 bytes in field
	short SwapLocation;  // 2 bytes in field
    char RESERVED;         //2 bytes in field

}Block0;

typedef union {
	char char_data[PGSIZE];
	UINT32 int_data[PGSIZE / sizeof(int)];
} DISK_DATA;

typedef struct{

	INT32 inode;
	char name[7];
	int FileDescription;
	long CreationTime;
	int indexLocation;
	int FileSize;
	int FileLocation;

}Header;

typedef struct
{
	int DiskID;
	int BitMapLocation;
	int BitMapSize;
	int UnusedBlocks;
	int DiskUnusedLocation;

}DiskInfo;

typedef struct FileInfo
{
	//INT32 physical_page;
	//INT32 DataWritten;
	Header FileHeader;
	struct FileInfo *next;

}FileInfo, *fileNode;

typedef struct
{
	fileNode front;
	fileNode rear;
	int size; 

}FileQueue;

void Initial_Block0(Block0 *block0, long DiskID);

INT32 SetBinaryNumber(int numberOf1);

void initialHeader(Header *header, Block0 *block0);


FileQueue *InitialFileQueue();
fileNode CreateNewFile(FileQueue *queue, Header *FileInfo); // create new file or new directory
fileNode DeleteFileByName(FileQueue *queue, char *name);  //delete files or directory by name
INT32 IsFileOrDirectoryExist(FileQueue *queue, char *name); //find the directory or file by name

INT32 CloseFile(FileQueue *queue, INT32 inode);

int IsFileQueueEmpty(FileQueue *queue);


#endif
