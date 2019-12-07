#ifndef FAKE_FS
#define FAKE_FS
#include<stdio.h>
#define BYTE    unsigned char
#define WORD    unsigned short
#define DWORD   unsigned int
#define QWORD   unsigned long
#define BOOL    unsigned char

#define TRUE    1
#define FALSE   0
#define NULL    0

#define FILESYSTEM_MAXFILENAMELENGTH        24
#define FILESYSTEM_CLUSTERSIZE 8*512
#endif