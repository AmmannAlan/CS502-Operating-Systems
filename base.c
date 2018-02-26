/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to SampleCode.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 4.20 Jan     2015: Thread safe code - prepare for multiprocessors
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>

#include             "Queue.h"
#include             "DiskManagement.h"
#include             "MemoryManagement.h"
#include             "base.h"

 #include <string.h>
 #include <stdio.h>
 #include "Queue.h"

 //  Allows the OS and the hardware to agree on where faults occur
extern void *TO_VECTOR[];

char *call_names[] = { "mem_read ", "mem_write", "read_mod ", "get_time ",
		"sleep    ", "get_pid  ", "create   ", "term_proc", "suspend  ",
		"resume   ", "ch_prior ", "send     ", "receive  ", "PhyDskRd ",
		"PhyDskWrt", "def_sh_ar", "Format   ", "CheckDisk", "Open_Dir ",
		"OpenFile ", "Crea_Dir ", "Crea_File", "ReadFile ", "WriteFile",
		"CloseFile", "DirContnt", "Del_Dir  ", "Del_File " };


#define         ILLEGAL_PRIORITY                -3
#define         LEGAL_PRIORITY                  10
#define         DUPLICATED                      -4
#define         DO_LOCK                          1
#define         DO_UNLOCK                        0
#define         SUSPEND_UNTIL_LOCKED             TRUE
#define         DO_NOT_SUSPEND                   FALSE
#define         MAX_LENGTH                       64
#define         MAX_COUNT                        20

#define         DISK_INTERRUPT_DISK2            (short)7
#define         DISK_INTERRUPT_DISK3            (short)8
#define         DISK_INTERRUPT_DISK4            (short)9
#define         DISK_INTERRUPT_DISK5            (short)10
#define         DISK_INTERRUPT_DISK6            (short)11
#define         DISK_INTERRUPT_DISK7            (short)12

//Definition of the Locks
#define         TIMERQ_INTERLOCK_OFFSET            2
#define         REAYQ_INTERLOCK_OFFSET             3
#define         DISKQ_INTERLOCK_OFFSET             4
#define         FAULT_HANDLER_OFFSET               5

int currentPCBnum = 1;

//PCBNode currentPCB;
//PCBNode startPCB;


PCB *CurrentPCB;

PCBQueue *ExistQueue;
PCBQueue *timerQueue;
PCBQueue *readyQueue;

//we need 8 DiskQ here.From 0 to 7

PCBQueue *diskQueue[8];

INT32 LockResult;

//following is the Memory Part
short *Z502_PAGE_TBL_ADDR;
//INT16 alterPageTable[1024];
//INT32 CurrentVictim;
LRU_Queue *LRUqueue;
LRU_Queue *MemAddressQueue;
short Data[10][1024][16];
short ShaowPageTable[10][1024];
//Declaration of the function

//file operation
int inode = 0;
FileQueue* File_Queue;
char BitMap[8][16]; //8 disks 16 blocks 
int DiskUseage[8];

//dispatch for all the whole operating system
void Dispatcher()
{
	MEMORY_MAPPED_IO mmio;


	while(IsEmpty(readyQueue) == 1)
	{
		//printf("***Dispatcher: ReadyQ is empty. \n");
		//printf("");
		CALL(WasteTime());
	}

	if (readyQueue->front != NULL)
	{
		//printf("\n *** Dispatcher 1: ReadyQ size£º%d", readyQueue->size);

		memcpy(CurrentPCB, &readyQueue->front->PCBdata, sizeof(PCB));
		//printf("\*** Dispatcher: Current PCB ID: %d ****", CurrentPCB->processID);
		DeQueueByName(readyQueue, CurrentPCB);
		//printf("\n *** Dispatcher 2: ReadyQ size£º%d", readyQueue->size);

		mmio.Mode = Z502StartContext;
		mmio.Field1 = (long)CurrentPCB->context;
		mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
		MEM_WRITE(Z502Context, &mmio);
		//DeQueueByName(readyQueue, CurrentPCB);
		//printf("\n *** Dispatcher 2: ReadyQ size£º%d", readyQueue->size);
	}
}

void WasteTime()
{
	printf("");
	//Waste Time
}

void StartTimer(long *SleepTime)
{
	long currentTime;
	MEMORY_MAPPED_IO mmio;
	//LOCK
	DoOneLock(TIMERQ_INTERLOCK_OFFSET);

	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = 0;
	MEM_WRITE(Z502Clock, &mmio);
	currentTime = (long)mmio.Field1;

	//printf("\n ** Timer: Current Time: %d, current PCB ID: %d \n", currentTime, CurrentPCB->processID);
	
	
	long wakeUpTime = currentTime + (long)SleepTime;
	CurrentPCB->wakeUpTime = wakeUpTime;

	EnQueueByWakeUpTime(timerQueue, CurrentPCB); //put currentPCB into timerQ

	//UNLOCK
	DoOneUnLock(TIMERQ_INTERLOCK_OFFSET);
	
	//printf("\n *** Timer: Timer Q front wake up time: %d ***\n", timerQueue->front->PCBdata.wakeUpTime);

	if (wakeUpTime <= timerQueue->front->PCBdata.wakeUpTime)
	{
		mmio.Mode = Z502Start;
		mmio.Field1 = (long)SleepTime;
		mmio.Field2 = mmio.Field3 = 0;
		MEM_WRITE(Z502Timer, &mmio);
		//printf("\n *** 1. Wake up timer: %d ", wakeUpTime);
		//printf("\n **** 1. wake<tq wake: Sleep Time£º %d \n", SleepTime);
	
	}
	else if(wakeUpTime > timerQueue->front->PCBdata.wakeUpTime)
	{
		long new_sleepTime = timerQueue->front->PCBdata.wakeUpTime - currentTime;
		mmio.Mode = Z502Start;
		mmio.Field1 = new_sleepTime;
		mmio.Field2 = mmio.Field3 = 0;
		MEM_WRITE(Z502Timer, &mmio);
		//printf("\n **** 1. wake>tq wake: Sleep Time£º %d", new_sleepTime);

	}

	Dispatcher();
	
}

void Physical_Disk_Write(long DiskID,long Sector, long disk_buffer_write)
{
	MEMORY_MAPPED_IO mmio;

	CurrentPCB->DiskID = DiskID;
	CurrentPCB->Sector = Sector;
	CurrentPCB->disk_buffer_write = disk_buffer_write;
	//strcpy(CurrentPCB->disk_buffer_read_new, disk_buffer_write);
	CurrentPCB->DiskStatus = DISK_WRITE;
	

	if (IsEmpty(diskQueue[DiskID]) == 1)
	{
		mmio.Mode = Z502DiskWrite;
		mmio.Field1 = DiskID;
		mmio.Field2 = Sector;
		mmio.Field3 = (long)disk_buffer_write;
		MEM_WRITE(Z502Disk, &mmio);
	}

	DoOneLock(DISKQ_INTERLOCK_OFFSET);
	EnQueueByPriority(diskQueue[DiskID], CurrentPCB);
	DoOneUnLock(DISKQ_INTERLOCK_OFFSET);

	Dispatcher();

}

void Physical_Disk_Read(long DiskID,long Sector, long disk_buffer_read)
{
	MEMORY_MAPPED_IO mmio;

	CurrentPCB->DiskID = DiskID;
	CurrentPCB->Sector = Sector;
	//strcpy(CurrentPCB->disk_buffer_read_new, disk_buffer_read);
	CurrentPCB->disk_buffer_read = disk_buffer_read;
	CurrentPCB->DiskStatus = DISK_READ;

	if (IsEmpty(diskQueue[DiskID]) == 1)
	{
		mmio.Mode = Z502DiskRead;
		mmio.Field1 = DiskID; // Pick same disk location
		mmio.Field2 = Sector;
		mmio.Field3 = (long)disk_buffer_read;
		MEM_WRITE(Z502Disk, &mmio);
	}

	DoOneLock(DISKQ_INTERLOCK_OFFSET);
	EnQueueByPriority(diskQueue[DiskID], CurrentPCB);
	DoOneUnLock(DISKQ_INTERLOCK_OFFSET);

	Dispatcher();

}

