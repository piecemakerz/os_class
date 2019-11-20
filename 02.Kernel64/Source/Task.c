#include "Task.h"
#include "Descriptor.h"
#include "Utility.h"

// 占쏙옙占쏙옙占쌕뤄옙 占쏙옙占쏙옙 占쌘료구占쏙옙
static SCHEDULER gs_stScheduler;
static TCBPOOLMANAGER gs_stTCBPoolManager;
int trace_task_sequence = 0;

//==============================================================================
//  占승쏙옙크 풀占쏙옙 占승쏙옙크 占쏙옙占쏙옙
//==============================================================================
/**
 *  占승쏙옙크 풀 占십깍옙화
 */
static void kInitializeTCBPool( void )
{
    int i;

    kMemSet( &( gs_stTCBPoolManager ), 0, sizeof( gs_stTCBPoolManager ) );

    // 占승쏙옙크 풀占쏙옙 占쏙옙藥뱄옙占쏙옙占� 占쏙옙占쏙옙占싹곤옙 占십깍옙화
    gs_stTCBPoolManager.pstStartAddress = ( TCB* ) TASK_TCBPOOLADDRESS;
    kMemSet( TASK_TCBPOOLADDRESS, 0, sizeof( TCB ) * TASK_MAXCOUNT );

    // TCB占쏙옙 ID 占쌀댐옙
    for( i = 0 ; i < TASK_MAXCOUNT ; i++ )
    {
        gs_stTCBPoolManager.pstStartAddress[ i ].stLink.qwID = i;
    }

    // TCB占쏙옙 占쌍댐옙 占쏙옙占쏙옙占쏙옙 占쌀댐옙占� 횟占쏙옙占쏙옙 占십깍옙화
    gs_stTCBPoolManager.iMaxCount = TASK_MAXCOUNT;
    gs_stTCBPoolManager.iAllocatedCount = 1;
}

/**
 *  TCB占쏙옙 占쌀댐옙 占쏙옙占쏙옙
 */
static TCB* kAllocateTCB( void )
{
    TCB* pstEmptyTCB;
    int i;

    if( gs_stTCBPoolManager.iUseCount == gs_stTCBPoolManager.iMaxCount )
    {
        return NULL;
    }

    for( i = 0 ; i < gs_stTCBPoolManager.iMaxCount ; i++ )
    {
        // ID占쏙옙 占쏙옙占쏙옙 32占쏙옙트占쏙옙 0占싱몌옙 占쌀댐옙占쏙옙占� 占쏙옙占쏙옙 TCB
        if( ( gs_stTCBPoolManager.pstStartAddress[ i ].stLink.qwID >> 32 ) == 0 )
        {
            pstEmptyTCB = &( gs_stTCBPoolManager.pstStartAddress[ i ] );
            break;
        }
    }

    // 占쏙옙占쏙옙 32占쏙옙트占쏙옙 0占쏙옙 占싣댐옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쌔쇽옙 占쌀댐옙占� TCB占쏙옙 占쏙옙占쏙옙
    pstEmptyTCB->stLink.qwID = ( ( QWORD ) gs_stTCBPoolManager.iAllocatedCount << 32 ) | i;
    gs_stTCBPoolManager.iUseCount++;
    gs_stTCBPoolManager.iAllocatedCount++;
    if( gs_stTCBPoolManager.iAllocatedCount == 0 )
    {
        gs_stTCBPoolManager.iAllocatedCount = 1;
    }

    return pstEmptyTCB;
}

/**
 *  TCB占쏙옙 占쏙옙占쏙옙占쏙옙
 */
static void kFreeTCB( QWORD qwID )
{
    int i;

    // 占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 32占쏙옙트占쏙옙 占싸듸옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙
   i = GETTCBOFFSET( qwID );

    // TCB占쏙옙 占십깍옙화占싹곤옙 ID 占쏙옙占쏙옙
    kMemSet( &( gs_stTCBPoolManager.pstStartAddress[ i ].stContext ), 0, sizeof( CONTEXT ) );
    gs_stTCBPoolManager.pstStartAddress[ i ].stLink.qwID = i;

    gs_stTCBPoolManager.iUseCount--;
}

/**
 *  占승쏙옙크占쏙옙 占쏙옙占쏙옙
 *      占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 풀占쏙옙占쏙옙 占쏙옙占쏙옙 占쌘듸옙 占쌀댐옙
 */

TCB* kCreateTask( QWORD qwFlags, QWORD qwEntryPointAddress )
{
    TCB* pstTask;
    void* pvStackAddress;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();
    pstTask = kAllocateTCB();
    if( pstTask == NULL )
    {
    		// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    	kUnlockForSystemData(bPreviousFlag);
        return NULL;
    }

    //占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);

    // 占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 占쏙옙藥뱄옙占� 占쏙옙占�, 占쏙옙占쏙옙 32占쏙옙트占쏙옙 占쏙옙占쏙옙 풀占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    pvStackAddress = ( void* ) ( TASK_STACKPOOLADDRESS + ( TASK_STACKSIZE *
            GETTCBOFFSET(pstTask->stLink.qwID)) );

    // TCB占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙占싹울옙 占쏙옙占쏙옙占쌕몌옙占쏙옙 占쏙옙 占쌍듸옙占쏙옙 占쏙옙
    kSetUpTask( pstTask, qwFlags, qwEntryPointAddress, pvStackAddress,
            TASK_STACKSIZE );

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占승쏙옙크占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙
    kAddTaskToReadyList( pstTask );

    //占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);

    return pstTask;
}

// 보폭 스케줄링 전용 kCreateTask
/*
TCB* kCreateTask( QWORD qwFlags, QWORD qwEntryPointAddress )
{
    TCB* pstTask;
    void* pvStackAddress;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();
    pstTask = kAllocateTCB();
    if( pstTask == NULL )
    {
    		// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    	kUnlockForSystemData(bPreviousFlag);
        return NULL;
    }

    //占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);

    // 占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 占쏙옙藥뱄옙占� 占쏙옙占�, 占쏙옙占쏙옙 32占쏙옙트占쏙옙 占쏙옙占쏙옙 풀占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    pvStackAddress = ( void* ) ( TASK_STACKPOOLADDRESS + ( TASK_STACKSIZE *
            GETTCBOFFSET(pstTask->stLink.qwID)) );

    // TCB占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙占싹울옙 占쏙옙占쏙옙占쌕몌옙占쏙옙 占쏙옙 占쌍듸옙占쏙옙 占쏙옙
    kSetUpTask( pstTask, qwFlags, qwEntryPointAddress, pvStackAddress,
            TASK_STACKSIZE );

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    pstTask->iPass = 0;
    // 占승쏙옙크占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙
    kAddTaskToReadyList( pstTask );
    pstTask->iPass = TASK_PASS_MAX;

    //占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);

    return pstTask;
}

*/
/**
 *  占식띰옙占쏙옙拷占� 占싱울옙占쌔쇽옙 TCB占쏙옙 占쏙옙占쏙옙
 */
