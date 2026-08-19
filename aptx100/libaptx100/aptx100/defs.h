#pragma once

#define _BYTE  unsigned char
#define _WORD  unsigned short
#define _DWORD unsigned int
#define _QWORD unsigned long long

#define LOBYTE(x)   (*((_BYTE*)&(x)))
#define LOWORD(x)   (*((_WORD*)&(x)))
#define LODWORD(x)  (*((_DWORD*)&(x)))
#define HIBYTE(x)   (*((_BYTE*)&(x)+1))
#define HIWORD(x)   (*((_WORD*)&(x)+1))
#define HIDWORD(x)  (*((_DWORD*)&(x)+1))

#define SLOBYTE(x)  (*((char*)&(x)))
#define SLOWORD(x)  (*((short*)&(x)))
#define SLODWORD(x) (*((int*)&(x)))
#define SHIBYTE(x)  (*((char*)&(x)+1))
#define SHIWORD(x)  (*((short*)&(x)+1))
#define SHIDWORD(x) (*((int*)&(x)+1))

#define BYTEn(x, n) (*((_BYTE*)&(x)+n))
#define BYTE1(x) BYTEn(x, 1)

#define __PAIR64__(hi, lo) (((long long)hi << 32) | (unsigned long long)lo)
