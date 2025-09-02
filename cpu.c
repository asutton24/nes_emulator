#include "cpu.h"

#define INDIRECTX 0
#define ZPG 1
#define IMMEDIATE 2
#define ABSOLUTE 3
#define INDIRECTY 4
#define ZPGX 5
#define ABSOLUTEY 6
#define ABSOLUTEX 7

static byte a;
static byte x;
static byte y;
static byte flags;
static byte sp;
static dbyte pc;
static byte pageCross;

Memory* cmem;

static void setFlagsFromReg(char reg){
	byte val = a * (reg == 'a') + x * (reg == 'x') + y * (reg == 'y');
	flags = (flags & 0x7D) | (2 * (val == 0)) | (val & 128);
	return;
}

static byte getOperand(int mode){
	pc++;
	dbyte oldAddress;
	pageCross = 0;
	switch (mode){
		case IMMEDIATE:
			pc++;
			return read(cmem, pc - 1);
		case ZPG:
			pc++;
			return read(cmem, read(cmem, pc - 1));
		case ZPGX:
			pc++;
			return read(cmem, (read(cmem, pc - 1) + x) % 256);
		case ABSOLUTE:
			pc += 2;
			return read(cmem, read16(cmem, pc - 2));		
		case ABSOLUTEX:
			pc += 2;
			oldAddress = read16(cmem, pc - 2);
			pageCross = oldAddress & 0xFF00 != (oldAddress + x) & 0xFF00;
			return read(cmem, read16(cmem, oldAddress + x));		
		case ABSOLUTEY:
			pc += 2;
			oldAddress = read16(cmem, pc - 2);
			pageCross = oldAddress & 0xFF00 != (oldAddress + y) & 0xFF00;
			return read(cmem, read16(cmem, oldAddress + y));		
		case INDIRECTX:
			pc++;
			return read(cmem, read16(cmem, (read(cmem, pc - 1) + x)	% 256));
		case INDIRECTY:
			pc++;
			oldAddress = read16(cmem, read(cmem, pc - 1));
			pageCross = oldAddress & 0xFF00 != (oldAddress + y) & 0xFF00;
			return read(cmem, oldAddress + y);
		default:
			return 0;
	}
}

static byte acm(char mode, byte op){
	byte olda = a;
	switch (mode){
		case '+':
			a += op + (flags & 1);
			flags = (flags & 0xFE) | (a < olda);
			flags = (flags & 0xBF) | (((olda & 128) && (op & 128) && !(a & 128)) || (!(olda & 128) && !(op & 128) && (a & 128)));
			break;
		case '-':
			return acm('+', op ^ 0xFF);
		case '&':
			a = a & op;
			break;
		case '|':
			a = a | op;
			break;
		case '^':
			a = a ^ op;
			break;
		case 'c':
			acm('-', op);
			a = olda;
			return 0;
		case 'l':
			a = op;
			break;
	}
	setFlagsFromReg('a');
	return 0;
}

static byte c1Instructions(){
	char* opmap = "|&^+xlcs";
	byte cycleVals[8] = {6, 3, 2, 4, 5, 4, 4, 4};
	byte opcode = read(cmem, pc);
	byte adMode = (opcode >> 2) & 7;
	byte operand = getOperand(adMode);
	if (opcode >> 5 == 4){
		if (adMode != IMMEDIATE){ 
			write(cmem, cmem->lastRead, a);
			if (adMode == ABSOLUTEX || adMode == ABSOLUTEY || adMode == INDIRECTY) pageCross = 1;
		} 
	} else {
		acm(opmap[opcode >> 5], operand);
	}
	return cycleVals[(opcode >> 2) & 7] + pageCross;
}

int runcmd(){
	byte cycles;
	byte op = read(cmem, pc);
	if (op & 3 == 1) return c1Instructions();
}
