#ifndef __TASK_H__
#define __TASK_H__

#include "Types.h"
#include "List.h"
#include "Console.h"

extern int trace_task_sequence;
////////////////////////////////////////////////////////////////////////////////
//
// 占쏙옙크占쏙옙
//
////////////////////////////////////////////////////////////////////////////////
// SS, RSP, RFLAGS, CS, RIP + ISR占쏙옙占쏙옙 占쏙옙占쏙옙占싹댐옙 19占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙
#define TASK_REGISTERCOUNT     ( 5 + 19 )
#define TASK_REGISTERSIZE       8

// Context 占쌘료구占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙
#define TASK_GSOFFSET           0
#define TASK_FSOFFSET           1
#define TASK_ESOFFSET           2
#define TASK_DSOFFSET           3
#define TASK_R15OFFSET          4
#define TASK_R14OFFSET          5
#define TASK_R13OFFSET          6
#define TASK_R12OFFSET          7
#define TASK_R11OFFSET          8
#define TASK_R10OFFSET          9
#define TASK_R9OFFSET           10
#define TASK_R8OFFSET           11
#define TASK_RSIOFFSET          12
#define TASK_RDIOFFSET          13
#define TASK_RDXOFFSET          14
#define TASK_RCXOFFSET          15
#define TASK_RBXOFFSET          16
#define TASK_RAXOFFSET          17
#define TASK_RBPOFFSET          18
#define TASK_RIPOFFSET          19
#define TASK_CSOFFSET           20
#define TASK_RFLAGSOFFSET       21
#define TASK_RSPOFFSET          22
#define TASK_SSOFFSET           23

// 占승쏙옙크 풀占쏙옙 占쏙옙藥뱄옙占�
#define TASK_TCBPOOLADDRESS     0x800000
#define TASK_MAXCOUNT           1024

// 占쏙옙占쏙옙 풀占쏙옙 占쏙옙占쏙옙占쏙옙 크占쏙옙
#define TASK_STACKPOOLADDRESS   ( TASK_TCBPOOLADDRESS + sizeof( TCB ) * TASK_MAXCOUNT )
#define TASK_STACKSIZE          8192

// 占쏙옙효占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크 ID
#define TASK_INVALIDID          0xFFFFFFFFFFFFFFFF

// 占승쏙옙크占쏙옙 占쌍댐옙占� 占쏙옙 占쏙옙 占쌍댐옙 占쏙옙占싸쇽옙占쏙옙 占시곤옙(5 ms)
#define TASK_PROCESSORTIME      5

// 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙
#define TASK_MAXREADYLISTCOUNT  5

// 占승쏙옙크占쏙옙 占쎌선 占쏙옙占쏙옙

/*
#define TASK_FLAGS_HIGHEST            0
#define TASK_FLAGS_HIGH               1
#define TASK_FLAGS_MEDIUM             2
#define TASK_FLAGS_LOW                3
#define TASK_FLAGS_LOWEST             4
*/

#define TASK_FLAGS_HIGHEST            50
#define TASK_FLAGS_HIGH               40
#define TASK_FLAGS_MEDIUM             30
#define TASK_FLAGS_LOW                20
#define TASK_FLAGS_LOWEST             10

#define TASK_FLAGS_WAIT               0xFF

// 600占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占싼다몌옙
// 占쏙옙占쏙옙 12, 15, 20, 30, 60占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占싱댐옙.
#define TASK_STRIDE_NUM				  600
// 占쏙옙 占승쏙옙크占쏙옙 pass占쏙옙 60占쏙옙 占실억옙占쏙옙 占쏙옙 占쏙옙占� 占승쏙옙크占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占실뤄옙
// pass占쏙옙 0占쏙옙占쏙옙 占십깍옙화占싼댐옙.
// pass占쏙옙 占썼열占쏙옙 占쏙옙占쏙옙占� 占쏙옙占싱댐옙.
#define TASK_PASS_MAX				  60

// 占승쏙옙크占쏙옙 占시뤄옙占쏙옙
#define TASK_FLAGS_ENDTASK            0x8000000000000000
#define TASK_FLAGS_IDLE               0x0800000000000000

