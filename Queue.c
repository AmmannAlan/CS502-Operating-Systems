
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Queue.h"

PCBQueue *initialQueue()
{
	PCBQueue *pqueue = (PCBQueue *)malloc(sizeof(PCBQueue));

	if (pqueue != NULL) {
		pqueue->front = NULL;
		pqueue->rear = NULL;
		pqueue->size = 0;
	}
	return pqueue;
}

PCBNode EnQueueByPriority(PCBQueue *queue, PCB *pcb)
{
	PCBNode current, previous;
	INT32 position, positioncount;
	PCBNode pnode = (PCBNode)malloc(sizeof(Node));
	pnode->PCBdata = *pcb;
	pnode->next = NULL;

	if (IsEmpty(queue))
	{
		queue->front = pnode;
		queue->rear = pnode;
		queue->size = 1;
		
		return pnode;
	}

	current = queue->front;
	position = 1;
	//get the insert position
	while (current != NULL&&position <= queue->size) {

		if (pnode->PCBdata.priority < current->PCBdata.priority) {
			break;
		}
		current = current->next;
		position++;
	}

	if (position == 1) { //if insert into the first position
		pnode->next = queue->front;
		queue->front = pnode;
		//pqueue->rear = current;
		queue->size++;
		return pnode;
	}
	else if (position == queue->size + 1) {//if insert into the last position
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
	else { //if insert between first and last node
		previous = queue->front;
		positioncount = 2;
		while (positioncount < position) {
			previous = previous->next;
			positioncount++;
		}
		pnode->next = previous->next;
		previous->next = pnode;
		queue->size++;
		return pnode;
	}


}

//return 1 if empty, 0 if not empty
INT32 IsEmpty(PCBQueue *queue)
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

PCBNode EnQueueByWakeUpTime(PCBQueue *queue, PCB *pcb)
{
	/*
	PCBNode pnode = (PCBNode)malloc(sizeof(Node));
	pnode->PCBdata = *pcb;
	pnode->next = NULL;

	PCBNode pprenode;
	*/

	PCBNode current, previous, pnode;
	INT32 position, positioncount;
	pnode = (PCBNode)malloc(sizeof(Node));
	pnode->PCBdata = *pcb;
	pnode->next = NULL;  //it should be NULL

	if (IsEmpty(queue))  //if the readyqueue is empty, init it
	{
		queue->front = pnode;
		queue->rear = pnode;
		//pqueue->size++;  
		queue->size = 1;
		return pnode;
	}

	current = queue->front;
	position = 1;
	//get the insert position
	while (current != NULL && position <= queue->size) {
		if (pnode->PCBdata.wakeUpTime < current->PCBdata.wakeUpTime) {
			break;
		}
		current = current->next;
		position++;
	}

	if (position == 1) { //if insert into the first position
		pnode->next = queue->front;
		queue->front = pnode;
		//pqueue->rear = current;
		queue->size++;
		return pnode;
	}
	else if (position == queue->size + 1) {//if insert into the last position
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
	else { //if insert between first and last node
		previous = queue->front;
		positioncount = 2;
		while (positioncount < position) {
			previous = previous->next;
			positioncount++;
		}
		pnode->next = previous->next;
		previous->next = pnode;
		queue->size++;
		return pnode;
	}

	printf(" \n\n*** EnQueue: Timer Q front wake up time: %d **** \n\n", queue->front->PCBdata.wakeUpTime);
	printf("\n\n *** EnQueue: EnQueue item wake up time: %d **** \n\n", pcb->wakeUpTime);
	
	

}

PCBNode DeQueueFirstElement(PCBQueue *queue)
{
	PCBNode pnode;
	pnode = queue->front;

	if (IsEmpty(queue) != 1 && pnode != NULL)
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

PCBNode DeQueueByName(PCBQueue *queue, PCB *pcb)
{
	PCBNode pnode = queue->front;
	PCBNode ptemp;
	int flag = 0;

	while (1)
	{
		if (pnode != NULL) {
			if (strcmp(pnode->PCBdata.name, pcb->name) == 0) //handle the head element
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
		if (strcmp(pnode->next->PCBdata.name, pcb->name) == 0)
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

void TerminateSelf(PCBQueue *queue, PCB *pcb)
{
	PCBNode pnode = (PCBNode)malloc(sizeof(Node));
	pnode->PCBdata.processID = pcb->processID;

	if (!IsEmpty(queue)) {

		PCBNode p1;
		PCBNode p2 = NULL;
		p1 = queue->front;

		
		while (p1->PCBdata.processID != pnode->PCBdata.processID && p1->next != NULL) {
			p2 = p1;
			p1 = p1->next;
		}

		if (p1->PCBdata.processID == pnode->PCBdata.processID) {

			if (p1 == queue->front) {
				queue->front = p1->next;
			}
			else
				p2->next = p1->next;
			queue->size--;
			free(p1);
		}


	}
}