void DoOneLock(INT32 Lock_Memory_Offset)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + Lock_Memory_Offset, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
}

void DoOneUnLock(INT32 Lock_Memory_Offset)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + Lock_Memory_Offset, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
}
/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the OS.
 ************************************************************************/
void InterruptHandler(void) {
	INT32 DeviceID;
	INT32 Status;
	//PCBNode temp;
	//INT32 currentTime;
	//INT32 sleepTime;

	//INT32 interruptingDevice; // to check wheteher there are more interrupts

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

	static BOOL remove_this_in_your_code = TRUE; /** TEMP **/
	static INT32 how_many_interrupt_entries = 0; /** TEMP **/

	// Get cause of interrupt
	mmio.Mode = Z502GetInterruptInfo;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;
	Status = mmio.Field2;
 
	

	//printf("Interrupt Handeler: Found Device ID %d with status %d \n", DeviceID, Status);

	if (mmio.Field4 != ERR_SUCCESS) {
		printf(
				"The InterruptDevice call in the InterruptHandler has failed.\n");
		printf("The DeviceId and Status that were returned are not valid.\n");
		//
	}



	if (DeviceID == TIMER_INTERRUPT)
	{
		DoOneLock(TIMERQ_INTERLOCK_OFFSET);

		long currentTime;
		mmio.Mode = Z502ReturnValue;
		mmio.Field1 = 0;
		MEM_READ(Z502Clock, &mmio);
		currentTime = mmio.Field1;
		//printf("\n\n TRUE: TIMER_INTERRUPT, Current Time: %d \n\n",currentTime);
		//printf("Timer Q front wake up time %d ", timerQueue->front->PCBdata.wakeUpTime);
		//printf("Ready Q size %d \n", readyQueue->size);

		//DeQueueByName(timerQueue, CurrentPCB);
		
	    if (IsEmpty(timerQueue) == 0)
		{
			if (timerQueue->front->PCBdata.wakeUpTime <= currentTime)
			{

				PCBNode pnode = DeQueueFirstElement(timerQueue);	
				PCB *pcb = (PCB*)malloc(sizeof(PCB));
				CALL(memcpy(pcb, &pnode->PCBdata, sizeof(PCB)));
				//DeQueueByName(timerQueue, pcb);

				//printf("\n *********** Interrupt Handler: Ready Q size %d ********** \n", readyQueue->size);
				//printf("\n *********** Interrupt Handler: Timer Q size %d **********\n", timerQueue->size);

				CALL(EnQueueByPriority(readyQueue, pcb));

				//printf("\n *********** Interrupt Handler: Ready Q size %d ********** \n", readyQueue->size);
				//printf("\n *********** Interrupt Handler: Timer Q size %d **********\n", timerQueue->size);

				if (IsEmpty(timerQueue) != 1)
				{
					long SleepTime = (long)timerQueue->front->PCBdata.wakeUpTime - (long)currentTime;
					mmio.Mode = Z502Start;
					mmio.Field1 = SleepTime;
					mmio.Field2 = mmio.Field3 = 0;
					MEM_WRITE(Z502Timer, &mmio);
				}
				
			}
			else if (timerQueue->front->PCBdata.wakeUpTime > currentTime)
			{
				long SleepTime = (long)timerQueue->front->PCBdata.wakeUpTime - (long)currentTime;

				mmio.Mode = Z502Start;
				mmio.Field1 = SleepTime;
				mmio.Field2 = mmio.Field3 = 0;
				MEM_WRITE(Z502Timer, &mmio);

			}
		}
		
		DoOneUnLock(TIMERQ_INTERLOCK_OFFSET);
	}

	//Disk Interrupt. DeviceID: 5-12 DiskID: 0-7
	if (DeviceID > 4 && DeviceID < 13)
	{

		//printf("\n INTERRUPT_HANDLER: DiskInterrupt. Device ID: %d \n",DeviceID);
		//mmio.Mode = Z502Status;

		DoOneLock(DISKQ_INTERLOCK_OFFSET);

		INT32 DiskID = DeviceID - 5;

		if (IsEmpty(diskQueue[DiskID]) == 0)
		{
			PCBNode pnode = DeQueueFirstElement(diskQueue[DiskID]);
           PCB *pcb = (PCB*)malloc(sizeof(PCB));
           CALL(memcpy(pcb, &pnode->PCBdata, sizeof(PCB)));

           CALL(EnQueueByPriority(readyQueue, pcb));
		}

		if (diskQueue[DiskID]->front != NULL)
		{
			if (diskQueue[DiskID]->front->PCBdata.DiskStatus == DISK_READ)
			{
				mmio.Mode = Z502DiskRead;
				mmio.Field1 = diskQueue[DiskID]->front->PCBdata.DiskID; // Pick same disk location
				mmio.Field2 = diskQueue[DiskID]->front->PCBdata.Sector;
				mmio.Field3 = (long)diskQueue[DiskID]->front->PCBdata.disk_buffer_read;
				MEM_WRITE(Z502Disk, &mmio);
			}
			else if (diskQueue[DiskID]->front->PCBdata.DiskStatus == DISK_WRITE)
			{
				mmio.Mode = Z502DiskWrite;
				mmio.Field1 = diskQueue[DiskID]->front->PCBdata.DiskID;
				mmio.Field2 = diskQueue[DiskID]->front->PCBdata.Sector;
				mmio.Field3 = (long)diskQueue[DiskID]->front->PCBdata.disk_buffer_write;
				MEM_WRITE(Z502Disk, &mmio);

			}
		}

		DoOneUnLock(DISKQ_INTERLOCK_OFFSET);
	}

	/** REMOVE THE NEXT SIX LINES **/
	//how_many_interrupt_entries++; /** TEMP **/
	//if (remove_this_in_your_code && (how_many_interrupt_entries < 10)) {
	//	printf("Interrupt_handler: Found device ID %d with status %d\n",
	//			(int) mmio.Field1, (int) mmio.Field2);
	//}
}           // End of InterruptHandler

/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