static void kSetUpTask( TCB* pstTCB, QWORD qwFlags, QWORD qwEntryPointAddress,
                 void* pvStackAddress, QWORD qwStackSize )
{
    // 占쏙옙占쌔쏙옙트 占십깍옙화
    kMemSet( pstTCB->stContext.vqRegister, 0, sizeof( pstTCB->stContext.vqRegister ) );

    // 占쏙옙占시울옙 占쏙옙占시듸옙 RSP, RBP 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    pstTCB->stContext.vqRegister[ TASK_RSPOFFSET ] = ( QWORD ) pvStackAddress +
            qwStackSize;
    pstTCB->stContext.vqRegister[ TASK_RBPOFFSET ] = ( QWORD ) pvStackAddress +
            qwStackSize;

    // 占쏙옙占쌓몌옙트 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    pstTCB->stContext.vqRegister[ TASK_CSOFFSET ] = GDT_KERNELCODESEGMENT;
    pstTCB->stContext.vqRegister[ TASK_DSOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_ESOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_FSOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_GSOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_SSOFFSET ] = GDT_KERNELDATASEGMENT;

    // RIP 占쏙옙占쏙옙占쏙옙占싶울옙 占쏙옙占싶뤄옙트 占시뤄옙占쏙옙 占쏙옙占쏙옙
    pstTCB->stContext.vqRegister[ TASK_RIPOFFSET ] = qwEntryPointAddress;

    // RFLAGS 占쏙옙占쏙옙占쏙옙占쏙옙占쏙옙 IF 占쏙옙트(占쏙옙트 9)占쏙옙 1占쏙옙 占쏙옙占쏙옙占싹울옙 占쏙옙占싶뤄옙트 활占쏙옙화
    pstTCB->stContext.vqRegister[ TASK_RFLAGSOFFSET ] |= 0x0200;

    // 占쏙옙占시곤옙 占시뤄옙占쏙옙 占쏙옙占쏙옙
    pstTCB->pvStackAddress = pvStackAddress;
    pstTCB->qwStackSize = qwStackSize;
    pstTCB->qwFlags = qwFlags;
    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쌕뤄옙占쏙옙 占쏙옙占쏙옙 占쌘듸옙
}

//==============================================================================
//  占쏙옙占쏙옙占쌕뤄옙 占쏙옙占쏙옙
//==============================================================================
/**
 *  占쏙옙占쏙옙占쌕뤄옙占쏙옙 占십깍옙화
 *      占쏙옙占쏙옙占쌕뤄옙占쏙옙 占십깍옙화占싹는듸옙 占십울옙占쏙옙 TCB 풀占쏙옙 init 占승쏙옙크占쏙옙 占쏙옙占쏙옙 占십깍옙화
 */
/*
void kInitializeScheduler( void )
{
	int i;

	// 占승쏙옙크 풀 占십깍옙화
	kInitializeTCBPool();

	// 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 횟占쏙옙占쏙옙 占십깍옙화占싹곤옙 占쏙옙占� 占쏙옙占쏙옙트占쏙옙 占십깍옙화
	for( i = 0 ; i < TASK_MAXREADYLISTCOUNT ; i++ )
	{
		kInitializeList( &( gs_stScheduler.vstReadyList[ i ] ) );
		gs_stScheduler.viExecuteCount[ i ] = 0;
	}
	kInitializeList( &( gs_stScheduler.stWaitList ) );

	// TCB占쏙옙 占쌀댐옙 占쌨억옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占싹울옙, 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 TCB占쏙옙 占쌔븝옙
	gs_stScheduler.pstRunningTask = kAllocateTCB();
	gs_stScheduler.pstRunningTask->qwFlags = TASK_FLAGS_HIGHEST;

	// 占쏙옙占싸쇽옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙求쨉占� 占쏙옙占쏙옙求占� 占쌘료구占쏙옙 占십깍옙화
	gs_stScheduler.qwSpendProcessorTimeInIdleTask = 0;
	gs_stScheduler.qwProcessorLoad = 0;
}
*/


// 추첨 스케줄링 전용 kInitializeScheduler
void kInitializeScheduler( void )
{
    // 占승쏙옙크 풀 占십깍옙화
    kInitializeTCBPool();

    // 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占� 占쏙옙占쏙옙트 占십깍옙화
    kInitializeList( &( gs_stScheduler.vstReadyList ) );
    kInitializeList( &( gs_stScheduler.stWaitList ) );

    // TCB占쏙옙 占쌀댐옙 占쌨억옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占싹울옙, 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 TCB占쏙옙 占쌔븝옙
    gs_stScheduler.pstRunningTask = kAllocateTCB();
    gs_stScheduler.pstRunningTask->qwFlags = TASK_FLAGS_HIGHEST;

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙求쨉占� 占쏙옙占쏙옙求占� 占쌘료구占쏙옙 占십깍옙화
    gs_stScheduler.qwSpendProcessorTimeInIdleTask = 0;
    gs_stScheduler.qwProcessorLoad = 0;
    gs_stScheduler.curTicketTotal = 0;
}


/*
// 보폭 스케줄링 전용 kInitializeScheduler
void kInitializeScheduler( void )
{
    // 占승쏙옙크 풀 占십깍옙화
    kInitializeTCBPool();

    // 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占� 占쏙옙占쏙옙트 占십깍옙화
    kInitializeList( &( gs_stScheduler.vstReadyList ) );
    kInitializeList( &( gs_stScheduler.stWaitList ) );

    // TCB占쏙옙 占쌀댐옙 占쌨억옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占싹울옙, 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 TCB占쏙옙 占쌔븝옙
    gs_stScheduler.pstRunningTask = kAllocateTCB();
    gs_stScheduler.pstRunningTask->qwFlags = TASK_FLAGS_HIGHEST;
    gs_stScheduler.pstRunningTask->iPass = TASK_PASS_MAX;

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙求쨉占� 占쏙옙占쏙옙求占� 占쌘료구占쏙옙 占십깍옙화
    gs_stScheduler.qwSpendProcessorTimeInIdleTask = 0;
    gs_stScheduler.qwProcessorLoad = 0;
}
*/

