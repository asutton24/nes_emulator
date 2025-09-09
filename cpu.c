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
static byte flags = 0x20;
static byte sp = 0xFF;
static dbyte pc;
static byte pageCross;

Memory* cmem;
static byte cpuInterrupt = 0;

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
			return read8(cmem, pc - 1);
		case ZPG:
			pc++;
			return read8(cmem, read8(cmem, pc - 1));
		case ZPGX:
			pc++;
			return read8(cmem, (read8(cmem, pc - 1) + x) % 256);
		case ABSOLUTE:
			pc += 2;
			return read8(cmem, read16(cmem, pc - 2));		
		case ABSOLUTEX:
			pc += 2;
			oldAddress = read16(cmem, pc - 2);
			pageCross = (oldAddress & 0xFF00) != ((oldAddress + x) & 0xFF00);
			return read8(cmem, oldAddress + x);		
		case ABSOLUTEY:
			pc += 2;
			oldAddress = read16(cmem, pc - 2);
			pageCross = (oldAddress & 0xFF00) != ((oldAddress + y) & 0xFF00);
			return read8(cmem, oldAddress + y);		
		case INDIRECTX:
			pc++;
			return read8(cmem, read16(cmem, (read8(cmem, pc - 1) + x) % 256));
		case INDIRECTY:
			pc++;
			oldAddress = read16(cmem, read8(cmem, pc - 1));
			pageCross = (oldAddress & 0xFF00) != ((oldAddress + y) & 0xFF00);
			return read8(cmem, oldAddress + y);
		default:
			return 0;
	}
}

static byte acm(char mode, byte op){
	byte olda = a;
	byte temp;
	switch (mode){
		case '+':
			a += op + (flags & 1);
			flags = (flags & 0xFE) | ((olda + op + (flags & 1)) > 255);
			flags = (flags & 0xBF) | ((((olda & 128) && (op & 128) && !(a & 128)) || (!(olda & 128) && !(op & 128) && (a & 128))) * 64);
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
			temp = a - op;
			flags = (flags & 124) | (temp & 128) | (2 * (a == op)) | (a >= op);
			return 0;
		case 'l':
			a = op;
			break;
	}
	setFlagsFromReg('a');
	return 0;
}

static int c1Instructions(){
	char* opmap = "|&^+xlc-";
	byte cycleVals[8] = {6, 3, 2, 4, 5, 4, 4, 4};
	byte opcode = read8(cmem, pc);
	byte adMode = (opcode >> 2) & 7;
	byte operand = getOperand(adMode);
	if (opcode >> 5 == 4){
		if (adMode != IMMEDIATE){
			write8(cmem, cmem->lastRead, a);
			if (adMode == ABSOLUTEX || adMode == ABSOLUTEY || adMode == INDIRECTY) pageCross = 1;
		} 
	} else {
		acm(opmap[opcode >> 5], operand);
	}
	return cycleVals[adMode] + pageCross;
}

static byte doShift(byte val, char mode){
	byte old;
	switch (mode){
		case 0:
			flags = (flags & 0xFE) | (val > 127);
			val = val << 1;
			break;
		case 1:
			old = val;
			val = (val << 1) + (flags & 1);
			flags = (flags & 0xFE) | (old > 127);
			break;
		case 2:
			flags = (flags & 0xFE) | (val & 1);
			val = val >> 1;
			break;
		case 3:
			old = val;
			val = (val >> 1) + ((flags & 1) * 128);
			flags = (flags & 0xFE) | (old & 1);
			break;
	}
	flags = (flags & 0x7D) | ((val == 0) * 2) | (val & 128);
	return val; 
}

static int shiftInstructions(){
	byte opcode = read8(cmem, pc);
	byte adMode = (opcode >> 2) & 7;
	byte shiftMode = (opcode >> 5);
	byte val;
	if (adMode == INDIRECTX || adMode == INDIRECTY || adMode == ABSOLUTEY){
		pc++;
		return 2;
	}
	if (adMode == IMMEDIATE){
		pc++;
		a = doShift(a, shiftMode);
		return 2;
	}
	val = getOperand(adMode);
	write8(cmem, cmem->lastRead, doShift(val, shiftMode));
	switch(adMode){
		case ZPG:
			return 5;
		case ZPGX:
		case ABSOLUTE:
			return 6;
		case ABSOLUTEX:
			return 7;
		default:
			return 2;
	}
}

