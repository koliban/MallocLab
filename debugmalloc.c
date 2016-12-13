#include <stdlib.h>
#include <string.h>
#include "debugmalloc.h"
#include "dmhelper.h"
#include <stdio.h>

//�ڴ��ͷ��
typedef struct BlockHeader{
	int checkSum;		//����
	char *fileName;		//�ļ���
	int lineNumber;		//����ʱ��������
	size_t size;		//���������ڴ��С
}blockHeader;

//�����洢��һ����װ�����ڴ��
typedef struct BlockList {
	blockHeader *block;	//����Ŀ�
	int lineNumber;		//��������ڴ�ʱ���������ı��ݣ���ֹ����������Ƿ���Ŀ��е�lineNumberֵ��������ʾ��������
	size_t size;		//��������ڴ��С�ı��ݣ���ֹ����������Ƿ���Ŀ��е�sizeֵ�������޷��õ�β��fence
	struct BlockList *next;
}blockList;

static blockList *header = NULL;	//ͷ�ڵ㣬���������

#define FENCE 0xCCDEADCC
#define FENCE_SIZE (sizeof(FENCE))
#define HEAP_BLOCK_HEADER_SIZE (sizeof(blockHeader) + FENCE_SIZE)
#define BASIC_HEAP_BLOCK_SIZE (sizeof(blockHeader)+2*FENCE_SIZE)
#define COMPUTE_CHECKSUM(curBlock) ((curBlock->size) | (curBlock->lineNumber))

//��ʼ��ͷ���
void init() {
	if (header != NULL) {
		return;
	}
	header = (blockHeader*)malloc(sizeof(blockList));
	header->block = NULL;
	header->lineNumber = 0;
	header->size = 0;
	header->next = NULL;
}

//�õ�ͷ��fence�ĵ�ַ
int *getHeaderFenceAddress(blockHeader *curBlock) {
	return (int *)((char *)curBlock + sizeof(blockHeader));
}

//�õ�β��fence�ĵ�ַ
int *getFooterFenceAddress(blockHeader *curBlock,size_t size) {
	return (int *)((char *)curBlock + HEAP_BLOCK_HEADER_SIZE + size);
}

//�õ���Ч�ڴ�ĵ�ַ
void *getPayloadAddress(blockHeader *curBlock) {
	return (void *)((char *)curBlock + HEAP_BLOCK_HEADER_SIZE);
}

/* Wrappers for malloc and free */

void *MyMalloc(size_t size, char *filename, int linenumber) {
	init();
	blockHeader *newblockHeader = (blockHeader *)malloc(BASIC_HEAP_BLOCK_SIZE + size);
	newblockHeader->size = size;
	newblockHeader->fileName = filename;
	newblockHeader->lineNumber = linenumber;
	newblockHeader->checkSum = COMPUTE_CHECKSUM(newblockHeader);
	*getHeaderFenceAddress(newblockHeader) = FENCE;
	*getFooterFenceAddress(newblockHeader,size) = FENCE;

	blockList *newListItem = (blockList*)malloc(sizeof(blockList));
	newListItem->block = newblockHeader;
	newListItem->lineNumber = newblockHeader->lineNumber;
	newListItem->size = newblockHeader->size;
	newListItem->next = NULL;

	blockList *pt = header;
	while (pt->next != NULL){
		pt = pt->next;
	}
	pt->next = newListItem;

	return getPayloadAddress(newblockHeader);
}

void MyFree(void *ptr, char *filename, int linenumber) {
	init();
	blockList *pre = header;
	blockList *cur = header->next;
	while ((cur != NULL) && (getPayloadAddress(cur->block) != ptr)) {
		pre = cur;
		cur = cur->next;
	}
	if (cur != NULL) {
		pre->next = cur->next;
		int errorNum = checkblockHeader(cur);
		if (errorNum != 0)
		{
			if ((errorNum & 0x001) != 0)
			{
				errorfl(1, cur->block->fileName, cur->lineNumber, filename, linenumber);
			}
			if ((errorNum & 0x010) != 0)
			{
				errorfl(2, cur->block->fileName, cur->lineNumber, filename, linenumber);
			}
			if ((errorNum & 0x100) != 0)
			{
				errorfl(3, cur->block->fileName, cur->lineNumber, filename, linenumber);
			}
		}
		free(cur->block);
		free(cur);
	}
	else{
		error(4, filename, linenumber);
	}
}

//���ÿ�������ڴ�
long int checkblockHeader(blockList *cur) {
	if (cur->block == NULL) {
		return 0;
	}
	else {
		long int errorNum = 0;
		if (*getHeaderFenceAddress(cur->block) != FENCE) {
			errorNum |= 0x001;
		}
		if (*getFooterFenceAddress(cur->block,cur->size) != FENCE) {
			errorNum |= 0x010;
		}
		if (cur->block->checkSum != COMPUTE_CHECKSUM(cur->block)) {
			errorNum |= 0x100;
		}
		return errorNum;
	}
}

/* returns number of bytes allocated using MyMalloc/MyFree:
used as a debugging tool to test for memory leaks */
int AllocatedSize() {
	int size = 0;
	blockList *pt = header->next;
	while (pt != NULL)
	{
		size += pt->block->size;
		pt = pt->next;
	}
	return size;
}



/* Optional functions */

/* Prints a list of all allocated blocks with the
filename/line number when they were MALLOC'd */
void PrintAllocatedBlocks() {
	init();
	blockList *cur = header->next;
	if (cur == NULL)
	{
		return;
	}
	else {
		printf("Currently allocated blocks:\n");
		while (cur != NULL) {
			printf("%d bytes,created at %s,line %d\n", cur->block->size, cur->block->fileName, cur->block->lineNumber);
			cur = cur->next;
		}
	}
}

/* Goes through the currently allocated blocks and checks
to see if they are all valid.
Returns -1 if it receives an error, 0 if all blocks are
okay.
*/
int HeapCheck() {
	init();
	blockList *cur = header->next;
	int result = 0;
	while (cur != NULL)
	{
		long int errorNum = checkblockHeader(cur);
		if (errorNum != 0)
		{
			if ((errorNum & 0x001) != 0)
			{
				PRINTERROR(1, cur->block->fileName, cur->lineNumber);
			}
			if ((errorNum & 0x010) != 0)
			{
				PRINTERROR(2, cur->block->fileName, cur->lineNumber);
			}
			if ((errorNum & 0x100) != 0)
			{
				PRINTERROR(3, cur->block->fileName, cur->lineNumber);
			}
			result = -1;
		}
		cur = cur->next;
	}
	return result;
}