/**
 *  占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙
 */
void kSetRunningTask( TCB* pstTask )
{
	BOOL bPreviousFlag;

	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
	bPreviousFlag = kLockForSystemData();

    gs_stScheduler.pstRunningTask = pstTask;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);
}

/**
 *  占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙환
 */
TCB* kGetRunningTask( void )
{
	BOOL bPreviousFlag;
	TCB* pstRunningTask;

	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
	bPreviousFlag = kLockForSystemData();

	pstRunningTask = gs_stScheduler.pstRunningTask;

	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
	kUnlockForSystemData(bPreviousFlag);

    return pstRunningTask;
}

/**
 *  占승쏙옙크 占쏙옙占쏙옙트占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙
 */
/*
static TCB* kGetNextTaskToRun( void )
{
    TCB* pstTarget = NULL;
    int iTaskCount, i, j;

    if(trace_task_sequence != 0)
	{
		kPrintf("curTask: %d, ", gs_stScheduler.pstRunningTask->stLink.qwID);
	}
    // 큐占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占� 큐占쏙옙 占승쏙옙크占쏙옙 1회占쏙옙 占쏙옙占쏙옙占� 占쏙옙占�, 占쏙옙占� 큐占쏙옙 占쏙옙占싸쇽옙占쏙옙占쏙옙
    // 占썹보占싹울옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙占쏙옙 NULL占쏙옙 占쏙옙占� 占싼뱄옙 占쏙옙 占쏙옙占쏙옙
    for( j = 0 ; j < 2 ; j++ )
    {
        // 占쏙옙占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 확占쏙옙占싹울옙 占쏙옙占쏙옙占쌕몌옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙
        for( i = 0 ; i < TASK_MAXREADYLISTCOUNT ; i++ )
        {
            iTaskCount = kGetListCount( &( gs_stScheduler.vstReadyList[ i ] ) );

            // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 횟占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占승쏙옙크 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙
            // 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙
            if( gs_stScheduler.viExecuteCount[ i ] < iTaskCount )
            {
                pstTarget = ( TCB* ) kRemoveListFromHeader(
                                        &( gs_stScheduler.vstReadyList[ i ] ) );
                gs_stScheduler.viExecuteCount[ i ]++;
                break;
            }
            // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 횟占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 횟占쏙옙占쏙옙 占십깍옙화占싹곤옙 占쏙옙占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙 占썹보占쏙옙
            else
            {
                gs_stScheduler.viExecuteCount[ i ] = 0;
            }
        }

        // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
        if( pstTarget != NULL )
        {
            break;
        }
    }

    if(trace_task_sequence != 0)
	{
		kPrintf("nextTask: %d\n", pstTarget->stLink.qwID);
		trace_task_sequence--;
	}

    return pstTarget;
}
*/

// 추첨 스케줄링 전용 kGetNextTaskToRun
static TCB* kGetNextTaskToRun( void )
{
    TCB* pstTarget, * curTask;
    int iTaskCount, i, randomSelection, curTicketsCount;

    srand(time());
    kAddTaskToReadyList(gs_stScheduler.pstRunningTask);

    pstTarget = (TCB*) kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );
    iTaskCount = kGetListCount( &( gs_stScheduler.vstReadyList ) );
    randomSelection = rand(gs_stScheduler.curTicketTotal);
    curTicketsCount = 0;

    if(iTaskCount == 0)
    {
    	return NULL;
    }

    if(trace_task_sequence != 0)
	{
		curTask = (TCB*) kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );
		kPrintf("curTask: %d, Lottery: %d, readyList: ",
				gs_stScheduler.pstRunningTask->stLink.qwID, randomSelection);

		for( i = 0 ; i < iTaskCount ; i++ )
		{
			kPrintf("%d/%d, ", curTask->stLink.qwID, GETPRIORITY(curTask->qwFlags));
			curTask = (TCB*)kGetNextFromList( &(gs_stScheduler.vstReadyList), curTask );
		}
	}

	// 占쏙옙첨 占쏙옙占쏙옙占� 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙
	for( i = 0 ; i < iTaskCount ; i++ )
	{
		curTicketsCount += GETPRIORITY(pstTarget->qwFlags);
		if( curTicketsCount >= randomSelection )
		{
			pstTarget = (TCB*)kRemoveTaskFromReadyList( pstTarget->stLink.qwID );
			break;
		}
		pstTarget = kGetNextFromList( &(gs_stScheduler.vstReadyList), pstTarget );
	}

	if(pstTarget != gs_stScheduler.pstRunningTask)
	{
		kRemoveTaskFromReadyList( gs_stScheduler.pstRunningTask->stLink.qwID );
	}

	if(trace_task_sequence != 0)
	{
		kPrintf("Select: %d\n", pstTarget->stLink.qwID);
		trace_task_sequence--;
	}

    return pstTarget;
}


/*
// 보폭 스케줄링 전용 kGetNextTaskToRun
static TCB* kGetNextTaskToRun( void )
{
    TCB* curTask, * pstTarget;
    int iTaskCount, i, curPassMin;
    BOOL allPassFull = TRUE;

    kAddTaskToReadyList(gs_stScheduler.pstRunningTask);
    curTask = (TCB*) kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );
    iTaskCount = kGetListCount( &( gs_stScheduler.vstReadyList ) );
    curPassMin = TASK_PASS_MAX;

    if(iTaskCount == 0)
    {
    	return NULL;
    }

	// 占쏙옙첨 占쏙옙占쏙옙占� 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙
	for( i = 0 ; i < iTaskCount ; i++ )
	{
		if(curPassMin > curTask->iPass)
		{
			curPassMin = curTask->iPass;
			pstTarget = curTask;
			allPassFull = FALSE;
		}
		curTask = (TCB*)kGetNextFromList( &(gs_stScheduler.vstReadyList), curTask );
	}

	if(allPassFull == TRUE)
	{
		kSetAllPassToZero();
		pstTarget = (TCB*)kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );
	}

	if(trace_task_sequence != 0)
	{
		curTask = (TCB*) kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );

		kPrintf("curTask: %d, readyList: ", gs_stScheduler.pstRunningTask->stLink.qwID);

		for( i = 0 ; i < iTaskCount ; i++ )
		{
			kPrintf("%d/%d, ", curTask->stLink.qwID, curTask->iPass);
			curTask = (TCB*)kGetNextFromList( &(gs_stScheduler.vstReadyList), curTask );
		}

		kPrintf("Select: %d\n", pstTarget->stLink.qwID);
		trace_task_sequence--;
	}

	kRemoveTaskFromReadyList( pstTarget->stLink.qwID );
	return pstTarget;
}
*/