static int branchInstructions(){
	byte branchType = read8(cmem, pc) >> 5;
	byte branchCondition = branchType & 1;
	byte bitToTest;
	byte operand;
	dbyte oldpc;
	int branchLen;
	byte cycles = 2;
	switch (branchType >> 1){
		case 0:
			bitToTest = 128;
			break;
		case 1:
			bitToTest = 64;
			break;
		case 2:
			bitToTest = 1;
			break;
		case 3:
			bitToTest = 2;
			break;
	}
	if (((flags & bitToTest) > 0) == branchCondition){
		cycles++;
		pc++;
		operand = read8(cmem, pc);
		if (operand < 128) branchLen = operand;
		else branchLen = (operand & 127) - 128;
		pc += branchLen;
		pc++;
		cycles += (oldpc / 256 != pc / 256);
	} else {
		pc += 2;
	}
	return cycles;
}

static int flagSetInstructions(){
	byte op = read8(cmem, pc) >> 5;
	byte bitMask;
	if (op == 4){
		//For some reason TYA falls here
		a = y;
		pc++;
		setFlagsFromReg('a');
		return 2;
	}
	if (op == 5) op--;
	switch (op >> 1){
		case 0:
			bitMask = 1;
			break;
		case 1:
			bitMask = 4;
			break;
		case 2:
			bitMask = 64;
			break;
		case 3:
			bitMask = 8;
			break;
	}
	if (op % 2 == 0) flags = flags & (bitMask ^ 0xFF);
	else flags = flags | bitMask;
	pc++;
	return 2;
}

static int stackIndexRegsInstructions(){
	byte op = read8(cmem, pc) >> 5;
	pc++;
	switch (op){
		case 0:
			write8(cmem, 0x100 + sp, flags);
			sp--;
			return 3;
		case 1:
			sp++;
			flags = read8(cmem, 0x100 + sp);
			return 4;
		case 2:
			write8(cmem, 0x100 + sp, a);
			sp--;
			return 3;
		case 3:
			sp++;
			a = read8(cmem, 0x100 + sp);
			setFlagsFromReg('a');
			return 4;
		case 4:
			y--;
			setFlagsFromReg('y');
			return 2;
		case 5:
			y = a;
			setFlagsFromReg('y');
			return 2;
		case 6:
			y++;
			setFlagsFromReg('y');
			return 2;
		case 7:
			x++;
			setFlagsFromReg('x');
			return 2;
	}
	return 2;
}

static int bitInstructions(){
	byte adMode = (read8(cmem, pc) >> 2) & 7;
	byte operand;
	if (adMode != ZPG && adMode != ABSOLUTE) return 2;
	operand = getOperand(adMode);
	flags = (flags & 61) | (operand & 192) | (2 * ((a & operand) == 0));
	if (adMode == ZPG) return 3;
	return 4;
}

static int storeXYInstructions(char reg){
	byte adMode = (read8(cmem, pc) >> 2) & 7;
	byte oldX = x;
	byte oldY = y;
	byte operand;
	if (adMode != ZPG && adMode != ABSOLUTE && adMode != ZPGX) return 2;
	if (reg == 'x'){
		y = oldX;
		x = oldY;
	}
	operand = getOperand(adMode);
	write8(cmem, cmem->lastRead, y);
	x = oldX;
	y = oldY;
	if (adMode == ZPG) return 3;
	return 4;	
}

static int ldXYInstructions(char reg){
	byte adMode = (read8(cmem, pc) >> 2) & 7;
	byte oldX = x;
	byte oldY = y;
	byte operand;
	if (adMode != 0 && adMode % 2 == 0) return 2;
	if (adMode == 0) adMode = IMMEDIATE;
	if (reg == 'x'){
		y = oldX;
		x = oldY;
	}
	y = getOperand(adMode);
	if (reg == 'x'){
		x = y;
		y = oldY;
	}
	setFlagsFromReg(reg);
	switch (adMode){
		case IMMEDIATE:
			return 2;
		case ZPG:
			return 3;
		case ZPGX:
		case ABSOLUTE:
			pageCross = 0;
		case ABSOLUTEX:
			return 4 + pageCross;
	}
	return 2;
}

