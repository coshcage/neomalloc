#include "neomalloc.h"

#define SIZ 128

char buff[SIZ * 2];

int main()
{
	P_HEAP_HEADER ph;
	void * p1;
	
	ph = nmCreateHeap(buff, SIZ, 7);
	
	if (NULL != ph)
	{
		ph = nmExtendHeap(ph, SIZ);
		
		if (NULL != ph)
		{
			p1 = nmAllocHeap(ph, 8);
			
			if (NULL != p1)
			{
				p1 = nmReallocHeap(ph, p1, 64);
				
				if (NULL != p1)
				{
					nmFreeHeap(ph, p1);
					return 0;
				}
				return 4;
			}
			return 3;
		}
		return 2;
	}
	return 1;
}

