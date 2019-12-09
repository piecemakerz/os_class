#ifndef __ASSEMBLYUTILITY_H__
#define __ASSEMBLYUTILITY_H__

#include "Types.h"
#include "Task.h"
////////////////////////////////////////////////////////////////////////////////
//
//  ÇÔ¼ö
//
////////////////////////////////////////////////////////////////////////////////
extern BYTE kInPortByte( WORD wPort );
extern void kOutPortByte( WORD wPort, BYTE bData );
// ?? ??? ???? ??
extern WORD kInPortWord( WORD wPort );
extern void kOutPortWord( WORD wPort, WORD wData );

extern void kLoadGDTR( QWORD qwGDTRAddress );
void kLoadTR( WORD wTSSSegmentOffset );
void kLoadIDTR( QWORD qwIDTRAddress);
void kEnableInterrupt( void );
void kDisableInterrupt( void );
QWORD kReadRFLAGS( void );
QWORD kReadTSC(void);
void kSwitchContext(CONTEXT* pstCurrentContext, CONTEXT* pstNextContext);
void kHlt(void);
extern BOOL kTestAndSet(volatile BYTE* pbDestination, BYTE bCompare, BYTE bSource);
////////////////////////////////////////////////////////////////////////////////
//
//  FPU ??
//
////////////////////////////////////////////////////////////////////////////////
void kInitializeFPU( void );
void kSaveFPUContext( void* pvFPUContext );
void kLoadFPUContext( void* pvFPUContext );
void kSetTS( void );
void kClearTS( void );
#endif /*__ASSEMBLYUTILITY_H__*/
