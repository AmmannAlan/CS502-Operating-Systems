#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MemoryManagement.h"

LRU_Queue *InitialLRUQueue()
{
	LRU_Queue *pqueue = (LRU_Queue *)malloc(sizeof(LRU_Queue));

  if (pqueue != NULL) {
	pqueue->front = NULL;
	pqueue->rear = NULL;
	pqueue->size = 0;
  }
  return pqueue;
}

pfNode EnQueueNewFrame(LRU_Queue *queue, MemoryInfo *status)
{
	pfNode current, pnode;
	pnode = (pfNode)malloc(sizeof(phy_frame));
	pnode->MemoryData = *status;
	pnode->next = NULL;

	if (IsLRUQueueEmpty(queue) == 1) //if LRU Queue is empty
	{
		queue->front = pnode;
		queue->rear = pnode;
		queue->size = 1;
		return pnode;
	}

	current = queue->front;
	while (current->next != NULL)
	{
		current = current->next;
	}
	current->next = pnode;
	queue->rear = pnode;
	queue->size++;
	return pnode;

}

//Dequeue First Element, this node will be the victim
pfNode DeQueueLeastUsedFrame(LRU_Queue *queue)
{
	pfNode pnode;
	pnode = queue->front;

	if (IsLRUQueueEmpty(queue) != 1 && pnode != NULL)
	{
		queue->size--;
		queue->front = pnode->next;

		if (queue->size == 0)
		{
			queue->rear = NULL;
		}
	}

	return pnode;
}

//return 1: there is free frame 0:No free Frame
int IsFreeFrameExist(LRU_Queue *queue)
{
	if (queue->size < 64)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

//return 1 if empty, 0 if empty
int IsLRUQueueEmpty(LRU_Queue *queue)
{
	if (queue->front == NULL && queue->rear == NULL && queue->size == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int GetFreeFrame(LRU_Queue *queue)
{
	int size = queue->size;

	if (size < 64)
	{
		return size;
	 }
}

pfNode DeQueueByVirtualPageNumber(LRU_Queue *queue, INT32 VPN)
{
	pfNode pnode = queue->front;
	pfNode ptemp;
	int flag = 0;

	while (1)
	{
		if (pnode != NULL) {
			if (pnode->MemoryData.VirtualPageNumber == VPN) //handle the head element
			{
				flag++;
				queue->front = queue->front->next;
				pnode = queue->front;
				queue->size--;
				if (queue->size == 0)//if no element left, delete rear node as well
				{
					queue->rear = NULL;
				}
			}
			else break;
		}
		else
		{
			//printf("Delete %d nodes, the total number left  in the queue is %d\n", flag, pqueue->size);
			return pnode;
		}
	}
	while (pnode->next != NULL)
	{
		if (pnode->MemoryData.VirtualPageNumber == VPN)
		{
			flag++;
			ptemp = pnode->next;
			if (pnode->next->next != NULL)
			{
				pnode->next = pnode->next->next;
				free(ptemp);
				ptemp = NULL;
				queue->size--;
			}
			else //if the target is the last node
			{
				free(ptemp);
				ptemp = NULL;
				pnode->next = NULL;
				queue->rear = pnode; //point the last element to rear after delete
				queue->size--;
				break;
			}
		}
		else //if no one match, move to next node
		{
			pnode = pnode->next;
		}
	}
	//printf("Delete %d nodes, the total number left in the queue is %d\n", flag, pqueue->size);
	return pnode;
}

long GetDataByMemoryAddress(LRU_Queue *pqueue, INT32  VirtualPage, INT32 processID)
{
	pfNode current = pqueue->front;

	while (current != NULL)
	{
		if (current->MemoryData.VirtualPageNumber == VirtualPage) // && current->MemoryData.ProcessID == processID) {
		{
			return current->MemoryData.physical_memory_io;
		}
		current = current->next;
	}
	//return 1024; 
}

//Important: New Method to Judge: 1: Exists 0: Does Not Exist
int IsMemoryAddressExist(LRU_Queue *queue, INT32 VirtualPage)
{
	pfNode current = queue->front;

	while (current != NULL)
	{
		if (current->MemoryData.VirtualPageNumber == VirtualPage)// && current->MemoryData.ProcessID == processID)
		{
			//if (current->MemoryData.ProcessID == processID)
			//{
				return 1;
				break;
			//}
		}
		current = current->next;
	}

	//return 0;
}

int GetPhysicalFrame(LRU_Queue *queue, INT32 VirtualPage)
{
	pfNode current = queue->front;

	while (current != NULL)
	{
		if (current->MemoryData.VirtualPageNumber == VirtualPage)// && current->MemoryData.ProcessID == processID)
		{
			//if (current->MemoryData.ProcessID == processID)
			//{
			return current->MemoryData.PhysicalFrame;
			break;
			//}
		}
		current = current->next;
	}

	//return 0;
}