static int cmpXYInstructions(){
	byte op = read8(cmem, pc);
	byte adMode = (op >> 2) & 7;
	byte val;
	byte cmpVal;
	byte operand;
	if (adMode == 2 || adMode > 3) return 2;
	if (adMode == 0) adMode = IMMEDIATE;
	if (op >> 5 == 6) val = y;
	else val = x;
	operand = getOperand(adMode);
	cmpVal = val - operand;
	flags = (flags & 124) | (cmpVal & 128) | (2 * (val == operand)) | (val >= operand);
	switch (adMode){
		case IMMEDIATE:
			return 2;
		case ZPG:
			return 3;
		case ABSOLUTE:
			return 4;
	}
	return 2;
}

static int incDecInstructions(){
	byte adMode = read8(cmem, pc) >> 2;
	byte diff;
	byte val;
	switch (adMode >> 3){
		case 6:
			diff = -1;
			break;
		case 7:
			diff = 1;
			break;
		default:
			return 2;
	}
	adMode = adMode & 7;
	if (adMode % 2 == 0) return 2;
	val = getOperand(adMode) + diff;
	write8(cmem, cmem->lastRead, val);
	flags = (flags & 0x7D) | (val & 128) | (2 * (val == 0));
	switch (adMode){
		case ZPG: return 5;
		case ABSOLUTEX: return 7;
		default: return 6;
	}
}

int runcmd(){
	switch (cpuInterrupt){
		case 1:
			cpuInterrupt = 0;
			if (flags & 4) break;
			pc = read16(cmem, 0xFFFE);
			break;
		case 2:
			cpuInterrupt = 0;
			pc = read16(cmem, 0xFFFA);
			break;
		case 3:
			cpuInterrupt = 0;
			pc = read16(cmem, 0xFFFC);
			break;
	}
	byte cycles;
	byte op = read8(cmem, pc);
	byte opA = op >> 5;
	byte opB = (op >> 2) & 7;
	byte opC = op & 3;
	if (opC == 1) return c1Instructions();
	else if (opC == 2 && opA < 4) return shiftInstructions();
	else if (opC == 0 && opB == 4) return branchInstructions();
	else if (opC == 0 && opB == 6) return flagSetInstructions();
	else if (opC == 0 && opB == 2) return stackIndexRegsInstructions();
	else if (op == 0){
		flags = flags | 16;
		pc = read16(cmem, 0xFFFE);
		return 7;
	} else if (op == 0x20){
		write8(cmem, 0x100 + sp, (pc + 2) / 256);
		sp--;
		write8(cmem, 0x100 + sp, (pc + 2) % 256);
		sp--;
		pc = read16(cmem, pc + 1);
		return 5;
	} else if (op == 0x40){
		sp++;
		flags = read8(cmem, 0x100 + sp);
		sp++;
		pc = read8(cmem, 0x100 + sp);
		sp++;
		pc += read8(cmem, 0x100 + sp) * 256;
		pc++;
		return 6;
	} else if (op == 0x60){	
		sp++;
		pc = read8(cmem, 0x100 + sp);
		sp++;
		pc += read8(cmem, 0x100 + sp) * 256;
		pc++;
		return 6;
	} else if (op == 0x4C){
		pc = read16(cmem, pc + 1);
		return 3;
	} else if (op == 0x6C){
		pc = read16(cmem, read16(cmem, pc + 1));
		return 5;
	} else if (opC == 0 && opA == 1) return bitInstructions();
	else if (opC == 0 && opA == 4) return storeXYInstructions('y');
	else if (opC == 0 && opA == 5) return ldXYInstructions('y');
	else if (opC == 0 && opA > 5) return cmpXYInstructions();
	else if (op == 0x8A){
		a = x;
		pc++;
		setFlagsFromReg('a');
		return 2;
	} else if (op == 0xAA){
		x = a;
		pc++;
		setFlagsFromReg('x');
		return 2;
	} else if (op == 0xCA){
		x--;
		pc++;
		setFlagsFromReg('x');
		return 2;
	} else if (op == 0xEA){
		pc++;	
		return 2;
	} else if (op == 0x9A){
		sp = x;
		pc++;
		return 2;
	} else if (op == 0xBA){
		x = sp;
		pc++;
		setFlagsFromReg('x');
		return 2;
	} else if (opC == 2 && opA == 4) return storeXYInstructions('x');
	else if (opC == 2 && opA == 5) return ldXYInstructions('x');
	else if (opC == 2 && opA > 5) return incDecInstructions();
	else return 2;
}

int raiseInterrupt(){
	cpuInterrupt = 1;
	return 0;	
}

int raiseNMI(){
	cpuInterrupt = 2;
	return 0;
}

int resetCPU(){
	cpuInterrupt = 3;
	return 0;
}