// 占쌉쇽옙 占쏙옙크占쏙옙
#define GETPRIORITY( x )        ( ( x ) & 0xFF )
#define SETPRIORITY( x, priority )  ( ( x ) = ( ( x ) & 0xFFFFFFFFFFFFFF00 ) | \
        ( priority ) )
#define GETTCBOFFSET( x )       ( ( x ) & 0xFFFFFFFF )

////////////////////////////////////////////////////////////////////////////////
//
// 占쏙옙占쏙옙체
//
////////////////////////////////////////////////////////////////////////////////
// 1占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙
#pragma pack( push, 1 )

// 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占시듸옙 占쌘료구占쏙옙
typedef struct kContextStruct
{
    QWORD vqRegister[ TASK_REGISTERCOUNT ];
} CONTEXT;

typedef struct kTaskControlBlockStruct
{
    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙치占쏙옙 ID
    LISTLINK stLink;

    // 占시뤄옙占쏙옙
    QWORD qwFlags;

    // 占쏙옙占쌔쏙옙트
    CONTEXT stContext;

    // 占쏙옙占쏙옙占쏙옙 占쏙옙藥뱄옙占쏙옙占� 크占쏙옙
    void* pvStackAddress;
    QWORD qwStackSize;
} TCB;

/*
// 보폭 스케줄링을 위한 TCB
typedef struct kTaskControlBlockStruct
{
    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙치占쏙옙 ID
    LISTLINK stLink;

    // 占시뤄옙占쏙옙
    QWORD qwFlags;

    // 占쏙옙占쌔쏙옙트
    CONTEXT stContext;

    // Pass
    int iPass;

    // 占쏙옙占쏙옙占쏙옙 占쏙옙藥뱄옙占쏙옙占� 크占쏙옙
    void* pvStackAddress;
    QWORD qwStackSize;
} TCB;
*/

// TCB 풀占쏙옙 占쏙옙占승몌옙 占쏙옙占쏙옙占싹댐옙 占쌘료구占쏙옙
typedef struct kTCBPoolManagerStruct
{
    // 占승쏙옙크 풀占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    TCB* pstStartAddress;
    int iMaxCount;
    int iUseCount;

    // TCB占쏙옙 占쌀댐옙占� 횟占쏙옙
    int iAllocatedCount;
} TCBPOOLMANAGER;


// 占쏙옙占쏙옙占쌕뤄옙占쏙옙 占쏙옙占승몌옙 占쏙옙占쏙옙占싹댐옙 占쌘료구占쏙옙
typedef struct kSchedulerStruct
{
    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크
    TCB* pstRunningTask;

    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쌍댐옙 占쏙옙占싸쇽옙占쏙옙 占시곤옙
    int iProcessorTime;

    // 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쌔븝옙占쏙옙占쏙옙 占쏙옙占쏙옙트, 占승쏙옙크占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    LIST vstReadyList[ TASK_MAXREADYLISTCOUNT ];

    // 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙占� 占쏙옙占쏙옙트
    LIST stWaitList;

    // 占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 횟占쏙옙占쏙옙 占쏙옙占쏙옙占싹댐옙 占쌘료구占쏙옙
    int viExecuteCount[ TASK_MAXREADYLISTCOUNT ];

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占싹몌옙 占쏙옙占쏙옙歐占� 占쏙옙占쏙옙 占쌘료구占쏙옙
    QWORD qwProcessorLoad;

    // 占쏙옙占쏙옙 占승쏙옙크(Idle Task)占쏙옙占쏙옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙
    QWORD qwSpendProcessorTimeInIdleTask;
} SCHEDULER;


