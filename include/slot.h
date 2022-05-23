#pragma once
#ifndef _SLOT_H_
#define _SLOT_H_
#include <stdint.h>

struct slot_t
{
	char** name;	//slot name的指针列表
	uint32_t size;
};

struct slot_t* slot_init(uint32_t num);
int slot_set(struct slot_t* slot, uint32_t id, char* name);
char* slot_get(struct slot_t* slot, uint32_t id);
void slot_free(struct slot_t* slot);

#endif
