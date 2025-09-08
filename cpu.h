#ifndef CPU_H
#define CPU_H

#include "memory.h"
#include <ncurses.h>

extern Memory* cmem;

int runcmd(void);
int raiseInterrupt(void);
int raiseNMI(void);
int resetCPU(void);

#endif