/**
 *  占승쏙옙크占쏙옙 占쏙옙占쏙옙占쌕뤄옙占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙
 */

/*
static BOOL kAddTaskToReadyList( TCB* pstTask )
{
    BYTE bPriority;

    bPriority = GETPRIORITY( pstTask->qwFlags );
    if( bPriority >= TASK_MAXREADYLISTCOUNT )
    {
        return FALSE;
    }

    kAddListToTail( &( gs_stScheduler.vstReadyList[ bPriority ] ), pstTask );
    return TRUE;
}
*/

// 추첨 스케줄링 전용 kAddTaskToReadyList
static BOOL kAddTaskToReadyList( TCB* pstTask )
{
    BYTE bPriority;

    bPriority = GETPRIORITY( pstTask->qwFlags );
    if( bPriority > TASK_FLAGS_HIGHEST )
	{
		return FALSE;
	}
    kAddListToTail( &( gs_stScheduler.vstReadyList ), pstTask );
    gs_stScheduler.curTicketTotal += bPriority;
    return TRUE;
}

/*
// 보폭 스케줄링 전용 kAddTaskToReadyList
static BOOL kAddTaskToReadyList( TCB* pstTask )
{
    BYTE bPriority;

    bPriority = GETPRIORITY( pstTask->qwFlags );
    if( bPriority > TASK_PASS_MAX )
	{
		return FALSE;
	}
    kAddListToTail( &( gs_stScheduler.vstReadyList ), pstTask );
    pstTask->iPass += kGetPass(bPriority);
    return TRUE;
}
*/

/**
 *  占쌔븝옙 큐占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙
 */

/*
static TCB* kRemoveTaskFromReadyList( QWORD qwTaskID )
{
    TCB* pstTarget;
    BYTE bPriority;

    // 占승쏙옙크 ID占쏙옙 占쏙옙효占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    if( GETTCBOFFSET( qwTaskID ) >= TASK_MAXCOUNT )
    {
        return NULL;
    }

    // TCB 풀占쏙옙占쏙옙 占쌔댐옙 占승쏙옙크占쏙옙 TCB占쏙옙 찾占쏙옙 占쏙옙占쏙옙占쏙옙 ID占쏙옙 占쏙옙치占싹는곤옙 확占쏙옙
    pstTarget = &( gs_stTCBPoolManager.pstStartAddress[ GETTCBOFFSET( qwTaskID ) ] );
    if( pstTarget->stLink.qwID != qwTaskID )
    {
        return NULL;
    }

    // 占승쏙옙크占쏙옙 占쏙옙占쏙옙占싹댐옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙
    bPriority = GETPRIORITY( pstTarget->qwFlags );

    pstTarget = kRemoveList( &( gs_stScheduler.vstReadyList[ bPriority ]),
                     qwTaskID );
    return pstTarget;
}
*/

// 추첨 스케줄링 전용 kRemoveTaskFromReadyList
static TCB* kRemoveTaskFromReadyList( QWORD qwTaskID )
{
    TCB* pstTarget;
    BYTE bPriority;

    // 占승쏙옙크 ID占쏙옙 占쏙옙효占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    if( GETTCBOFFSET( qwTaskID ) >= TASK_MAXCOUNT )
    {
        return NULL;
    }

    // TCB 풀占쏙옙占쏙옙 占쌔댐옙 占승쏙옙크占쏙옙 TCB占쏙옙 찾占쏙옙 占쏙옙占쏙옙占쏙옙 ID占쏙옙 占쏙옙치占싹는곤옙 확占쏙옙
    pstTarget = &( gs_stTCBPoolManager.pstStartAddress[ GETTCBOFFSET( qwTaskID ) ] );
    if( pstTarget->stLink.qwID != qwTaskID )
    {
        return NULL;
    }

    bPriority = GETPRIORITY( pstTarget->qwFlags );
    pstTarget = kRemoveList( &( gs_stScheduler.vstReadyList ), qwTaskID );
    gs_stScheduler.curTicketTotal -= bPriority;
    return pstTarget;
}

/*
// 보폭 스케줄링 전용 kRemoveTaskFromReadyList
static TCB* kRemoveTaskFromReadyList( QWORD qwTaskID )
{
    TCB* pstTarget;

    // 占승쏙옙크 ID占쏙옙 占쏙옙효占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    if( GETTCBOFFSET( qwTaskID ) >= TASK_MAXCOUNT )
    {
        return NULL;
    }

    // TCB 풀占쏙옙占쏙옙 占쌔댐옙 占승쏙옙크占쏙옙 TCB占쏙옙 찾占쏙옙 占쏙옙占쏙옙占쏙옙 ID占쏙옙 占쏙옙치占싹는곤옙 확占쏙옙
    pstTarget = &( gs_stTCBPoolManager.pstStartAddress[ GETTCBOFFSET( qwTaskID ) ] );
    if( pstTarget->stLink.qwID != qwTaskID )
    {
        return NULL;
    }

    pstTarget = kRemoveList( &( gs_stScheduler.vstReadyList ), qwTaskID );
    return pstTarget;
}
*/

/**
 *  占승쏙옙크占쏙옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙
 */
