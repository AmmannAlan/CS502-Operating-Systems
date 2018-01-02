#pragma once
#pragma once
#ifndef _QUEUE_H_
#define _QUEUE_H_

//#include "base.c" This line must be DELETED
#include "global.h"
#include "z502.h"
#include "syscalls.h"
#include "DiskManagement.h"
#include <stdlib.h>

//
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
#define                  TERMINATED                  1
#define                  DISK_WRITE                  1
#define                  DISK_READ                   2

typedef struct
{
	long processID;
	long priority;
	char name[20];
	long address;
	long wakeUpTime;
	void *context;

	//Disk Data
	long DiskID;
	long Sector;
	long disk_buffer_write;
	long disk_buffer_read;
	int DiskStatus;

	char disk_buffer_write_new[16];
	char disk_buffer_read_new[16];

	long* PageTable;  //mmio.Field3

	DISK_DATA Disk_data_written;
	DISK_DATA Disk_data_read;
	//Current Directory
	Header CurrentDirectory;

}PCB;

typedef struct node
{
	PCB PCBdata;
	long time;
	long pid;
	int priority;
	char name[20];
	INT32 wakeuptime;
	//MMIO for each process , use long type because the definition are using the long type
	void *context; // mmio.Field1, it should be a context pointer, not a definite data type
	long ContextID;   //mmio.Field1
	long ContextAddress; //mmio.Field2
	void *PageTable;    //mmio.Field3, used in the second half of the project

	//Disk Read and Write
	struct node *next;

}Node, *PCBNode;

typedef struct
{
	PCBNode front;
	PCBNode rear;
	int size;

}PCBQueue;

// on the following is the definition of the Disk Queue



PCBQueue *initialQueue();

PCBNode EnQueueByPriority(PCBQueue *queue, PCB *pcb);

INT32 IsEmpty(PCBQueue *queue);

PCBNode EnQueueByWakeUpTime(PCBQueue *queue, PCB *pcb);

PCBNode DeQueueFirstElement(PCBQueue *queue);

PCBNode DeQueueByName(PCBQueue *queue,PCB *pcb);

void TerminateSelf(PCBQueue *queue, PCB *pcb);

//On the following is the operation of the Queue


#endif // !_QUEUE_H_
