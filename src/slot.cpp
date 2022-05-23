#include "slot.h"
#include <stdlib.h>
#include <string.h>

struct slot_t* slot_init(uint32_t num)
{
	struct slot_t* slot_ptr = nullptr;
	char** name_ptr = nullptr;

	slot_ptr = (struct slot_t*)malloc(sizeof(struct slot_t));
	if (slot_ptr == nullptr)
		return nullptr;

	memset(slot_ptr, 0, sizeof(struct slot_t));
	slot_ptr->size = num;
	if (num > 0)
	{
		name_ptr = (char**)malloc(num * sizeof(char*));
		if (!name_ptr)
		{
			free(slot_ptr);
			return nullptr;
		}
		slot_ptr->name = name_ptr;
	}
	return slot_ptr;
}

int slot_set(struct slot_t* slot, uint32_t id, char* name)
{
	if (id < slot->size && slot->name != nullptr)
	{
		slot->name[id] = name;
		return 1;
	}
	return 0;
}

char* slot_get(struct slot_t* slot, uint32_t id)
{
	if (id < slot->size && slot->name != nullptr && slot->name[id] != nullptr)
	{
		return slot->name[id];
	}
	return nullptr;
}

void slot_free(struct slot_t* slot)
{
	if (slot != nullptr)
	{
		if (slot->size > 0 && slot->name != nullptr)
		{
			free(slot->name);
		}
		free(slot);
	}
}