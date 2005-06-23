/** 
 @file memory.c
 @brief ENet memory management functions
*/
#define ENET_BUILDING_LIB 1
#include "enet/types.h"
#include "enet/memory.h"

void *
enet_malloc (size_t size)
{
   void * memory = malloc (size);

   if (memory == NULL)
     abort ();

   return memory;
}

void *
enet_realloc (void * memory, size_t size)
{
   memory = realloc (memory, size);

   if (size > 0 &&
       memory == NULL)
     abort ();

   return memory;
}

void *
enet_calloc (size_t elements, size_t size)
{
   void * memory = calloc (elements, size);

   if (memory == NULL)
     abort ();

   return memory;
}

void
enet_free (void * memory)
{
   free (memory);
}