void FaultHandler(void) {
	INT32 DeviceID;

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware
	//record the victim information
	//pfNode *victim = (pfNode*)malloc(sizeof(pfNode));
	MemoryInfo *currentVictim = (MemoryInfo*)malloc(sizeof(MemoryInfo));

	//Physical Memory Write and Read Data
	long physical_memory_write[16]; //buffer write
	long physical_memory_read[16]; //buffer read
	int ProcessID = 0;
	//int MemoryAddressProcessID;
	//long disk_id_memory_use;
	//long disk_id_memory_use = 1;

	// Get cause of interrupt
	mmio.Mode = Z502GetInterruptInfo;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	//mmio.Mode = Z502GetInterruptInfo;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;

	INT32 Status;     //Status here is the fault page number
	Status = mmio.Field2;
	printf("\n Fault_handler: Found vector type %d with value %d\n", DeviceID, Status);

	Z502_PAGE_TBL_ADDR = (short*)CurrentPCB->PageTable;


	int Physical_Frame_number = 0;

	//printf("\n The (DiskID)Process ID: %d \n", CurrentPCB->processID);

	ProcessID = CurrentPCB->processID;

	if (DeviceID == INVALID_MEMORY)
	{

		if (Status >= NUMBER_VIRTUAL_PAGES) //if the page number exceeds the maximum
		{
			//printf("\n Virtual Address exceeds the virtual pages. The simulation will end here. \n");
			mmio.Mode = Z502Action;
			MEM_WRITE(Z502Halt, &mmio);
		}
		else if (Status < 0)
		{
			mmio.Mode = Z502Action;
			MEM_WRITE(Z502Halt, &mmio);
		}

		//Z502_PAGE_TBL_ADDR = (short*)CurrentPCB->PageTable;

		//printf("\n Current Page Table: %ld, CurrentProcess: %d \n", CurrentPCB->PageTable,CurrentPCB->processID);

		if ((ShaowPageTable[ProcessID][Status] & PTBL_REFERENCED_BIT) >> 13 == 0) //if not referenced before 
		{

			if (IsFreeFrameExist(LRUqueue) == 1)  //free frames avaiable
			{

				//printf("\n *************** Situation :1 ************** \n");

				Physical_Frame_number = GetFreeFrame(LRUqueue);
				ProcessID = CurrentPCB->processID;
				MemoryInfo *NewMemoryInfo = (MemoryInfo*)malloc(sizeof(MemoryInfo));
				NewMemoryInfo->VirtualPageNumber = Status;
				NewMemoryInfo->PhysicalFrame = Physical_Frame_number;
				NewMemoryInfo->ProcessID = ProcessID;
				NewMemoryInfo->PageTable = (long)CurrentPCB->PageTable;

				if (ProcessID < 8)
				{
					NewMemoryInfo->DiskID = ProcessID;
				}
				else if (ProcessID >= 8)
				{
					NewMemoryInfo->DiskID = ProcessID - 8;
				}

				//Enqueue them

				//DoOneLock(FAULT_HANDLER_OFFSET);
				CALL(EnQueueNewFrame(LRUqueue, NewMemoryInfo));
				//DoOneUnLock(FAULT_HANDLER_OFFSET);

			
				//Access Physical Memory using Page Table

				Z502_PAGE_TBL_ADDR[Status] = (short) PTBL_VALID_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				
				//
				//Z502ReadPhysicalMemory(Physical_Frame_number, (char*)physical_memory_write);
				//Physical_Disk_Write(ProcessID, Status, physical_memory_write);

				ShaowPageTable[ProcessID][Status] = (short)(PTBL_REFERENCED_BIT | PTBL_VALID_BIT) | (Physical_Frame_number & PTBL_PHYS_PG_NO);


				//Z502ReadPhysicalMemory(Physical_Frame_number, Data[ProcessID][Status]);
			}
			else //if frames not available, pick one victim
			{
				//printf("\n *************** Situation :2 ************** \n");
				int VictimProcessID;
				int VictimVirtualPage;
				long VictimPageTable;
				int VictimDiskID;

				MemoryInfo *NewMemoryInfo = (MemoryInfo*)malloc(sizeof(MemoryInfo));	
				pfNode victim = DeQueueLeastUsedFrame(LRUqueue);
				CALL(memcpy(currentVictim, &victim->MemoryData, sizeof(MemoryInfo)));


					Physical_Frame_number = currentVictim->PhysicalFrame;
					VictimProcessID = currentVictim->ProcessID;
					VictimVirtualPage = currentVictim->VirtualPageNumber;
					VictimPageTable = currentVictim->PageTable;
					VictimDiskID = currentVictim->DiskID;

					ProcessID = CurrentPCB->processID;
					
					NewMemoryInfo->VirtualPageNumber = Status;
					NewMemoryInfo->PhysicalFrame = Physical_Frame_number;
					NewMemoryInfo->ProcessID = ProcessID;
					NewMemoryInfo->PageTable = (long)CurrentPCB->PageTable;
         			//CALL(EnQueueNewFrame(LRUqueue, NewMemoryInfo));

				//printf("\n  Victim Virtual Page Number: %d Status: %d \n", VictimVirtualPage, Status);

				//printf("\n **** Situation 2: CurrentPCB: %d \n",CurrentPCB->processID);

				if (ProcessID < 8)
				{
					NewMemoryInfo->DiskID = currentPCBnum;
				}
				else if (ProcessID >= 8)
				{
					NewMemoryInfo->DiskID = ProcessID - 8;
				}

				CALL(EnQueueNewFrame(LRUqueue, NewMemoryInfo));

				//problems here
				//DoOneLock(FAULT_HANDLER_OFFSET);
				Z502ReadPhysicalMemory(Physical_Frame_number, (char*)physical_memory_write);
				Physical_Disk_Write(VictimDiskID, VictimVirtualPage, (long)physical_memory_write); //change process id to disk id
				//DoOneUnLock(FAULT_HANDLER_OFFSET);
				//printf("\n **** Situation 2: CurrentPCB: %d \n", CurrentPCB->processID);

				//another try:
				Z502_PAGE_TBL_ADDR = (short*)VictimPageTable;

				Z502_PAGE_TBL_ADDR[VictimVirtualPage] = (short)PTBL_REFERENCED_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);

				Z502_PAGE_TBL_ADDR = (short*)CurrentPCB->PageTable;
				
				//set valid

				Z502_PAGE_TBL_ADDR[Status] = (short)PTBL_VALID_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				//Z502_PAGE_TBL_ADDR[Status] = (short)PTBL_REFERENCED_BIT| (Physical_Frame_number & PTBL_PHYS_PG_NO);


				//Z502ReadPhysicalMemory(Physical_Frame_number, (char*)physical_memory_read);
				//Physical_Disk_Write(ProcessID, Status, physical_memory_read);

				//try. To change the page table here

				//DoOneLock(FAULT_HANDLER_OFFSET);
				//Z502_PAGE_TBL_ADDR = (short*)VictimPageTable;

				//Z502_PAGE_TBL_ADDR[VictimVirtualPage] = (short)PTBL_REFERENCED_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				//DoOneUnLock(FAULT_HANDLER_OFFSET);

				ShaowPageTable[ProcessID][Status] = (short)(PTBL_REFERENCED_BIT | PTBL_VALID_BIT) | (Physical_Frame_number & PTBL_PHYS_PG_NO);

				ShaowPageTable[VictimProcessID][VictimVirtualPage] = (short)PTBL_REFERENCED_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				

			}


		}
		else //Memory address has been addressed before
		{
			if (IsFreeFrameExist(LRUqueue) == 1)  //free frames avaiable
			{
				//printf("\n *************** Situation :3 ************** \n");

				int NewDiskID;
				Physical_Frame_number = GetFreeFrame(LRUqueue);
				ProcessID = CurrentPCB->processID;
				MemoryInfo *NewMemoryInfo = (MemoryInfo*)malloc(sizeof(MemoryInfo));
				NewMemoryInfo->VirtualPageNumber = Status;
				NewMemoryInfo->PhysicalFrame = Physical_Frame_number;
				NewMemoryInfo->ProcessID = ProcessID;
				NewMemoryInfo->PageTable = (long)CurrentPCB->PageTable;

				if (ProcessID < 8)
				{
					NewMemoryInfo->DiskID = ProcessID;
				}
				else if (currentPCBnum >= 8)
				{
					NewMemoryInfo->DiskID = ProcessID - 8;
				}
				NewDiskID = NewMemoryInfo->DiskID;

				//Enqueue them

				//DoOneLock(FAULT_HANDLER_OFFSET);
				CALL(EnQueueNewFrame(LRUqueue, NewMemoryInfo));
				//DoOneUnLock(FAULT_HANDLER_OFFSET);



				//Z502WritePhysicalMemory(Physical_Frame_number, Data[ProcessID][Status]);
			
				//Set the address 
				//DoOneLock(FAULT_HANDLER_OFFSET);
				
				//printf("\n **** Situation 3: CurrentPCB: %d \n", CurrentPCB->processID);

				//DoOneLock(FAULT_HANDLER_OFFSET);
				Z502_PAGE_TBL_ADDR[Status] = (short)PTBL_VALID_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);

				//DoOneLock(FAULT_HANDLER_OFFSET);
				Physical_Disk_Read(NewDiskID, Status, (long)physical_memory_read); //change process id to disk id
				Z502WritePhysicalMemory(Physical_Frame_number, (char*)physical_memory_read);
				//DoOneUnLock(FAULT_HANDLER_OFFSET);

				//Z502_PAGE_TBL_ADDR[Status] = (short)PTBL_REFERENCED_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				ShaowPageTable[ProcessID][Status] = (short)(PTBL_REFERENCED_BIT | PTBL_VALID_BIT) | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				//DoOneUnLock(FAULT_HANDLER_OFFSET);

				//printf("\n **** Situation 3(ends): CurrentPCB: %d \n", CurrentPCB->processID);
			}
			else // pick one victim 
			{
				int VictimProcessID;
				int VictimVirtualPage;
				long VictimPageTable;
				int VictimDiskID;
				int DiskID;

				MemoryInfo *NewMemoryInfo = (MemoryInfo*)malloc(sizeof(MemoryInfo));
				
				//printf("\n *************** Situation :4 ************** \n");

					//DoOneLock(FAULT_HANDLER_OFFSET);
					pfNode victim = DeQueueLeastUsedFrame(LRUqueue);
					CALL(memcpy(currentVictim, &victim->MemoryData, sizeof(MemoryInfo)));
					//DoOneUnLock(FAULT_HANDLER_OFFSET);

					Physical_Frame_number = currentVictim->PhysicalFrame;
					VictimProcessID = currentVictim->ProcessID;
					VictimVirtualPage = currentVictim->VirtualPageNumber;
					VictimPageTable = currentVictim->PageTable;

					//new
					VictimDiskID = currentVictim->DiskID;

					ProcessID = CurrentPCB->processID;

					NewMemoryInfo->VirtualPageNumber = Status;
					NewMemoryInfo->PhysicalFrame = Physical_Frame_number;
					NewMemoryInfo->ProcessID = ProcessID;
					NewMemoryInfo->PageTable = (long)CurrentPCB->PageTable;

					if (ProcessID < 8)
					{
						NewMemoryInfo->DiskID = ProcessID;
					}
					else if (ProcessID >= 8)
					{
						NewMemoryInfo->DiskID = ProcessID - 8;
					}
					DiskID = NewMemoryInfo->DiskID;


					//Enqueue them
					//DoOneLock(FAULT_HANDLER_OFFSET);
					CALL(EnQueueNewFrame(LRUqueue, NewMemoryInfo));
					//DoOneUnLock(FAULT_HANDLER_OFFSET);
				
			
				//printf("\n **** Situation 4(begins): CurrentPCB: %d \n", CurrentPCB->processID);

				//DoOneLock(FAULT_HANDLER_OFFSET);

				Z502ReadPhysicalMemory(Physical_Frame_number, (char*)physical_memory_write);
				Physical_Disk_Write(VictimDiskID, VictimVirtualPage, (long)physical_memory_write); // change victim process id to victim disk id

				//DoOneUnLock(FAULT_HANDLER_OFFSET);

				//DoOneLock(FAULT_HANDLER_OFFSET);
				Physical_Disk_Read(DiskID, Status, (long)physical_memory_read);
				Z502WritePhysicalMemory(Physical_Frame_number, (char*)physical_memory_read);
				//DoOneUnLock(FAULT_HANDLER_OFFSET);

				//DoOneUnLock(FAULT_HANDLER_OFFSET);
				//printf("\n **** Situation 4(ends): CurrentPCB: %d \n", CurrentPCB->processID);

				//Another Try: Set Invalid First
				Z502_PAGE_TBL_ADDR = (short*)VictimPageTable;

				Z502_PAGE_TBL_ADDR[VictimVirtualPage] = (short)PTBL_REFERENCED_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);

				Z502_PAGE_TBL_ADDR = (short*)CurrentPCB->PageTable;

				Z502_PAGE_TBL_ADDR[Status] = (short)PTBL_VALID_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
			
				ShaowPageTable[ProcessID][Status] = (short) (PTBL_REFERENCED_BIT | PTBL_VALID_BIT) | (Physical_Frame_number & PTBL_PHYS_PG_NO);

				ShaowPageTable[VictimProcessID][VictimVirtualPage] = (short)PTBL_REFERENCED_BIT | (Physical_Frame_number & PTBL_PHYS_PG_NO);
				//printf("\n *************** Situation :4  ENDS************** \n");
			}
		}

	}
	else if (DeviceID == TO_VECTOR_TRAP_HANDLER_ADDR)
	{
		//printf("\n Fault Handler: Interrupt is 2 \n");
	}
	else if (DeviceID == CPU_ERROR)
	{
		printf("\n CPU ERROR.\n");
	}


	
} // End of FaultHandler


