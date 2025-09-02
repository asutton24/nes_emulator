#include "memory.h"

byte read(Memory* m, dbyte add){
	add = mirrorAddress(m->mirrors, add);
	m->lastRead = add; 
	bankList* current = m->head;
	while (current != NULL){
		if (add >= current->low && add <= current->high){
			bankNode* node = current->head;
			for (int i = 0; i < current->bank; i++){
				node = node->next;
			}
			return node->bank[add - current->low];
		} else {
			current = current->next;
		}
	}
	return 0;
}

byte write(Memory* m, dbyte add, byte val){
	add = mirrorAddress(m->mirrors, add);
	m->lastWrite = add; 
	bankList* current = m->head;
	while (current != NULL){
		if (add >= current->low && add <= current->high){
			bankNode* node = current->head;
			for (int i = 0; i < current->bank; i++){
				node = node->next;
			}
			if (node->type == 'w'){
				node->bank[add - current->low] = val;
			}
			return 1;
		} else {
			current = current->next;
		}
	}
	return 0;
}

dbyte read16(Memory* m, dbyte add){
	return read(m, add) + read(m, add + 1) * 256;
}

byte write16(Memory* m, dbyte add, dbyte val){
	return write(m, add, val % 256) + write(m, add + 1, val >> 8);
}

dbyte mirrorAddress(mirrorNode* head, dbyte add){
	mirrorNode* cur = head;
	while (cur != NULL){
		if ((add - cur->low) / (cur->high - cur->low + 1) < cur->repeat){
			return cur->newLow + (add - cur->low) % (cur->high - cur->low + 1);
		}
		cur = cur->next;
	}
	return add;
}

int blockSet(byte* data, dbyte len, bankList* b, byte index){
	bankNode* cur = b->head;
	if (index >= b->length) return -1;
	for (int i = 0; i < index; i++){
		cur = cur->next;
	}
	for (int i = 0; i < len; i++){
		cur->bank[i] = data[i];
	}
	return 0;
}

int addBank(bankList* b, char type){
	if (b->length == 0){
		b->head = (bankNode*)malloc(sizeof(bankNode));
		b->head->bank = (byte*)malloc(b->high - b->low + 1);
		for (int i = 0; i < b->high - b->low + 1; i++){
			b->head->bank[i] = 0;
		}
		b->head->next = NULL;
		b->head->type = type;
		b->length++;
		return 0;
	}
	int ret = 1;
	bankNode* cur = b->head;
	while (cur->next != NULL){
		cur = cur->next;
		ret++;
	}
	cur->next = (bankNode*)malloc(sizeof(bankNode));
	cur->next->bank = (byte*)malloc(b->high - b->low + 1);
	for (int i = 0; i < b->high - b->low + 1; i++){
		cur->next->bank[i] = 0;
	}
	cur->next->next = NULL;
	cur->next->type = type;
	b->length++;
	return ret;
}

int addBankList(Memory* m, dbyte low, dbyte high){
	if (high < low) return -1;
	if (m->head == NULL){
		m->head = (bankList*)malloc(sizeof(bankList));
		m->head->next = NULL;
		m->head->head = NULL;
		m->head->low = low;
		m->head->high = high;
		m->head->length = 0;
		m->head->bank = 0;
		return 0;
	}
	if (high < m->head->low){
		bankList* new;
		new = (bankList*)malloc(sizeof(bankList));
		new->next = m->head;
		new->head = NULL;
		new->low = low;
		new->high = high;
		new->length = 0;
		new->bank = 0;
		m->head = new;
		return 0;
	}
	if ((low >= m->head->low && low <= m->head->high) || (high >= m->head->low && high<= m->head->high)) return -1;
	bankList* last = m->head;
	bankList* cur = m->head->next;
	byte found = 0;
	while (cur != NULL && !found){
		if (low > last->high && high < cur->low){
			found = 1;
		} else {
			cur = cur->next;
			last = last->next;
		}
	}
	if (!found && low > last->high && cur == NULL){
		last->next = (bankList*)malloc(sizeof(bankList));
		last->next->next = NULL;
		last->next->head = NULL;
		last->next->low = low;
		last->next->high = high;
		last->next->length = 0;
		last->next->bank = 0;
		return 0;
	}
	if (found){
		bankList* new;
		new = (bankList*)malloc(sizeof(bankList));
		new->next = cur;
		new->head = NULL;
		new->low = low;
		new->high = high;
		new->length = 0;
		new->bank = 0;
		last->next = new;
		return 0;
	}
	return -1;
}

int addBankAt(Memory* m, dbyte low, dbyte high, char type){
	bankList* cur = m->head;
	while (cur != NULL){
		if (cur->low == low && cur->high == high) return addBank(cur, type);
		cur = cur->next;
	}
	return -1;
}

int freeMemory(Memory* m){
	bankList* cur = m->head;
	bankList* old;
	while (cur != NULL){
		bankNode* curBank = cur->head;
		bankNode* oldBank;
		while (curBank != NULL){
			if (curBank->bank != NULL){
				free(curBank->bank);
			}
			oldBank = curBank;
			curBank = curBank->next;
			free(oldBank);
		}
		old = cur;
		cur = cur->next;
		free(old);
	}
	mirrorNode* mCur = m->mirrors
	mirrorNode* mTemp;
	while (mCur != NULL){
		mTemp = mCur->next;
		free(mCur);
		mCur = mTemp;
	}
	free(m);
	return 0;
}

int bankSwitch(Memory* m, dbyte low, dbyte high){
	bankList* cur = m->head;
	while (cur != NULL){
		if (cur->low == low && cur->high == high){
			cur->bank++;
			if (cur->bank == cur->length){
				cur->bank = 0;
			}
			return 0;
		}
		cur = cur->next;
	}
	return -1;
}
