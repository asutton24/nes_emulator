#ifndef MEM_H
#define MEM_H

#include <stdlib.h>
#include <stdio.h>

typedef unsigned char byte;
typedef unsigned short int dbyte;

typedef struct bNode{
	byte* bank;
	struct bNode* next;
	char type;
} bankNode;

typedef struct bList{
	bankNode* head;
	struct bList* next;
	dbyte low;
	dbyte high;
	byte length;
	byte bank;
} bankList;

typedef struct mNode{
	dbyte low;
	dbyte high;
	dbyte newLow;
	dbyte repeat;
	struct mNode* next;
} mirrorNode;

typedef struct memoryList{
	bankList* head;
	mirrorNode* mirrors;
	dbyte lastRead;
	dbyte lastWrite;
} Memory;

byte read8(Memory* m, dbyte add);
byte write8(Memory* m, dbyte add, byte val);
dbyte read16(Memory* m, dbyte add);
byte write16(Memory* m, dbyte add, dbyte val);
dbyte mirrorAddress(mirrorNode* head, dbyte add);
int blockSet(byte* data, dbyte len, bankList* b, byte index);
int addBank(bankList* b, char type);
int addBankList(Memory* m, dbyte low, dbyte high);
int addBankAt(Memory* m, dbyte low, dbyte high, char type);
int bankSwitch(Memory* m, dbyte low, dbyte high);
int addMirror(Memory* m, dbyte low, dbyte high, dbyte newLow, dbyte repeat);
int freeMemory(Memory* m);

#endif
