#ifndef FAKE_DM
#define FAKE_DM
#include<stdlib.h>
void* kAllocateMemory( unsigned long qwSize ){
    return malloc(qwSize);
}

#endif