/*
BOOL kChangePriority( QWORD qwTaskID, BYTE bPriority )
{
    TCB* pstTarget;
    BOOL bPreviousFlag;

    if( bPriority > TASK_MAXREADYLISTCOUNT )
    {
        return FALSE;
    }

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占싱몌옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    // PIT 占쏙옙트占싼뤄옙占쏙옙 占쏙옙占싶뤄옙트(IRQ 0)占쏙옙 占쌩삼옙占싹울옙 占승쏙옙크 占쏙옙환占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쏙옙占쏙옙占�
    // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占싱듸옙
    pstTarget = gs_stScheduler.pstRunningTask;
    if( pstTarget->stLink.qwID == qwTaskID )
    {
        SETPRIORITY( pstTarget->qwFlags, bPriority );
    }
    // 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占싣니몌옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙占쏙옙 찾占싣쇽옙 占쌔댐옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占싱듸옙
    else
    {
        // 占쌔븝옙 占쏙옙占쏙옙트占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占쏙옙 占쏙옙占싹몌옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占싣쇽옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
        pstTarget = kRemoveTaskFromReadyList( qwTaskID );
        if( pstTarget == NULL )
        {
            // 占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 찾占싣쇽옙 占쏙옙占쏙옙
            pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
            if( pstTarget != NULL )
            {
                // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
                SETPRIORITY( pstTarget->qwFlags, bPriority );
            }
        }
        else
        {
            // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占싹곤옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쌕쏙옙 占쏙옙占쏙옙
            SETPRIORITY( pstTarget->qwFlags, bPriority );
            kAddTaskToReadyList( pstTarget );
        }
    }
    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);
    return TRUE;
}
*/


// 추첨 스케줄링과 보폭 스케줄링 전용 kChangePriority
BOOL kChangePriority( QWORD qwTaskID, BYTE bPriority )
{
    TCB* pstTarget;
    BOOL bPreviousFlag;

    if( bPriority > TASK_FLAGS_HIGHEST )
    {
        return FALSE;
    }

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占싱몌옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    // PIT 占쏙옙트占싼뤄옙占쏙옙 占쏙옙占싶뤄옙트(IRQ 0)占쏙옙 占쌩삼옙占싹울옙 占승쏙옙크 占쏙옙환占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쏙옙占쏙옙占�
    // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占싱듸옙
    pstTarget = gs_stScheduler.pstRunningTask;
    if( pstTarget->stLink.qwID == qwTaskID )
    {
        SETPRIORITY( pstTarget->qwFlags, bPriority );
    }
    // 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占싣니몌옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙占쏙옙 찾占싣쇽옙 占쌔댐옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占싱듸옙
    else
    {
        // 占쌔븝옙 占쏙옙占쏙옙트占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占쏙옙 占쏙옙占싹몌옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占싣쇽옙 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
        pstTarget = kRemoveTaskFromReadyList( qwTaskID );
        if( pstTarget == NULL )
        {
            // 占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 찾占싣쇽옙 占쏙옙占쏙옙
            pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
            if( pstTarget != NULL )
            {
                // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
                SETPRIORITY( pstTarget->qwFlags, bPriority );
            }
        }
        else
        {
            // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占싹곤옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙 占쌕쏙옙 占쏙옙占쏙옙
            SETPRIORITY( pstTarget->qwFlags, bPriority );
            kAddTaskToReadyList( pstTarget );
        }
    }
    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);
    return TRUE;
}


/**
 *  占쌕몌옙 占승쏙옙크占쏙옙 찾占싣쇽옙 占쏙옙환
 *      占쏙옙占싶뤄옙트占쏙옙 占쏙옙占쌤곤옙 占쌩삼옙占쏙옙占쏙옙 占쏙옙 호占쏙옙占싹몌옙 占싫듸옙
 */
/*
void kSchedule( void )
{
    TCB* pstRunningTask, * pstNextTask;
    BOOL bPreviousFlag;

    // 占쏙옙환占쏙옙 占승쏙옙크占쏙옙 占쌍억옙占� 占쏙옙
    if( kGetReadyTaskCount() < 1 )
    {
        return ;
    }

    // 占쏙옙환占싹댐옙 占쏙옙占쏙옙 占쏙옙占싶뤄옙트占쏙옙 占쌩삼옙占싹울옙 占승쏙옙크 占쏙옙환占쏙옙 占쏙옙 占싹어나占쏙옙 占쏙옙占쏙옙球퓐占� 占쏙옙환占싹댐옙
    // 占쏙옙占쏙옙 占쏙옙占싶뤄옙트占쏙옙 占쌩삼옙占쏙옙占쏙옙 占쏙옙占싹듸옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();
    // 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙
    pstNextTask = kGetNextTaskToRun();

    if( pstNextTask == NULL )
    {
        // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    	kUnlockForSystemData(bPreviousFlag);
        return ;
    }

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쏙옙占쌔쏙옙트 占쏙옙환
	pstRunningTask = gs_stScheduler.pstRunningTask;
	gs_stScheduler.pstRunningTask = pstNextTask;

	// 占쏙옙占쏙옙 占승쏙옙크占쏙옙占쏙옙 占쏙옙환占실억옙占쌕몌옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙킴
	if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
	{
		gs_stScheduler.qwSpendProcessorTimeInIdleTask +=
			TASK_PROCESSORTIME - gs_stScheduler.iProcessorTime;
	}

	// 占쏙옙占싸쇽옙占쏙옙 占쏙옙占� 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙트
	gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;

	// 占승쏙옙크 占쏙옙占쏙옙 占시뤄옙占쌓곤옙 占쏙옙占쏙옙占쏙옙 占쏙옙占� 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏙옙占쏙옙 占십요가 占쏙옙占쏙옙占실뤄옙, 占쏙옙占� 占쏙옙占쏙옙트占쏙옙
	// 占쏙옙占쏙옙占싹곤옙 占쏙옙占쌔쏙옙트 占쏙옙환
	if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
	{
		kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
		kSwitchContext( NULL, &( pstNextTask->stContext ) );
	}
	else
	{
		kAddTaskToReadyList( pstRunningTask );
		kSwitchContext( &( pstRunningTask->stContext ), &( pstNextTask->stContext ) );
	}

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
	kUnlockForSystemData(bPreviousFlag);
}
*/