/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/

void svc(SYSTEM_CALL_DATA *SystemCallData) {

	short call_type;
	static short do_print = 10;
	short i;
	PCBNode temp = (PCBNode)malloc(sizeof(PCBNode));
	void *PageTable = (void *)calloc(2, NUMBER_VIRTUAL_PAGES);

	//For the creations of processes
	PCB *process_control_block = (PCB*)malloc(sizeof(PCB));
	long pid;

	DISK_DATA *Disk_Data_Written = (DISK_DATA *) calloc(1, sizeof(DISK_DATA));
	DISK_DATA *Disk_Data_Read = (DISK_DATA *)calloc(1, sizeof(DISK_DATA));

	MEMORY_MAPPED_IO mmio;

	//Disk Operations
	long DiskID;
	long Sector;
	long disk_buffer_write;
	long disk_buffer_read;
	//char disk_buffer_write_new[16];
	//char disk_buffer_read_new[16];
	
	//char char_data[16];
	Block0 *block0 = (Block0*)malloc(sizeof(Block0));
	Header *header = (Header*)malloc(sizeof(Header));

	char FileName[7]; //may be a root name or file name
	int ProcessID;
	int inodeSysCall;

	call_type = (short) SystemCallData->SystemCallNumber;
	if (do_print > 0) {
		printf("\n SVC handler: %s\n", call_names[call_type]);
		for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
			//Value = (long)*SystemCallData->Argument[i];
			printf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
					(unsigned long) SystemCallData->Argument[i],
					(unsigned long) SystemCallData->Argument[i]);
		}
		do_print--;
	}
        // Write code here

	switch (call_type)
	{
	    case SYSNUM_GET_TIME_OF_DAY:   //this value is found in syscalls.h

		mmio.Mode = Z502ReturnValue;
		mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
		MEM_READ(Z502Clock, &mmio);
		long currentTime = (long)mmio.Field1;
		*(long *)SystemCallData->Argument[0] = mmio.Field1;
		printf("\n Current Time: %d \n", currentTime);
		break;

		case SYSNUM_TERMINATE_PROCESS:

			if ((long)SystemCallData->Argument[0] == -1)
			{
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			
				printf("Current PCB pid %d", CurrentPCB->processID);
				CALL(DeQueueByName(timerQueue, CurrentPCB));
				CALL(DeQueueByName(readyQueue, CurrentPCB));
				CALL(DeQueueByName(ExistQueue, CurrentPCB));

				printf("\n Timer Q size: %d", timerQueue->size);
				printf("\n Ready Q size: %d ", readyQueue->size);
				//printf("\n Exist Q size: %d \n", ExistQueue->size);


				if (IsEmpty(readyQueue) == 1 && IsEmpty(timerQueue) == 1)
				{
					mmio.Mode = Z502Action;
					MEM_WRITE(Z502Halt, &mmio);
				}

				Dispatcher();

			}
			else if ((long)SystemCallData->Argument[0] == -2)
			{
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
				mmio.Mode = Z502Action;
				MEM_WRITE(Z502Halt, &mmio);
			}
			break;

		case SYSNUM_SLEEP:

			if (SystemCallData->Argument[0] < 0)
			{
				printf("ERROR! The Sleep Time is Illegal.\n");
				break;
			}

			//Start the timer and do not use the code on the following
			StartTimer(SystemCallData->Argument[0]); 

			break;

		case SYSNUM_CREATE_PROCESS:
			
			strcpy(process_control_block->name, (char*)SystemCallData->Argument[0]);
			process_control_block->address = (long)SystemCallData->Argument[1];
			process_control_block->priority = (long)SystemCallData->Argument[2];

			pid = OSCreateProcess(process_control_block);

			if (pid == ILLEGAL_PRIORITY)
			{
				printf("ILLEGAL_PRIORITY \n\n");
				*(long*)SystemCallData->Argument[3] = pid;
				*(long*)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}
			else if (pid == DUPLICATED)
			{
				printf("Duplicated Name, Already Existed \n\n");
				*(long *)SystemCallData->Argument[3] = pid;
				*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}
			else
			{
				printf("----------\nPCB %ld pid = %ld enqueued \n ------- \n", readyQueue->rear->PCBdata.processID, readyQueue->rear->PCBdata.processID);
				*(long *)SystemCallData->Argument[3] = pid;
				*(long *)SystemCallData->Argument[4] = ERR_SUCCESS;
			}

			if (currentPCBnum > 24)
			{
				*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}


			break;

		case SYSNUM_GET_PROCESS_ID:

			pid = GetPIDByName((char*)SystemCallData->Argument[0]);

			if (pid == 99)
			{
				*(long *)SystemCallData->Argument[1] = -1;
				*(long *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
			else
			{
				*(long *)SystemCallData->Argument[1] = pid;
				*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}
			
			//printf("\n Porcess Name: %s, Process ID: %d \n",SystemCallData->Argument[0],pid);
		
			break;

		case SYSNUM_PHYSICAL_DISK_WRITE:
		
			if ((long)SystemCallData->Argument[0] < 0 || (long)SystemCallData->Argument[0] > 7)
			{
				printf("\n SYSNUM_CALL ERROR: Wrong Disk ID. \n");
				//*(long*)SystemCallData->Argument[1] = ERR_BAD_PARAM;
				break;
			}
			else
			{
				DiskID = (long)SystemCallData->Argument[0];
				Sector = (long)SystemCallData->Argument[1];
				disk_buffer_write = (long)SystemCallData->Argument[2];
				//strcpy(disk_buffer_write_new, (char*)SystemCallData->Argument[2],sizeof(SystemCallData->Argument[2]));
				Physical_Disk_Write(DiskID, Sector, disk_buffer_write);

				//*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
				
			}
			break;

		case SYSNUM_PHYSICAL_DISK_READ:

			if ((long)SystemCallData->Argument[0] < 0 || (long)SystemCallData->Argument[0] > 7)
			{
				printf("\n SYSNUM_CALL ERROR: Wrong Disk ID. \n");
				//*(long*)SystemCallData->Argument[1] = ERR_BAD_PARAM;

				break;
			}
			else
			{
				DiskID = (long)SystemCallData->Argument[0];
				Sector = (long)SystemCallData->Argument[1];
				disk_buffer_read = (long)SystemCallData->Argument[2];
				//strcpy(disk_buffer_read_new, SystemCallData->Argument[2]);
                Physical_Disk_Read(DiskID, Sector, disk_buffer_read);
				//*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
				break;
			}

			break;

	    //commencing from test 9
		case SYSNUM_CHECK_DISK:
				
		     mmio.Mode = Z502CheckDisk;
		     mmio.Field1 = (long)SystemCallData->Argument[0];
			 mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
			 MEM_READ(Z502Disk, &mmio);
			*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			
			break;

		case SYSNUM_FORMAT:
		
			DiskID = (long)SystemCallData->Argument[0];
			CurrentPCB->DiskID = DiskID;

			if (DiskID < 0 || DiskID > 7)
			{
				printf("\n SYSNUM_FORMAT: ILLEGAL Disk ID. \n");
				*(long*)SystemCallData->Argument[1] = ERR_BAD_PARAM;
				
			}
			else
			{
				
				Initial_Block0(block0, DiskID);
			
				FormatDisk(block0,header);

				//use block 0 to initialize the root directory
				initialHeader(header, block0);

				CALL(CreateNewFile(File_Queue, header));

				inode = inode++;

			}
			break;

		case SYSNUM_OPEN_DIR:

			//in the format disk, the root directory has been created
			DiskID = (long)SystemCallData->Argument[0];
			ProcessID = CurrentPCB->processID;

			if (DiskID == -1)
			{
				strcpy(FileName, (char*)SystemCallData->Argument[1]);
				ProcessID = CurrentPCB->processID;
				DiskID = CurrentPCB->DiskID;

				if (IsFileOrDirectoryExist(File_Queue, FileName) == 1) //Exist
				{
					printf("\n Open Directory Successful. \n");
					*(long*)SystemCallData->Argument[2] = ERR_SUCCESS;
				}
				else //does not exist
				{
					printf("\n The directory does not exist. Create One. \n");

					Header *FileHeader = (Header*)malloc(sizeof(Header));
					FileHeader->CreationTime = GetTheTimeOfNow();
					FileHeader->FileDescription = 31;
					FileHeader->inode = inode;
					strcpy(FileHeader->name, FileName);
					header->FileSize = 0;      //should be 0?
					header->FileLocation = DiskUseage[ProcessID];
					header->indexLocation = DiskUseage[ProcessID] + 1;
					DiskUseage[DiskID] = DiskUseage[ProcessID] + 2;

					CALL(CreateNewFile(File_Queue, FileHeader));

					UpdateBitMap(DiskID, DiskUseage[ProcessID]);
					inode++; // increase 1

					*(long*)SystemCallData->Argument[2] = ERR_SUCCESS;
				}

			}
			else if(DiskID > -1 || DiskID < 8)
			{
				strcpy(FileName, (char*)SystemCallData->Argument[1]);
				CurrentPCB->DiskID = DiskID;

				if (IsFileOrDirectoryExist(File_Queue, FileName) == 1) //Exist
				{
					printf("\n Open Directory Successful. \n");
					*(long*)SystemCallData->Argument[2] = ERR_SUCCESS;
				}
				else //does not exist
				{
					printf("\n The directory does not exist. Create One. \n");

					Header *FileHeader = (Header*)malloc(sizeof(Header));
					FileHeader->CreationTime = GetTheTimeOfNow();
					FileHeader->FileDescription = 31;
					FileHeader->inode = inode;
					strcpy(FileHeader->name, FileName);
					header->FileSize = 0;      //should be 0?
					header->FileLocation = DiskUseage[DiskID];
					header->indexLocation = DiskUseage[DiskID] + 1;
					DiskUseage[DiskID] = DiskUseage[DiskID] + 2;
					CALL(CreateNewFile(File_Queue, FileHeader));


					WriteHeaderToDisk(DiskID, header);
					UpdateBitMap(DiskID, DiskUseage[ProcessID]);
					inode++; // increase 1

					*(long*)SystemCallData->Argument[2] = ERR_SUCCESS;
				}
			}
			else
			{
				printf("\n SYSNUM_OPEN_DIR ILLEGAL Disk ID. \n");
				*(long*)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}

		
			break;

		case SYSNUM_CREATE_DIR:
			     
			strcpy(FileName, (char*)SystemCallData->Argument[1]);
			DiskID = CurrentPCB->DiskID;
			ProcessID = CurrentPCB->processID;
			    
			if (IsFileOrDirectoryExist(File_Queue, FileName) == 1) //Exist
			{
				printf("\n The directory has been created. \n");
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else //does not exist
			{
				printf("\n The directory does not exist. Create One. \n");

				Header *FileHeader = (Header*)malloc(sizeof(Header));
				FileHeader->CreationTime = GetTheTimeOfNow();
				FileHeader->FileDescription = 1; //0: file, 1: directory 31:root directory
				FileHeader->inode = inode;
				strcpy(FileHeader->name, FileName);
				header->FileSize = 0;      //should be 0?
				header->FileLocation = DiskUseage[DiskID];
				header->indexLocation = DiskUseage[DiskID] + 1;
				DiskUseage[DiskID] = DiskUseage[DiskID] + 2;

				CALL(CreateNewFile(File_Queue, FileHeader));

				WriteHeaderToDisk(DiskID, header);


				UpdateBitMap(DiskID, DiskUseage[ProcessID]);
				inode++; // increase 1
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;

			}
		        
			break;

		case SYSNUM_CREATE_FILE:

			strcpy(FileName, (char*)SystemCallData->Argument[1]);

			if (IsFileOrDirectoryExist(File_Queue, FileName) == 1) //Exist
			{
				printf("\n Creating File: The File has been created. \n");
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else //does not exist
			{
				printf("\n (Create File): The File does not exist. Create One. \n");

				Header *FileHeader = (Header*)malloc(sizeof(Header));
				FileHeader->CreationTime = GetTheTimeOfNow();
				FileHeader->FileDescription = 0;  //0: file, 1: directory 31:root directory
				FileHeader->inode = inode;
				strcpy(FileHeader->name, FileName);
				header->FileSize = 0;      //should be 0?
				header->FileLocation = DiskUseage[DiskID];
				header->indexLocation = DiskUseage[DiskID] + 1;
				DiskUseage[DiskID] = DiskUseage[DiskID] + 2;

				CALL(CreateNewFile(File_Queue, FileHeader));

				WriteHeaderToDisk(DiskID, header);


				UpdateBitMap(DiskID, DiskUseage[ProcessID]);
				inode++; // increase 1
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			}

			break;

		case SYSNUM_OPEN_FILE:

			strcpy(FileName, (char*)SystemCallData->Argument[1]);

			if (IsFileOrDirectoryExist(File_Queue, FileName) == 1) //Exist
			{
				printf("\n Open File: The File has been created. \n");
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else //does not exist
			{
				printf("\n (Open File): The File does not exist. Create One. \n");

				Header *FileHeader = (Header*)malloc(sizeof(Header));
				FileHeader->CreationTime = GetTheTimeOfNow();
				FileHeader->FileDescription = 0;  //0: file, 1: directory 31:root directory
				FileHeader->inode = inode;
				strcpy(FileHeader->name, FileName);
				header->FileSize = 0;      //should be 0?
				header->FileLocation = DiskUseage[DiskID];
				header->indexLocation = DiskUseage[DiskID] + 1;
				//update the useage
				DiskUseage[DiskID] = DiskUseage[DiskID] + 2;

				CALL(CreateNewFile(File_Queue, FileHeader));

				WriteHeaderToDisk(DiskID, header);

				UpdateBitMap(DiskID, DiskUseage[ProcessID]);
				inode++; // increase 1
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			}

			break;

		case SYSNUM_CLOSE_FILE:
			    
			inodeSysCall = (long)SystemCallData->Argument[0];

			if (CloseFile(File_Queue, inodeSysCall) == 1)
			{
				printf("\n Close File: File Existed Close Successfully. \n");
				*(long*)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else
			{
				printf("\n Close File: File Does Not Exist. \n");
				*(long*)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}

			break;

		case SYSNUM_DIR_CONTENTS:

			break;


	    
	}

}                                               // End of svc

//return pid
long OSCreateProcess(PCB *process_control_block)
{
	int tempProcessID;
	//CALL(Dispatcher);
	MEMORY_MAPPED_IO mmio;
	PCBNode temp = (PCBNode)malloc(sizeof(Node));
	PCBNode pnode = (PCBNode)malloc(sizeof(Node));



	void *PageTable = (void *)calloc(2, NUMBER_VIRTUAL_PAGES);
	//void *PageTable = (void *)malloc(2,1024);
	


	if (process_control_block->priority == ILLEGAL_PRIORITY)
	{
		return ILLEGAL_PRIORITY;
	}

	temp = readyQueue->front;

	while (temp != NULL)
	{
		if (strcmp(process_control_block->name, temp->PCBdata.name) == 0)
		{
			printf("---\n This process is %s, Already Existed \n--- \n", temp->PCBdata.name);
			return DUPLICATED;
		}
		temp = temp->next;
	}

	currentPCBnum++;
	process_control_block->processID = (long)currentPCBnum;
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long)process_control_block->address;
	mmio.Field3 = (long)PageTable;
	MEM_WRITE(Z502Context, &mmio);
	process_control_block->context = (long*)mmio.Field1;
	process_control_block->PageTable = PageTable;

	//every process will have its own pageTable
	tempProcessID = currentPCBnum;
	mmio.Mode = Z502GetPageTable;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Context, &mmio);
	//problems here: 


	EnQueueByPriority(readyQueue, process_control_block);
	
	//Put the PCB into the Existence Queue
	EnQueueByPriority(ExistQueue, process_control_block);

	//currentPCBnum++;
	return process_control_block->processID;
}

long GetPIDByName(char *name)
{

	if (strcmp(name, "") == 0)
	{
		return (long)CurrentPCB->processID;
	}

	PCBNode temp = (PCBNode)malloc(sizeof(Node));
	temp = ExistQueue->front;

	while (temp != NULL)
	{
		if (strcmp(temp->PCBdata.name, name) == 0)
		{
			return (long)temp->PCBdata.processID;
		}
		temp = temp->next;
	}

	return 99;
}

long GetTheTimeOfNow()
{
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);

	long time = mmio.Field1;

	return time;
}

void InitializeAndStartContext(MEMORY_MAPPED_IO mmio,PCB *pcb,void *PageTable)
{
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long)pcb->address;
	mmio.Field3 = (long)PageTable;
	MEM_WRITE(Z502Context, &mmio);
	CurrentPCB->context = (long*)mmio.Field1;

	mmio.Mode = Z502StartContext;
	mmio.Field1 = (long)CurrentPCB->context;
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	MEM_WRITE(Z502Context, &mmio);

}

/******************Disk Management Functions Here*************/
void FormatDisk(Block0 *block0, Header*RootDirectory)
{

	DISK_DATA *Disk_Data_Written = (DISK_DATA*)malloc(sizeof(DISK_DATA));

	Disk_Data_Written->char_data[0] = block0->DiskID;
	Disk_Data_Written->char_data[1] = block0->BitmapSize;
	Disk_Data_Written->char_data[2] = block0->RootDirSize;
	Disk_Data_Written->char_data[3] = block0->SwapSize;
	Disk_Data_Written->char_data[4] = (block0->DiskLength & 255);    //LSB
	Disk_Data_Written->char_data[5] = ((block0->DiskLength >> 8) & 255); //MSB
	Disk_Data_Written->char_data[6] = (block0->BitmapLocation & 255);
	Disk_Data_Written->char_data[7] = ((block0->BitmapLocation >> 8) & 255);
	Disk_Data_Written->char_data[8] = (block0->RootDirLocation & 255);
	Disk_Data_Written->char_data[9] = ((block0->RootDirLocation >> 8) & 255);
	Disk_Data_Written->char_data[10] = (block0->SwapLocation & 255);
	Disk_Data_Written->char_data[11] = ((block0->SwapLocation >> 8) & 255);
	Disk_Data_Written->char_data[12] = Disk_Data_Written->char_data[13] = Disk_Data_Written->char_data[14] = Disk_Data_Written->char_data[15] = block0->RESERVED;

	//Write Sector 0
	Physical_Disk_Write(block0->DiskID, 0, (long)Disk_Data_Written->char_data);

	initialHeader(RootDirectory, block0);
	int indexLocation = block0->RootDirLocation; //17, put the root directory here. There are blocks on this line represent the index location of other
												 // files and sub-directories           
												 // other files and sub-directories have its pointer to the Place where to store the 

	Disk_Data_Written->char_data[0] = RootDirectory->inode;
	for (int i = 0; i < 7; i++)
	{
		Disk_Data_Written->char_data[i + 1] = RootDirectory->name[i];
	}
	Disk_Data_Written->char_data[8] = RootDirectory->FileDescription;
	Disk_Data_Written->char_data[9] = (RootDirectory->CreationTime & 255);
	Disk_Data_Written->char_data[10] = 25; //?? Why leave a blank here?
	Disk_Data_Written->char_data[11] = ((RootDirectory->CreationTime >> 8) & 255);
	Disk_Data_Written->char_data[12] = (RootDirectory->indexLocation & 255); //LSB
	Disk_Data_Written->char_data[13] = ((RootDirectory->indexLocation >> 8) & 255); //MSB
	Disk_Data_Written->char_data[14] = (RootDirectory->FileSize & 255);    //root: 0?
	Disk_Data_Written->char_data[15] = ((RootDirectory->FileSize >> 8) & 255); //root: 0?

																			   /*Write Root Directory*/
	Physical_Disk_Write(block0->DiskID, indexLocation, (long)Disk_Data_Written->char_data); //17, 17-21 to store the info of other files and directories

																							//SetBitMap
	SetBitMap(block0);

	//Set the Occupied Disks

	//Swap Area: Now filled them with 0
	Disk_Data_Written->int_data[0] = Disk_Data_Written->int_data[1] = Disk_Data_Written->int_data[2] = Disk_Data_Written->int_data[3] = 0;
	for (int m = 0; m < block0->SwapSize * 4; m++)
	{
		Physical_Disk_Write(block0->DiskID, block0->SwapLocation + m, (long)Disk_Data_Written->char_data);
	}


	//RootDirectory: Now Fill them with 0
	Disk_Data_Written->int_data[0] = Disk_Data_Written->int_data[1] = Disk_Data_Written->int_data[2] = Disk_Data_Written->int_data[3] = 0;

	for (int j = 1; j < block0->RootDirSize; j++) //!!!!!Attention! here youshould write from 18!
	{
		Physical_Disk_Write(block0->DiskID, block0->RootDirLocation + j, (long)Disk_Data_Written->char_data);
	}

	//Set Other Disks
	int OriginalSize = block0->SwapLocation + block0->SwapSize * 4;  //block0->BitmapSize * 4 + block0->RootDirSize + block0->SwapSize * 4 + 2;
																	 //sector0: from 0 -28, when read use for loop, start from 29
	int FollowingSectorSize = block0->DiskLength - OriginalSize;

	Disk_Data_Written->int_data[0] = 0;
	Disk_Data_Written->int_data[1] = 0;
	Disk_Data_Written->int_data[2] = 0;
	Disk_Data_Written->int_data[3] = 0;

	//when read, start from 29
	for (int i = 0; i < FollowingSectorSize; i++)
	{
		Physical_Disk_Write(block0->DiskID, OriginalSize + i, (long)Disk_Data_Written->char_data);
	}

}

void SetBitMap(Block0 *block0)
{
	long DiskID = (long)block0->DiskID;
	//DISK_DATA *Disk_Data_Written = (Block0*)malloc(sizeof(Block0));

	DISK_DATA *BitMapData = (DISK_DATA*)malloc(sizeof(DISK_DATA));

	int SectorBitMapOccupied = block0->BitmapSize * 4; //16 
	int OriginalSize = block0->BitmapSize * 4 + block0->RootDirSize + block0->SwapSize * 4 + 1; //29

	int Block0_in_BitMap_Size = OriginalSize / 128; //maybe 0,if original size is large enough, should be over than 1

	int Bit_1_Number_Remaining = OriginalSize % 128; //How many bits needed for the remaining
	int Byte_1_Number_Remaining = Bit_1_Number_Remaining / 8; //How many bytes needed
	int Bit_1_Number_remaining_in_one_byte = Bit_1_Number_Remaining % 8; //how many bits following;

																		 //int Byte_0_Number_Remaining = 16 - Byte_1_Number_Remaining - 1; //1 is that this byte is mixed with 0 and 1

	if (Block0_in_BitMap_Size == 0)
	{
		for (int i = 0; i < Byte_1_Number_Remaining; i++)
		{
			BitMapData->char_data[i] = 255;

		}

		BitMapData->char_data[Byte_1_Number_Remaining] = SetBinaryNumber(Bit_1_Number_remaining_in_one_byte);

		for (int j = Byte_1_Number_Remaining + 1; j < 16; j++)
		{
			BitMapData->char_data[j] = 0;
		}

		Physical_Disk_Write(DiskID, block0->BitmapLocation, (long)(char*)BitMapData->char_data);

		//Remaining BitMap not occupied - 0
		BitMapData->int_data[0] = BitMapData->int_data[1] = BitMapData->int_data[2] = BitMapData->int_data[3] = 0;

		for (int t = 0; t < block0->BitmapSize * 4 - 1; t++)
		{
			Physical_Disk_Write(DiskID, block0->BitmapLocation + 1 + t, (long)(char*)BitMapData->char_data);
		}

	}
	else if (Block0_in_BitMap_Size != 0)
	{
		BitMapData->int_data[0] = BitMapData->int_data[1] = BitMapData->int_data[2] = BitMapData->int_data[3] = 4294967295;

		for (int m = 0; m < Block0_in_BitMap_Size; m++)
		{
			Physical_Disk_Write(DiskID, block0->BitmapLocation + m, (long)(char*)BitMapData->char_data);
		}

		//Reamining, first, this one is mixed by 0 and 1
		for (int i = 0; i < Byte_1_Number_Remaining; i++)
		{
			BitMapData->char_data[i] = 255;

		}

		BitMapData->char_data[Byte_1_Number_Remaining] = SetBinaryNumber(Bit_1_Number_remaining_in_one_byte);

		for (int j = Byte_1_Number_Remaining + 1; j < 16; j++)
		{
			BitMapData->char_data[j] = 0;
		}

		Physical_Disk_Write(DiskID, block0->BitmapLocation + Block0_in_BitMap_Size, (long)(char*)BitMapData->char_data);

		//Remaining BitMap not occupied
		BitMapData->int_data[0] = BitMapData->int_data[1] = BitMapData->int_data[2] = BitMapData->int_data[3] = 0;

		for (int t = 0; t < block0->BitmapSize * 4 - Block0_in_BitMap_Size - 1; t++)
		{
			Physical_Disk_Write(DiskID, block0->BitmapLocation + 1 + t, (long)(char*)BitMapData->char_data);
		}

	}

}

void UpdateBitMap(INT32 DiskID, int DiskUseage)
{
	int ColumnNumber = DiskUseage / 128; //from 16
	int ModNumber = DiskUseage % 128;

	//In certain block
	int FullNumber = ModNumber / 8;
	int BlockModNumber = ModNumber % 8;
	int WrittenNumber = SetBinaryNumber(BlockModNumber);

	char data[16];
	char data_2[16];
	//the 1

	for (int m = 0; m < 16; m++)
	{
		data[m] = 255;
	}

	for (int i = 0; i < ColumnNumber; i++)
	{
		Physical_Disk_Write(DiskID, 1 + i, (long)data);
	}

	for (int n = 0; n < FullNumber; n++) //if fullnumber is 0 will not be excuted
	{
		data_2[n] = 255;
	}

	if (FullNumber < 16)
	{
		data_2[FullNumber] = WrittenNumber;
	}

	for (int t = FullNumber + 1; t < 16; t++)
	{
		data_2[t] = 0;
	}


	Physical_Disk_Write(DiskID, 1 + ColumnNumber, (long)data_2);
}

void WriteHeaderToDisk(INT32 DiskID, Header *header)
{
	char headerData[16];

	int Location = header->FileLocation;
	headerData[0] = header->inode;
	for (int i = 0; i < 7; i++)
	{
		headerData[i + 1] = header->name[i];
	}
	headerData[8] = header->FileDescription;
	headerData[9] = (header->CreationTime & 255);
	headerData[10] = 25; //?? Why leave a blank here?
	headerData[11] = ((header->CreationTime >> 8) & 255);
	headerData[12] = (header->indexLocation & 255); //LSB
	headerData[13] = ((header->indexLocation >> 8) & 255); //MSB
	headerData[14] = (header->FileSize & 255);    //root: 0?
	headerData[15] = ((header->FileSize >> 8) & 255); //root: 0?

	Physical_Disk_Write(DiskID, Location, (long)headerData);
}

/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void osInit(int argc, char *argv[]) {

	//void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	INT32 i;
	MEMORY_MAPPED_IO mmio;


	// Demonstrates how calling arguments are passed thru to here

	printf("Program called with %d arguments:", argc);
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");
	printf("Calling with argument 'sample' executes the sample program.\n");

	// Here we check if a second argument is present on the command line.
	// If so, run in multiprocessor mode.  Note - sometimes people change
	// around where the "M" should go.  Allow for both possibilities
	if (argc > 2) {
		if ((strcmp(argv[1], "M") ==0) || (strcmp(argv[1], "m")==0)) {
			strcpy(argv[1], argv[2]);
			strcpy(argv[2],"M\0");
		}
		if ((strcmp(argv[2], "M") ==0) || (strcmp(argv[2], "m")==0)) {
			printf("Simulation is running as a MultProcessor\n\n");
			mmio.Mode = Z502SetProcessorNumber;
			mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
			mmio.Field2 = (long) 0;
			mmio.Field3 = (long) 0;
			mmio.Field4 = (long) 0;
			MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
		}
	} else {
		printf("Simulation is running as a UniProcessor\n");
		printf(
				"Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
	}

	//Setup so handlers will come to code in base.c

	TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR ] = (void *) InterruptHandler;
	TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR ] = (void *) FaultHandler;
	TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR ] = (void *) svc;
	

	//  Determine if the switch was set, and if so go to demo routine.

	void* PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);

	if ((argc > 1) && (strcmp(argv[1], "sample") == 0)) {
		mmio.Mode = Z502InitializeContext;
		mmio.Field1 = 0;
		mmio.Field2 = (long) SampleCode;
		mmio.Field3 = (long) PageTable;

		MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence
		mmio.Mode = Z502StartContext;
		// Field1 contains the value of the context returned in the last call
		mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
		MEM_WRITE(Z502Context, &mmio);     // Start up the context

	} // End of handler for sample code - This routine should never return here
	//  By default test0 runs if no arguments are given on the command line
	//  Creation and Switching of contexts should be done in a separate routine.
	//  This should be done by a "OsMakeProcess" routine, so that
	//  test0 runs on a process recognized by the operating system.

	timerQueue = initialQueue();
	readyQueue = initialQueue();
	ExistQueue = initialQueue();

	//initial the LRU Queue
     LRUqueue = InitialLRUQueue();
	 MemAddressQueue = InitialLRUQueue();
	 File_Queue = InitialFileQueue();

	//initial the page table 
	for (int i = 0; i < 8; i++)
	{
		diskQueue[i] = initialQueue();
	}

	for (int i = 0; i < 8; i++)
	{
		DiskUseage[i] = 29;
	}
	
	//Data written (memory)
	for (int i = 0; i < 10; i++)
	{
		
			for (int j = 0; j < 1024; j++)
			{
				for (int m = 0; m < 16; m++)
				{
					Data[i][j][m] = 0;
				}
			}
		
	}

	//Initialize the shadow page table
	for (int i = 0; i < 10; i++)
	{
		for (int m = 0; m < 1024; m++)
		{
			ShaowPageTable[i][m] = 1;
		}
	}

	PCB *process_control_block = (PCB*)malloc(sizeof(PCB));
	
	mmio.Mode = Z502StartContext;

	if (argc > 1)
	{

		if (strcmp(argv[1], "test0") == 0)
		{
			strcpy(process_control_block->name, "test0");
			process_control_block->address = (long)test0;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test1") == 0)
		{
			strcpy(process_control_block->name, "test1");
			process_control_block->address = (long)test1;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test2") == 0)
		{
			strcpy(process_control_block->name,"test2" );
			process_control_block->address = (long)test2;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test3") == 0)
		{
			strcpy(process_control_block->name, "test3");
			process_control_block->address = (long)test3;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test4") == 0)
		{
			strcpy(process_control_block->name, "test4");
			process_control_block->address = (long)test4;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;
			
			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test5") == 0)
		{
			strcpy(process_control_block->name, "test5");
			process_control_block->address = (long)test5;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test6") == 0)
		{
			strcpy(process_control_block->name, "test6");
			process_control_block->address = (long)test6;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test7") == 0)
		{
			strcpy(process_control_block->name, "test7");
			process_control_block->address = (long)test7;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test8") == 0)
		{
			strcpy(process_control_block->name, "test8");
			process_control_block->address = (long)test8;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test9") == 0)
		{
			strcpy(process_control_block->name, "test9");
			process_control_block->address = (long)test9;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test10") == 0)
		{
			strcpy(process_control_block->name, "test10");
			process_control_block->address = (long)test10;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test11") == 0)
		{
			strcpy(process_control_block->name, "test11");
			process_control_block->address = (long)test11;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test12") == 0)
		{
			strcpy(process_control_block->name, "test12");
			process_control_block->address = (long)test12;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test13") == 0)
		{
			strcpy(process_control_block->name, "test13");
			process_control_block->address = (long)test13;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test14") == 0)
		{
			strcpy(process_control_block->name, "test14");
			process_control_block->address = (long)test14;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test15") == 0)
		{
			strcpy(process_control_block->name, "test15");
			process_control_block->address = (long)test15;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test16") == 0)
		{
			strcpy(process_control_block->name, "test16");
			process_control_block->address = (long)test16;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;
		}
		else if (strcmp(argv[1], "test21") == 0)
		{
			strcpy(process_control_block->name, "test21");
			process_control_block->address = (long)test21;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);


		    //Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test22") == 0)
		{
			strcpy(process_control_block->name, "test22");
			process_control_block->address = (long)test22;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);


			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test23") == 0)
		{
			strcpy(process_control_block->name, "test23");
			process_control_block->address = (long)test23;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);


			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test24") == 0)
		{
			strcpy(process_control_block->name, "test24");
			process_control_block->address = (long)test24;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;

			InitializeAndStartContext(mmio, process_control_block, PageTable);

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test25") == 0)
		{
			int processID;
			strcpy(process_control_block->name, "test25");
			process_control_block->address = (long)test25;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;

			InitializeAndStartContext(mmio, process_control_block, PageTable);
			//CurrentPCB->PageTable = PageTable;

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			//test here
			processID = CurrentPCB->processID;
			mmio.Mode = Z502GetPageTable;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
			MEM_READ(Z502Context, &mmio);
			//Z502_PAGE_TBL_ADDR[processID] = (short*)mmio.Field1;

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test26") == 0)
		{
			strcpy(process_control_block->name, "test26");
			process_control_block->address = (long)test26;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);


			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test27") == 0)
		{
			strcpy(process_control_block->name, "test27");
			process_control_block->address = (long)test27;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);
			CurrentPCB->PageTable = PageTable;

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}
		else if (strcmp(argv[1], "test28") == 0)
		{
			strcpy(process_control_block->name, "test28");
			process_control_block->address = (long)test28;
			process_control_block->priority = 1;
			process_control_block->processID = 1;
			process_control_block->PageTable = PageTable;

			CurrentPCB = process_control_block;
			InitializeAndStartContext(mmio, process_control_block, PageTable);
			CurrentPCB->PageTable = PageTable;

			//Put the PCB into the Existence Queue
			EnQueueByPriority(ExistQueue, process_control_block);

			currentPCBnum++;

		}

	}

}                                               // End of osInit