/*
// 추첨 스케줄링을 위한 SCHEDULER
typedef struct kSchedulerStruct
{
    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크
    TCB* pstRunningTask;

    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쌍댐옙 占쏙옙占싸쇽옙占쏙옙 占시곤옙
    int iProcessorTime;

    // 占쏙옙占쏙옙 占승쏙옙크占쏙옙占쏙옙 티占쏙옙 占쏙옙 占쏙옙
    unsigned int curTicketTotal;

    // 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쌔븝옙占쏙옙占쏙옙 占쏙옙占쏙옙트.
    // 占쏙옙 占쏙옙占쏙옙트 占쏙옙占쏙옙占� 占승쏙옙크占쏙옙 占쎌선占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 티占쏙옙占쏙옙 占싸울옙占쌨는댐옙.
    LIST vstReadyList;

    // 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙占� 占쏙옙占쏙옙트
    LIST stWaitList;

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占싹몌옙 占쏙옙占쏙옙歐占� 占쏙옙占쏙옙 占쌘료구占쏙옙
    QWORD qwProcessorLoad;

    // 占쏙옙占쏙옙 占승쏙옙크(Idle Task)占쏙옙占쏙옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙
    QWORD qwSpendProcessorTimeInIdleTask;
} SCHEDULER;
*/

/*
// 보폭 스케줄링을 위한 SCHEDULER
typedef struct kSchedulerStruct
{
    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크
    TCB* pstRunningTask;

    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쌍댐옙 占쏙옙占싸쇽옙占쏙옙 占시곤옙
    int iProcessorTime;

    // 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쌔븝옙占쏙옙占쏙옙 占쏙옙占쏙옙트.
    // 占쏙옙 占쏙옙占쏙옙트 占쏙옙占쏙옙占� 占승쏙옙크占쏙옙 占쎌선占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 티占쏙옙占쏙옙 占싸울옙占쌨는댐옙.
    LIST vstReadyList;

    // 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙占� 占쏙옙占쏙옙트
    LIST stWaitList;

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占싹몌옙 占쏙옙占쏙옙歐占� 占쏙옙占쏙옙 占쌘료구占쏙옙
    QWORD qwProcessorLoad;

    // 占쏙옙占쏙옙 占승쏙옙크(Idle Task)占쏙옙占쏙옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙
    QWORD qwSpendProcessorTimeInIdleTask;
} SCHEDULER;
*/
#pragma pack( pop )

////////////////////////////////////////////////////////////////////////////////
//
// 占쌉쇽옙
//
////////////////////////////////////////////////////////////////////////////////
//==============================================================================
//  占승쏙옙크 풀占쏙옙 占승쏙옙크 占쏙옙占쏙옙
//==============================================================================
static void kInitializeTCBPool( void );
static TCB* kAllocateTCB( void );
static void kFreeTCB( QWORD qwID );
TCB* kCreateTask( QWORD qwFlags, QWORD qwEntryPointAddress );
static void kSetUpTask( TCB* pstTCB, QWORD qwFlags, QWORD qwEntryPointAddress,
        void* pvStackAddress, QWORD qwStackSize );

//==============================================================================
//  占쏙옙占쏙옙占쌕뤄옙 占쏙옙占쏙옙
//==============================================================================
void kInitializeScheduler( void );
void kInitializeLotteryScheduler( void );
void kSetRunningTask( TCB* pstTask );
TCB* kGetRunningTask( void );
static TCB* kGetNextTaskToRun( void );
static TCB* kGetNextTaskToRunLottery(void);
static BOOL kAddTaskToReadyList( TCB* pstTask );
void kSchedule( void );
BOOL kScheduleInInterrupt( void );
void kDecreaseProcessorTime( void );
BOOL kIsProcessorTimeExpired( void );
static TCB* kRemoveTaskFromReadyList( QWORD qwTaskID );
BOOL kChangePriority( QWORD qwID, BYTE bPriority );
BOOL kEndTask( QWORD qwTaskID );
void kExitTask( void );
int kGetReadyTaskCount( void );
int kGetTaskCount( void );
TCB* kGetTCBInTCBPool( int iOffset );
BOOL kIsTaskExist( QWORD qwID );
QWORD kGetProcessorLoad( void );
int kGetPass(int stride);
//void kSetAllPassToZero();
//==============================================================================
//  占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙
//==============================================================================
void kIdleTask( void );
void kHaltProcessorByLoad( void );

#endif /*__TASK_H__*/