// 추첨 스케줄링과 보폭 스케줄링 전용 kSchedule
void kSchedule( void )
{
    TCB* pstRunningTask, * pstNextTask;
    BOOL bPreviousFlag;

    // 占쏙옙환占쏙옙 占승쏙옙크占쏙옙 占쌍억옙占� 占쏙옙
    if( kGetReadyTaskCount() < 1 )
    {
        return ;
    }

    // 占쏙옙환占싹댐옙 占쏙옙占쏙옙 占쏙옙占싶뤄옙트占쏙옙 占쌩삼옙占싹울옙 占승쏙옙크 占쏙옙환占쏙옙 占쏙옙 占싹어나占쏙옙 占쏙옙占쏙옙球퓐占� 占쏙옙환占싹댐옙
    // 占쏙옙占쏙옙 占쏙옙占싶뤄옙트占쏙옙 占쌩삼옙占쏙옙占쏙옙 占쏙옙占싹듸옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();
    // 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙

    pstNextTask = kGetNextTaskToRun();

    if( pstNextTask == NULL )
    {
        // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    	kUnlockForSystemData(bPreviousFlag);
        return ;
    }

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쏙옙占쌔쏙옙트 占쏙옙환
	pstRunningTask = gs_stScheduler.pstRunningTask;
	gs_stScheduler.pstRunningTask = pstNextTask;

	// 占쏙옙占쏙옙 占승쏙옙크占쏙옙占쏙옙 占쏙옙환占실억옙占쌕몌옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙킴
	if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
	{
		gs_stScheduler.qwSpendProcessorTimeInIdleTask +=
			TASK_PROCESSORTIME - gs_stScheduler.iProcessorTime;
	}
	// 占쏙옙占싸쇽옙占쏙옙 占쏙옙占� 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙트
	gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;

	// 占승쏙옙크 占쏙옙占쏙옙 占시뤄옙占쌓곤옙 占쏙옙占쏙옙占쏙옙 占쏙옙占� 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏙옙占쏙옙 占십요가 占쏙옙占쏙옙占실뤄옙, 占쏙옙占� 占쏙옙占쏙옙트占쏙옙
	// 占쏙옙占쏙옙占싹곤옙 占쏙옙占쌔쏙옙트 占쏙옙환
	if(pstRunningTask != pstNextTask)
	{
		if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
		{
			kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
			kSwitchContext( NULL, &( pstNextTask->stContext ) );
		}
		else
		{
			kAddTaskToReadyList( pstRunningTask );
			kSwitchContext( &( pstRunningTask->stContext ), &( pstNextTask->stContext ) );
		}
	}
	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
	kUnlockForSystemData(bPreviousFlag);
}

/*
BOOL kScheduleInInterrupt( void )
{
    TCB* pstRunningTask, * pstNextTask;
    char* pcContextAddress;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙환占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    pstNextTask = kGetNextTaskToRun();
    if( pstNextTask == NULL )
    {
    		// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    	kUnlockForSystemData(bPreviousFlag);
        return FALSE;
    }

    //==========================================================================
    //  占승쏙옙크 占쏙옙환 처占쏙옙
    //      占쏙옙占싶뤄옙트 占쌘들러占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쌔쏙옙트占쏙옙 占쌕몌옙 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏘쓰占쏙옙 占쏙옙占쏙옙占쏙옙占� 처占쏙옙
    //==========================================================================
    pcContextAddress = ( char* ) IST_STARTADDRESS + IST_SIZE - sizeof( CONTEXT );

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쏙옙占쌔쏙옙트 占쏙옙환
	pstRunningTask = gs_stScheduler.pstRunningTask;
	gs_stScheduler.pstRunningTask = pstNextTask;

	// 占쏙옙占쏙옙 占승쏙옙크占쏙옙占쏙옙 占쏙옙환占실억옙占쌕몌옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙킴
	if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
	{
		gs_stScheduler.qwSpendProcessorTimeInIdleTask += TASK_PROCESSORTIME;
	}

	// 占승쏙옙크 占쏙옙占쏙옙 占시뤄옙占쌓곤옙 占쏙옙占쏙옙占쏙옙 占쏙옙占�, 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占십곤옙 占쏙옙占� 占쏙옙占쏙옙트占쏙옙占쏙옙 占쏙옙占쏙옙
	if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
	{
		kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
	}
	// 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙占� 占쏙옙占쏙옙占쏙옙 IST占쏙옙 占쌍댐옙 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏙옙占싹곤옙, 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙
	// 占신깍옙
	else
	{
		kMemCpy( &( pstRunningTask->stContext ), pcContextAddress, sizeof( CONTEXT ) );
		kAddTaskToReadyList( pstRunningTask );
	}

	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
	kUnlockForSystemData(bPreviousFlag);

    // 占쏙옙환占쌔쇽옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 Running Task占쏙옙 占쏙옙占쏙옙占싹곤옙 占쏙옙占쌔쏙옙트占쏙옙 IST占쏙옙 占쏙옙占쏙옙占쌔쇽옙
    // 占쌘듸옙占쏙옙占쏙옙 占승쏙옙크 占쏙옙환占쏙옙 占싹어나占쏙옙占쏙옙 占쏙옙
	kMemCpy( pcContextAddress, &( pstNextTask->stContext ), sizeof( CONTEXT ) );

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占� 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙트
    gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;
    return TRUE;
}
*/

/**
 *  占쏙옙占싶뤄옙트占쏙옙 占쌩삼옙占쏙옙占쏙옙 占쏙옙, 占쌕몌옙 占승쏙옙크占쏙옙 찾占쏙옙 占쏙옙환
 *      占쌥듸옙占� 占쏙옙占싶뤄옙트占쏙옙 占쏙옙占쌤곤옙 占쌩삼옙占쏙옙占쏙옙 占쏙옙 호占쏙옙占쌔억옙 占쏙옙
 */

