#pragma once
#ifndef  _MEMORYMANAGEMENT_H_
#define _MEMORYMANAGEMENT_H_

#include <stdio.h>
#include "global.h"
#include "z502.h"
#include "syscalls.h"

//struct to store the physical frame

typedef struct 
{
	INT32 PhysicalFrame;
	INT32 DataWritten;
	INT32 VirtualPageNumber; //Important
	long PageTable;

	long physical_memory_io;
	int DiskID;
	int ProcessID;
}MemoryInfo;

typedef struct phy_frame
{
	//INT32 physical_page;
	//INT32 DataWritten;
	MemoryInfo MemoryData;
	struct phy_frame *next;

}phy_frame, *pfNode;

typedef struct
{
	pfNode front;
	pfNode rear;
	int size; //64

}LRU_Queue;

LRU_Queue *InitialLRUQueue();
pfNode EnQueueNewFrame(LRU_Queue *queue, MemoryInfo *status);
pfNode DeQueueLeastUsedFrame(LRU_Queue *queue);
pfNode DeQueueByVirtualPageNumber(LRU_Queue *queue, INT32 VPN);


int IsFreeFrameExist(LRU_Queue *queue);
int GetFreeFrame(LRU_Queue *queue);
int IsLRUQueueEmpty(LRU_Queue *queue);
long GetDataByMemoryAddress(LRU_Queue *pqueue, INT32  VirtualPage, INT32 processID);
int IsMemoryAddressExist(LRU_Queue *queue, INT32 VirtualPage);
int GetPhysicalFrame(LRU_Queue *queue, INT32 VirtualPage);
//int GetFirstElement



#endif // ! _DISKMANAGEMENT_H_
