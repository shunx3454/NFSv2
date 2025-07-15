#include "utils.h"

void memww(void *src, int n)
{
	*(int *)src = n;
}

int memrw(const void *src)
{
	return *(int *)src;
}