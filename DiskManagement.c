#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DiskManagement.h"
//#include "z502.h"
//#include <math.h>

void Initial_Block0(Block0 *block0, long DiskID)
{
	block0->DiskID = DiskID;
	block0->BitmapSize = 4; //4*4 blocks   16, 1-16
	block0->RootDirSize = 4; //need change 4
	block0->SwapSize = 2;    // need change 4*2 blocks 8
	block0->DiskLength = 2048;
	block0->BitmapLocation = 1; //block1 - 16
	block0->RootDirLocation = 17; //block17-20
	block0->SwapLocation = 21; //block21-28
	block0->RESERVED = '\0';
}
//The order
//0: sector 0
//1: -16: BitMap
//17-20: RootDiretcory and its index block
//21-28 Swap Location

void initialHeader(Header *header, Block0 *block0)
{
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);

	long time = (long)mmio.Field1;
	int indexLocation = block0->RootDirLocation + 1; //17 is the rootDirectory, 18 is the index block

	char name[7] = "root";
	header->CreationTime = time;
	strcpy(header->name, name);
	header->inode = 0;     //root is 0
	header->FileDescription = 31; //binary: 0001111 first bit 0/1:root/file,directory, 00/01/10/11:0,1,23,index level  3-7:inode
	header->FileSize = 0;      //should be 0?
	header->indexLocation = indexLocation; //18
}

INT32 SetBinaryNumber(int numberOf1)
{
	//int DecimalNumber;

	if (numberOf1 > 8)
	{
		printf("ERROR: Calculation is wrong.\n");
		return 255; //this byte is all occupied
	}
	else
	{
		int sum = 0;
		for (int i = 0; i < numberOf1; i++)
		{
			sum = sum + pow(2, 7 - i);
		}

		return sum;
	}
}


FileQueue *InitialFileQueue()
{
	FileQueue *pqueue = (FileQueue *)malloc(sizeof(FileQueue));

	if (pqueue != NULL) {
		pqueue->front = NULL;
		pqueue->rear = NULL;
		pqueue->size = 0;
	}
	return pqueue;
}


fileNode CreateNewFile(FileQueue *queue, Header *file)
{
	fileNode current, pnode;
	pnode = (fileNode)malloc(sizeof(FileInfo));
	pnode->FileHeader = *file;
	pnode->next = NULL;

	if (IsFileQueueEmpty(queue) == 1) //if LRU Queue is empty
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


fileNode DeleteFileByName(FileQueue *queue,char *name)
{
	fileNode pnode = queue->front;
	fileNode ptemp;
	int flag = 0;

	while (1)
	{
		if (pnode != NULL) {
			if (strcmp(pnode->FileHeader.name, name) == 0) //handle the head element
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
		if (strcmp(pnode->next->FileHeader.name, name) == 0)
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


INT32 IsFileOrDirectoryExist(FileQueue *queue, char *name)
{
	fileNode temp = (fileNode)malloc(sizeof(FileInfo));
	temp = queue->front; 

	while (temp != NULL)
	{
		if (strcmp(temp->FileHeader.name, name) == 0)
		{
			return 1; //if 1 means the file or directory has been created
		}
		temp = temp->next;
	}

	return 0; //if 0 means th file or directory does not exist
}


INT32 CloseFile(FileQueue *queue, INT32 inode)
{
	fileNode temp = (fileNode)malloc(sizeof(FileInfo));
	temp = queue->front;

	while (temp != NULL)
	{
		if (temp->FileHeader.inode == inode)
		{
			return 1; //if 1 means the file or directory has been created
		}
		temp = temp->next;
	}

	return 0; //if 0 means th file or directory does not exist
}

int IsFileQueueEmpty(FileQueue *queue)
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