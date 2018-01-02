#pragma once
#ifndef _BASE_H_
#define _BASE_H_

//#include <string.h>
//#include <stdio.h>
//#include "Queue.h"
//declarations of the functions here

void Dispatcher();
void DoOneLock(INT32 Lock_Memory_Offset);
void DoOneUnLock(INT32 Lock_Memory_Offset);
long GetPIDByName(char *name);
void InitializeAndStartContext(MEMORY_MAPPED_IO mmio, PCB *pcb, void *PageTable);
long OSCreateProcess(PCB *process_control_block);
void Physical_Disk_Read(long DiskID, long Sector, long disk_buffer_read);
void Physical_Disk_Write(long DiskID, long Sector, long disk_buffer_write);
void StartTimer(long *SleepTime);
void WasteTime();
long GetTheTimeOfNow();

//Disk Functions
void WriteHeaderToDisk(INT32 DiskID, Header *header);
void UpdateBitMap(INT32 DiskID, int DiskUsege);
void SetBitMap(Block0 *block0);
void FormatDisk(Block0 *block0, Header *RootDirectory);

#endif 