// 추첨 스케줄링 및 보폭 스케줄링 전용 kScheduleInInterrupt
BOOL kScheduleInInterrupt( void )
{
    TCB* pstRunningTask, * pstNextTask;
    char* pcContextAddress;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙환占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    pstNextTask = kGetNextTaskToRun();
    if( pstNextTask == NULL )
    {
    		// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    	kUnlockForSystemData(bPreviousFlag);
        return FALSE;
    }

    //==========================================================================
    //  占승쏙옙크 占쏙옙환 처占쏙옙
    //      占쏙옙占싶뤄옙트 占쌘들러占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쌔쏙옙트占쏙옙 占쌕몌옙 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏘쓰占쏙옙 占쏙옙占쏙옙占쏙옙占� 처占쏙옙
    //==========================================================================
    pcContextAddress = ( char* ) IST_STARTADDRESS + IST_SIZE - sizeof( CONTEXT );

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쏙옙占쌔쏙옙트 占쏙옙환
	pstRunningTask = gs_stScheduler.pstRunningTask;
	gs_stScheduler.pstRunningTask = pstNextTask;

	// 占쏙옙占쏙옙 占승쏙옙크占쏙옙占쏙옙 占쏙옙환占실억옙占쌕몌옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙킴
	if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
	{
		gs_stScheduler.qwSpendProcessorTimeInIdleTask += TASK_PROCESSORTIME;
	}

	if(pstRunningTask != pstNextTask)
	{
		// 占승쏙옙크 占쏙옙占쏙옙 占시뤄옙占쌓곤옙 占쏙옙占쏙옙占쏙옙 占쏙옙占�, 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占십곤옙 占쏙옙占� 占쏙옙占쏙옙트占쏙옙占쏙옙 占쏙옙占쏙옙
		if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
		{
			kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
		}
		// 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙占� 占쏙옙占쏙옙占쏙옙 IST占쏙옙 占쌍댐옙 占쏙옙占쌔쏙옙트占쏙옙 占쏙옙占쏙옙占싹곤옙, 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쌔븝옙 占쏙옙占쏙옙트占쏙옙
		// 占신깍옙
		else
		{
			kMemCpy( &( pstRunningTask->stContext ), pcContextAddress, sizeof( CONTEXT ) );
			kAddTaskToReadyList( pstRunningTask );
		}
	}

	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
	kUnlockForSystemData(bPreviousFlag);

    // 占쏙옙환占쌔쇽옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 Running Task占쏙옙 占쏙옙占쏙옙占싹곤옙 占쏙옙占쌔쏙옙트占쏙옙 IST占쏙옙 占쏙옙占쏙옙占쌔쇽옙
    // 占쌘듸옙占쏙옙占쏙옙 占승쏙옙크 占쏙옙환占쏙옙 占싹어나占쏙옙占쏙옙 占쏙옙
	if(pstRunningTask != pstNextTask)
	{
		kMemCpy( pcContextAddress, &( pstNextTask->stContext ), sizeof( CONTEXT ) );
	}

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙占� 占시곤옙占쏙옙 占쏙옙占쏙옙占쏙옙트
    gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;
    return TRUE;
}


/**
 *  占쏙옙占싸쇽옙占쏙옙占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쌍댐옙 占시곤옙占쏙옙 占싹놂옙 占쏙옙占쏙옙
 */
void kDecreaseProcessorTime( void )
{
    if( gs_stScheduler.iProcessorTime > 0 )
    {
        gs_stScheduler.iProcessorTime--;
    }
}

/**
 *  占쏙옙占싸쇽옙占쏙옙占쏙옙 占쏙옙占쏙옙占� 占쏙옙 占쌍댐옙 占시곤옙占쏙옙 占쏙옙 占실억옙占쏙옙占쏙옙 占쏙옙占싸몌옙 占쏙옙환
 */
BOOL kIsProcessorTimeExpired( void )
{
    if( gs_stScheduler.iProcessorTime <= 0 )
    {
        return TRUE;
    }
    return FALSE;
}

/**
 *  占승쏙옙크占쏙옙 占쏙옙占쏙옙
 */
BOOL kEndTask( QWORD qwTaskID )
{
    TCB* pstTarget;
    BYTE bPriority;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占승쏙옙크占싱몌옙 EndTask 占쏙옙트占쏙옙 占쏙옙占쏙옙占싹곤옙 占승쏙옙크占쏙옙 占쏙옙환
    pstTarget = gs_stScheduler.pstRunningTask;
    if( pstTarget->stLink.qwID == qwTaskID )
    {
        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );

        	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
        kUnlockForSystemData(bPreviousFlag);

        kSchedule();

        // 占승쏙옙크占쏙옙 占쏙옙환 占실억옙占쏙옙占실뤄옙 占싣뤄옙 占쌘듸옙占� 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占� 占쏙옙占쏙옙
        while( 1 ) ;
    }
    // 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占싣니몌옙 占쌔븝옙 큐占쏙옙占쏙옙 占쏙옙占쏙옙 찾占싣쇽옙 占쏙옙占� 占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙
    else
    {
        // 占쌔븝옙 占쏙옙占쏙옙트占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占쏙옙 占쏙옙占싹몌옙 占쏙옙占쏙옙 占승쏙옙크占쏙옙 찾占싣쇽옙 占승쏙옙크 占쏙옙占쏙옙 占쏙옙트占쏙옙
        // 占쏙옙占쏙옙
        pstTarget = kRemoveTaskFromReadyList( qwTaskID );
        if( pstTarget == NULL )
        {
            // 占승쏙옙크 ID占쏙옙 占쏙옙占쏙옙 찾占싣쇽옙 占쏙옙占쏙옙
            pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
            if( pstTarget != NULL )
            {
                pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
                SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );
            }
            	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
            kUnlockForSystemData(bPreviousFlag);
            return TRUE;
        }

        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );
        kAddListToTail( &( gs_stScheduler.stWaitList ), pstTarget );
    }
    return TRUE;
}

/**
 *  占승쏙옙크占쏙옙 占쌘쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙
 */
void kExitTask( void )
{
    kEndTask( gs_stScheduler.pstRunningTask->stLink.qwID );
}

/**
 *  占쌔븝옙 큐占쏙옙 占쌍댐옙 占쏙옙占� 占승쏙옙크占쏙옙 占쏙옙占쏙옙 占쏙옙환
 */

/*
int kGetReadyTaskCount( void )
{
    int iTotalCount = 0;
    int i;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙占� 占쌔븝옙 큐占쏙옙 확占쏙옙占싹울옙 占승쏙옙크 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    for( i = 0 ; i < TASK_MAXREADYLISTCOUNT ; i++ )
    {
        iTotalCount += kGetListCount( &( gs_stScheduler.vstReadyList[ i ] ) );
    }

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);
    return iTotalCount ;
}
*/


// 추첨 스케줄링 및 보폭 스케줄링 전용 kGetReadyTaskCount
int kGetReadyTaskCount( void )
{
    int iTotalCount = 0;
    int i;
    BOOL bPreviousFlag;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    // 占쏙옙占� 占쌔븝옙 큐占쏙옙 확占쏙옙占싹울옙 占승쏙옙크 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    iTotalCount += kGetListCount( &( gs_stScheduler.vstReadyList ) );

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);
    return iTotalCount;
}


/**
 *  占쏙옙체 占승쏙옙크占쏙옙 占쏙옙占쏙옙 占쏙옙환
 */
int kGetTaskCount( void )
{
    int iTotalCount;
    BOOL bPreviousFlag;

    // 占쌔븝옙 큐占쏙옙 占승쏙옙크 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙, 占쏙옙占� 큐占쏙옙 占승쏙옙크 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙 占쏙옙占쏙옙
    iTotalCount = kGetReadyTaskCount();

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
    bPreviousFlag = kLockForSystemData();

    iTotalCount += kGetListCount( &( gs_stScheduler.stWaitList ) ) + 1;

    // 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
    kUnlockForSystemData(bPreviousFlag);
    return iTotalCount;
}

