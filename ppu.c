#include "ppu.h"

Memory* vmem;
byte frameBuffer[61440] = {0};
int ppuCycle = 0;
static byte oam[256];
static byte candidates[8];
static byte* ppuRegs;
static dbyte ppuAddr;
static int xscroll;
static int yscroll;
static byte scrollFlip = 0;
static byte AddressFlip = 0;
double startOfFrame;

static int linkPPUtoCPU(){
	bankList* head = cmem->head;
	while (head != NULL && (head->low != 0x2000 || head->high != 0x2007)) head = head->next;
	if (head == NULL) return -1;
	ppuRegs = head->head->bank;
	return 0;
}

static int collectCandidates(int scanline){
	int cIndex = 0;
	int spriteHeight = 8 + 8 * (ppuRegs[0] >> 5);
	for (int i = 0; i < 64; i++;){
		if (oam[4 * i] + 1 >= scanline && oam[4 * i] + 1 < scanline + spriteHeight){
			if (cIndex == 8){
				ppuRegs[2] = ppuRegs | 0x40;
				return 1;
			}
			candidates[cIndex] = i;
			cIndex++;
		}
	}
	ppuRegs[2] = ppuRegs[2] & 0xC0;
	return 0;
}

static int getSpritePixelValue(int spriteIndex, int xpos, int ypos){
	int pTableIndex = oam[4 * spriteIndex + 1];
	int attributes = oam[4 * spriteIndex + 2];
	int tableBase = 0;
	byte highBit;
	byte lowBit;
	if (attributes & 0x40) xpos = 7 - xpos;
	if (attributes & 0x80){
		if (ppuRegs[1] & 0x20) ypos = 15 - ypos;
		else ypos = 7 - ypos;
	}

	if (ppuRegs[1] & 0x20){
		if (spriteIndex & 1) tableBase = 0x1000;
		spriteIndex /= 2;
		if (ypos < 8){
			highBit = (read8(pmem, tableBase + spriteIndex * 32 + 8 + ypos) >> (7 - xpos)) & 1;
			lowBit = (read8(pmem, tableBase + spriteIndex * 32 + ypos) >> (7 - xpos)) & 1;
		} else {
			highBit = (read8(pmem, tableBase + spriteIndex * 32 + 24 + ypos) >> (7 - xpos)) & 1;
			lowBit = (read8(pmem, tableBase + spriteIndex * 32 + 16 + ypos) >> (7 - xpos)) & 1;
		}
	} else {
		if (ppuRegs[0] & 0x8) tableBase = 0x1000;
		highBit = (read8(pmem, tableBase + spriteIndex * 16 + 8 + ypos) >> (7 - xpos)) & 1;
		lowBit = (read8(pmem, tableBase + spriteIndex * 16 + ypos) >> (7 - xpos)) & 1;
	}	
	return highBit * 2 + lowBit;
}

static int getBackgroundPixelValue(int xpos, int ypos){
	int currentNametable = 0x2000 + 0x400 * (ppuRegs[0] & 3);
	int patternTableBase = 0;
	byte highBit;
	byte lowBit;
	xpos += xscroll;
	ypos += yscroll;
	int tileIndex = currentNametable + 32 * (ypos / 8) + (xpos / 8);
	if (ppuRegs[0] & 0x10) patternTableBase = 0x1000;
	xpos %= 8;
	ypos %= 8;
	highBit = (read8(pmem, tableBase + tileIndex * 16 + 8 + ypos) >> (7 - xpos)) & 1;
	lowBit = (read8(pmem, patternTableBase + tileIndex * 16 + ypos) >> (7 - xpos)) & 1;
	return highBit * 2 + lowBit;
}

int attemptPixelDraw(int scanline, int horizontal){
	byte currentNametable = ppuRegs[0] & 3;
	byte bestHighSprite = 0xFF;
	byte bestLowSprite = 0xFF;
	for (int i = 0; i < 8; i++){
		if (oam[candidates[i] * 4 + 3] - horizontal < 8){
			
		}
	} 
}

int queryRegisters(){
	
}
