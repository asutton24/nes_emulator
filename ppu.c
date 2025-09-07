#include "ppu.h"

Memory* vmem;
byte frameBuffer[61440] = {0};
int ppuCycle = 0;
static byte oam[256];
static byte* ppuRegs;
static dbyte ppuAddr;
double startOfFrame;

static int linkPPUtoCPU(){
	bankList* head = cmem->head;
	while (head != NULL && (head->low != 0x2000 || head->high != 0x2007)) head = head->next;
	if (head == NULL) return -1;
	ppuRegs = head->head->bank;
	return 0;
}

int attemptPixelDraw(){
	
}
int queryRegisters(){
	
}