/**
 *  TCB 풀占쏙옙占쏙옙 占쌔댐옙 占쏙옙占쏙옙占쏙옙占쏙옙 TCB占쏙옙 占쏙옙환
 */
TCB* kGetTCBInTCBPool( int iOffset )
{
    if( ( iOffset < -1 ) && ( iOffset > TASK_MAXCOUNT ) )
    {
        return NULL;
    }

    return &( gs_stTCBPoolManager.pstStartAddress[ iOffset ] );
}

/**
 *  占승쏙옙크占쏙옙 占쏙옙占쏙옙占싹댐옙占쏙옙 占쏙옙占싸몌옙 占쏙옙환
 */
BOOL kIsTaskExist( QWORD qwID )
{
    TCB* pstTCB;

    // ID占쏙옙 TCB占쏙옙 占쏙옙환
    pstTCB = kGetTCBInTCBPool( GETTCBOFFSET( qwID ) );
    // TCB占쏙옙 占쏙옙占신놂옙 ID占쏙옙 占쏙옙치占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占십댐옙 占쏙옙占쏙옙
    if( ( pstTCB == NULL ) || ( pstTCB->stLink.qwID != qwID ) )
    {
        return FALSE;
    }
    return TRUE;
}

/**
 *  占쏙옙占싸쇽옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙환
 */
QWORD kGetProcessorLoad( void )
{
    return gs_stScheduler.qwProcessorLoad;
}

//==============================================================================
//  占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙
//==============================================================================
/**
 *  占쏙옙占쏙옙 占승쏙옙크
 *      占쏙옙占� 큐占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占� 占승쏙옙크占쏙옙 占쏙옙占쏙옙
 */
void kIdleTask( void )
{
    TCB* pstTask;
    QWORD qwLastMeasureTickCount, qwLastSpendTickInIdleTask;
    QWORD qwCurrentMeasureTickCount, qwCurrentSpendTickInIdleTask;
    BOOL bPreviousFlag;
    QWORD qwTaskID;

    // 占쏙옙占싸쇽옙占쏙옙 占쏙옙酉� 占쏙옙占쏙옙占� 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
    qwLastSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;
    qwLastMeasureTickCount = kGetTickCount();

    while( 1 )
    {
        // 占쏙옙占쏙옙 占쏙옙占승몌옙 占쏙옙占쏙옙
        qwCurrentMeasureTickCount = kGetTickCount();
        qwCurrentSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;

        // 占쏙옙占싸쇽옙占쏙옙 占쏙옙酉�占쏙옙 占쏙옙占�
        // 100 - ( 占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙 ) * 100 / ( 占시쏙옙占쏙옙 占쏙옙체占쏙옙占쏙옙
        // 占쏙옙占쏙옙占� 占쏙옙占싸쇽옙占쏙옙 占시곤옙 )
        if( qwCurrentMeasureTickCount - qwLastMeasureTickCount == 0 )
        {
            gs_stScheduler.qwProcessorLoad = 0;
        }
        else
        {
            gs_stScheduler.qwProcessorLoad = 100 -
                ( qwCurrentSpendTickInIdleTask - qwLastSpendTickInIdleTask ) *
                100 /( qwCurrentMeasureTickCount - qwLastMeasureTickCount );
        }

        // 占쏙옙占쏙옙 占쏙옙占승몌옙 占쏙옙占쏙옙 占쏙옙占승울옙 占쏙옙占쏙옙
        qwLastMeasureTickCount = qwCurrentMeasureTickCount;
        qwLastSpendTickInIdleTask = qwCurrentSpendTickInIdleTask;

        // 占쏙옙占싸쇽옙占쏙옙占쏙옙 占쏙옙占싹울옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙
        kHaltProcessorByLoad();

        // 占쏙옙占� 큐占쏙옙 占쏙옙占쏙옙占쏙옙占� 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크占쏙옙 占쏙옙占쏙옙占쏙옙
        if( kGetListCount( &( gs_stScheduler.stWaitList ) ) >= 0 )
        {
            while( 1 )
            {
            		// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙占쏙옙
            	bPreviousFlag = kLockForSystemData();
                pstTask = kRemoveListFromHeader( &( gs_stScheduler.stWaitList ) );
                if( pstTask == NULL )
                {
                		// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
                	kUnlockForSystemData(bPreviousFlag);
                	break;
                }
                qwTaskID = pstTask->stLink.qwID;
                kFreeTCB( qwTaskID );
                	// 占쌈곤옙 占쏙옙占쏙옙 占쏙옙
                kUnlockForSystemData(bPreviousFlag);
                kPrintf( "IDLE: Task ID[0x%q] is completely ended.\n",
                                        qwTaskID );
            }
        }

        kSchedule();
    }
}

/**
 *  占쏙옙占쏙옙占쏙옙 占쏙옙占싸쇽옙占쏙옙 占쏙옙占싹울옙 占쏙옙占쏙옙 占쏙옙占싸쇽옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙
 */
void kHaltProcessorByLoad( void )
{
    if( gs_stScheduler.qwProcessorLoad < 40 )
    {
        kHlt();
        kHlt();
        kHlt();
    }
    else if( gs_stScheduler.qwProcessorLoad < 80 )
    {
        kHlt();
        kHlt();
    }
    else if( gs_stScheduler.qwProcessorLoad < 95 )
    {
        kHlt();
    }
}

int kGetPass(int stride)
{
	return TASK_STRIDE_NUM / stride;
}

/*
void kSetAllPassToZero()
{
	TCB * pstTarget;
	int iTaskCount, i;

	pstTarget = (TCB*) kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );
	iTaskCount = kGetListCount( &( gs_stScheduler.vstReadyList ) );

	gs_stScheduler.pstRunningTask->iPass = 0;

	if(iTaskCount == 0)
	{
		return NULL;
	}

	// 占쏙옙첨 占쏙옙占쏙옙占� 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占승쏙옙크 占쏙옙占쏙옙
	for( i = 0 ; i < iTaskCount ; i++ )
	{
		pstTarget->iPass = 0;
		pstTarget = kGetNextFromList( &(gs_stScheduler.vstReadyList), pstTarget );
	}
}
*/
