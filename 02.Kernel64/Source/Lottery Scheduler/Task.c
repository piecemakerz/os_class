#include "Task.h"
#include "Descriptor.h"
#include "Utility.h"

// 스케줄러 관련 자료구조
static SCHEDULER gs_stScheduler;
static TCBPOOLMANAGER gs_stTCBPoolManager;
// 스케줄링 순서 출력 함수인 kTraceScheduler에서 사용하는 전역변수
// ConsoleShell.c에서 참조한다.
int trace_task_sequence = 0;
int task_count[TASK_MAXCOUNT] = { 0, };

//==============================================================================
//  태스크 풀과 태스크 관련
//==============================================================================
/**
 *  태스크 풀 초기화
 */
static void kInitializeTCBPool( void )
{
    int i;

    kMemSet( &( gs_stTCBPoolManager ), 0, sizeof( gs_stTCBPoolManager ) );

    // 태스크 풀의 어드레스를 지정하고 초기화
    gs_stTCBPoolManager.pstStartAddress = ( TCB* ) TASK_TCBPOOLADDRESS;
    kMemSet( TASK_TCBPOOLADDRESS, 0, sizeof( TCB ) * TASK_MAXCOUNT );

    // TCB에 ID 할당
    for( i = 0 ; i < TASK_MAXCOUNT ; i++ )
    {
        gs_stTCBPoolManager.pstStartAddress[ i ].stLink.qwID = i;
    }

    // TCB의 최대 개수와 할당된 횟수를 초기화
    gs_stTCBPoolManager.iMaxCount = TASK_MAXCOUNT;
    gs_stTCBPoolManager.iAllocatedCount = 1;
}

/**
 *  TCB를 할당 받음
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
        // ID의 상위 32비트가 0이면 할당되지 않은 TCB
        if( ( gs_stTCBPoolManager.pstStartAddress[ i ].stLink.qwID >> 32 ) == 0 )
        {
            pstEmptyTCB = &( gs_stTCBPoolManager.pstStartAddress[ i ] );
            break;
        }
    }

    // 상위 32비트를 0이 아닌 값으로 설정해서 할당된 TCB로 설정
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
 *  TCB를 해제함
 */
static void kFreeTCB( QWORD qwID )
{
    int i;

    // 태스크 ID의 하위 32비트가 인덱스 역할을 함
   i = GETTCBOFFSET( qwID );

   // TCB를 초기화하고 ID 설정
    kMemSet( &( gs_stTCBPoolManager.pstStartAddress[ i ].stContext ), 0, sizeof( CONTEXT ) );
    gs_stTCBPoolManager.pstStartAddress[ i ].stLink.qwID = i;

    gs_stTCBPoolManager.iUseCount--;
}

/**
 *  태스크를 생성
 *      태스크 ID에 따라서 스택 풀에서 스택 자동 할당
 */

// 멀티레벨 큐와 추첨 스케줄링을 위한 kCreateTask
TCB* kCreateTask( QWORD qwFlags, void* pvMemoryAddress, QWORD qwMemorySize,
					QWORD qwEntryPointAddress )
{
    TCB* pstTask, * pstProcess;
    void* pvStackAddress;
    BOOL bPreviousFlag;

    // 임계 영역 시작
    bPreviousFlag = kLockForSystemData();
    pstTask = kAllocateTCB();
    if( pstTask == NULL )
    {
        // 임계영역 끝
    	kUnlockForSystemData(bPreviousFlag);
        return NULL;
    }

	// 현재 프로세스 또는 스레드가 속한 프로세스를 검색
    pstProcess = kGetProcessByThread( kGetRunningTask() );
    // 만약 프로세스가 없다면 아무런 작업도 하지 않음
    if( pstProcess == NULL )
    {
        kFreeTCB( pstTask->stLink.qwID );
        // 임계 영역 끝
        kUnlockForSystemData( bPreviousFlag );
        return NULL;
    }

    // 스레드를 생성하는 경우라면 내가 속한 프로세스의 자식 스레드 리스트에 연결함
    if( qwFlags & TASK_FLAGS_THREAD )
    {
        // 현재 스레드의 프로세스를 찾아서 생성할 스레드에 프로세스 정보를 상속
        pstTask->qwParentProcessID = pstProcess->stLink.qwID;
        pstTask->pvMemoryAddress = pstProcess->pvMemoryAddress;
        pstTask->qwMemorySize = pstProcess->qwMemorySize;

        // 부모 프로세스의 자식 스레드 리스트에 추가
        kAddListToTail( &( pstProcess->stChildThreadList ), &( pstTask->stThreadLink ) );
    }
    // 프로세스는 파라미터로 넘어온 값을 그대로 설정
    else
    {
        pstTask->qwParentProcessID = pstProcess->stLink.qwID;
        pstTask->pvMemoryAddress = pvMemoryAddress;
        pstTask->qwMemorySize = qwMemorySize;
    }

    // 스레드의 ID를 태스크 ID와 동일하게 설정
    pstTask->stThreadLink.qwID = pstTask->stLink.qwID;

    // 임계 영역 끝
    kUnlockForSystemData(bPreviousFlag);

    // 태스크 ID로 스택 어드레스 계산, 하위 32비트가 스택 풀의 오프셋 역할 수행
    pvStackAddress = ( void* ) ( TASK_STACKPOOLADDRESS + ( TASK_STACKSIZE *
            GETTCBOFFSET(pstTask->stLink.qwID)) );

    // TCB를 설정한 후 준비 리스트에 삽입하여 스케줄링될 수 있도록 함
    kSetUpTask( pstTask, qwFlags, qwEntryPointAddress, pvStackAddress,
            TASK_STACKSIZE );

	// 자식 스레드 리스트를 초기화
    kInitializeList( &( pstTask->stChildThreadList ) );

    // 임계 영역 시작
    bPreviousFlag = kLockForSystemData();

    // 태스크를 준비 리스트에 삽입
    kAddTaskToReadyList( pstTask );

    // 임계 영역 끝
    kUnlockForSystemData(bPreviousFlag);

    return pstTask;
}

/**
 *  파라미터를 이용해서 TCB를 설정
 */
static void kSetUpTask( TCB* pstTCB, QWORD qwFlags, QWORD qwEntryPointAddress,
                 void* pvStackAddress, QWORD qwStackSize )
{
    // 콘텍스트 초기화
    kMemSet( pstTCB->stContext.vqRegister, 0, sizeof( pstTCB->stContext.vqRegister ) );

    // 스택에 관련된 RSP, RBP 레지스터 설정
    pstTCB->stContext.vqRegister[ TASK_RSPOFFSET ] = ( QWORD ) pvStackAddress +
            qwStackSize - 8;
    pstTCB->stContext.vqRegister[ TASK_RBPOFFSET ] = ( QWORD ) pvStackAddress +
            qwStackSize - 8;

    // Return Address 영역에 kExitTask() 함수의 어드레스를 삽입하여 태스크의 엔트리
    // 포인트 함수를 빠져나감과 동시에 kExitTask() 함수로 이동하도록 함
    *( QWORD * ) ( ( QWORD ) pvStackAddress + qwStackSize - 8 ) = ( QWORD ) kExitTask;

    // 세그먼트 셀렉터 설정
    pstTCB->stContext.vqRegister[ TASK_CSOFFSET ] = GDT_KERNELCODESEGMENT;
    pstTCB->stContext.vqRegister[ TASK_DSOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_ESOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_FSOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_GSOFFSET ] = GDT_KERNELDATASEGMENT;
    pstTCB->stContext.vqRegister[ TASK_SSOFFSET ] = GDT_KERNELDATASEGMENT;

    // RIP 레지스터와 인터럽트 플래그 설정
    pstTCB->stContext.vqRegister[ TASK_RIPOFFSET ] = qwEntryPointAddress;

    // RFLAGS 레지스터의 IF 비트(비트 9)를 1로 설정하여 인터럽트 활성화
        pstTCB->stContext.vqRegister[ TASK_RFLAGSOFFSET ] |= 0x0200;

        // 스택과 플래그 저장
        pstTCB->pvStackAddress = pvStackAddress;
        pstTCB->qwStackSize = qwStackSize;
        pstTCB->qwFlags = qwFlags;
}

//==============================================================================
//  스케줄러 관련
//==============================================================================
/**
 *  스케줄러를 초기화
 *      스케줄러를 초기화하는데 필요한 TCB 풀과 init 태스크도 같이 초기화
 */

// 추첨 스케줄링 전용 kInitializeScheduler
void kInitializeScheduler( void )
{
	TCB* pstTask;
    kInitializeTCBPool();
    kInitializeList( &( gs_stScheduler.vstReadyList ) );
    kInitializeList( &( gs_stScheduler.stWaitList ) );
	// TCB를 할당 받아 부팅을 수행한 태스크를 커널 최초의 프로세스로 설정
    pstTask = kAllocateTCB();
    gs_stScheduler.pstRunningTask = pstTask;
    pstTask->qwFlags = TASK_FLAGS_HIGHEST | TASK_FLAGS_PROCESS | TASK_FLAGS_SYSTEM;
    pstTask->qwParentProcessID = pstTask->stLink.qwID;
    pstTask->pvMemoryAddress = ( void* ) 0x100000;
    pstTask->qwMemorySize = 0x500000;
    pstTask->pvStackAddress = ( void* ) 0x600000;
    pstTask->qwStackSize = 0x100000;
    gs_stScheduler.qwSpendProcessorTimeInIdleTask = 0;
    gs_stScheduler.qwProcessorLoad = 0;
    gs_stScheduler.curTicketTotal = 0;
}

/**
 *  현재 수행 중인 태스크를 설정
 */
void kSetRunningTask( TCB* pstTask )
{
	BOOL bPreviousFlag;

	// 임계 영역 시작
	bPreviousFlag = kLockForSystemData();

	gs_stScheduler.pstRunningTask = pstTask;

	// 임계 영역 끝
	kUnlockForSystemData( bPreviousFlag );
}

/**
 *  현재 수행 중인 태스크를 반환
 */
TCB* kGetRunningTask( void )
{
    BOOL bPreviousFlag;
    TCB* pstRunningTask;

    // 임계 영역 시작
    bPreviousFlag = kLockForSystemData();

    pstRunningTask = gs_stScheduler.pstRunningTask;

    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );

    return pstRunningTask;
}

/**
 *  태스크 리스트에서 다음으로 실행할 태스크를 얻음
 */

// 추첨 스케줄링 전용 kGetNextTaskToRun
static TCB* kGetNextTaskToRun( void )
{
    TCB* pstTarget, * curTask;
    int iTaskCount, i, randomSelection, curTicketsCount;
	// rand를 호출하기 전 seed값 초기화
    srand(time());
    // 현재 수행중인 태스크도 스케줄링에서 고려하기 위해 ReadyList에 추가
    kAddTaskToReadyList(gs_stScheduler.pstRunningTask);
    pstTarget = (TCB*) kGetHeaderFromList( &(gs_stScheduler.vstReadyList) );
    iTaskCount = kGetListCount( &( gs_stScheduler.vstReadyList ) );
    // 추첨 스케줄러가 다음 스케줄 할 태스크를 결정하기 위해 추첨
    randomSelection = rand(gs_stScheduler.curTicketTotal);
    curTicketsCount = 0;
    if(iTaskCount == 0)
    {
    	return NULL;
    }
	// tracescheduler가 요청되었다면 trace_task_sequence개의 태스크 출력
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
	for( i = 0 ; i < iTaskCount ; i++ )
	{
		curTicketsCount += GETPRIORITY(pstTarget->qwFlags);
		// 현재까지의 ticket 값의 합이 추첨 값 이상이면 현재 태스크 스케줄링
		if( curTicketsCount >= randomSelection )
		{
			pstTarget = (TCB*)kRemoveTaskFromReadyList( pstTarget->stLink.qwID );
			break;
		}
		pstTarget = kGetNextFromList( &(gs_stScheduler.vstReadyList), pstTarget );
	}
	// 현재 이미 실행중이던 태스크가 다시 스케줄링 되는 경우
	// ReadyList에서 해당 태스크를 제거한다.
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

/**
 *  태스크를 스케줄러의 준비 리스트에 삽입
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
    // 태스크가 ReadyList에 추가되면 리스트의 총 ticket의 합 갱신
    gs_stScheduler.curTicketTotal += bPriority;
    return TRUE;
}

/**
 *  준비 큐에서 태스크를 제거
 */

// 추첨 스케줄링 전용 kRemoveTaskFromReadyList
static TCB* kRemoveTaskFromReadyList( QWORD qwTaskID )
{
    TCB* pstTarget;
    BYTE bPriority;
    if( GETTCBOFFSET( qwTaskID ) >= TASK_MAXCOUNT )
    {
        return NULL;
    }
    pstTarget = &( gs_stTCBPoolManager.pstStartAddress[ GETTCBOFFSET( qwTaskID ) ] );
    if( pstTarget->stLink.qwID != qwTaskID )
    {
        return NULL;
    }
    bPriority = GETPRIORITY( pstTarget->qwFlags );
    pstTarget = kRemoveList( &( gs_stScheduler.vstReadyList ), qwTaskID );
    // ReadyList에서 태스크가 제거된다면 전체 태스크 티켓 수 갱신
    gs_stScheduler.curTicketTotal -= bPriority;
    return pstTarget;
}

/**
 *  태스크의 우선 순위를 변경함
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
    bPreviousFlag = kLockForSystemData();
    pstTarget = gs_stScheduler.pstRunningTask;
    if( pstTarget->stLink.qwID == qwTaskID )
    {
        SETPRIORITY( pstTarget->qwFlags, bPriority );
    }
    else
    {
        pstTarget = kRemoveTaskFromReadyList( qwTaskID );
        if( pstTarget == NULL )
        {
            pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
            if( pstTarget != NULL )
            {
                // 占쎌선 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
                SETPRIORITY( pstTarget->qwFlags, bPriority );
            }
        }
        else
        {
            SETPRIORITY( pstTarget->qwFlags, bPriority );
            kAddTaskToReadyList( pstTarget );
        }
    }
    kUnlockForSystemData(bPreviousFlag);
    return TRUE;
}

/**
 *  다른 태스크를 찾아서 전환
 *      인터럽트나 예외가 발생했을 때 호출하면 안됨
 */

// 추첨 스케줄링과 보폭 스케줄링 전용 kSchedule
void kSchedule( void )
{
    TCB* pstRunningTask, * pstNextTask;
    BOOL bPreviousFlag;
    if( kGetReadyTaskCount() < 1 )
    {
        return ;
    }
    bPreviousFlag = kLockForSystemData();
    pstNextTask = kGetNextTaskToRun();
    if( pstNextTask == NULL )
    {
    	kUnlockForSystemData(bPreviousFlag);
        return ;
    }
	pstRunningTask = gs_stScheduler.pstRunningTask;
	gs_stScheduler.pstRunningTask = pstNextTask;
	if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
	{
		gs_stScheduler.qwSpendProcessorTimeInIdleTask +=
			TASK_PROCESSORTIME - gs_stScheduler.iProcessorTime;
	}
	gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;
	// 이미 현재 실행중인 태스크를 다시 스케줄하는 경우 이 단계를 건너뛴다.
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
	pstNextTask->qwRunningTime += 1;
	kUnlockForSystemData(bPreviousFlag);
}

/**
 *  인터럽트가 발생했을 때, 다른 태스크를 찾아 전환
 *      반드시 인터럽트나 예외가 발생했을 때 호출해야 함
 */

// 추첨 스케줄링 및 보폭 스케줄링 전용 kScheduleInInterrupt
BOOL kScheduleInInterrupt( void )
{
    TCB* pstRunningTask, * pstNextTask;
    char* pcContextAddress;
    BOOL bPreviousFlag;
    bPreviousFlag = kLockForSystemData();
    pstNextTask = kGetNextTaskToRun();
    if( pstNextTask == NULL )
    {
    	kUnlockForSystemData(bPreviousFlag);
        return FALSE;
    }
    pcContextAddress = ( char* ) IST_STARTADDRESS + IST_SIZE - sizeof( CONTEXT );
	pstRunningTask = gs_stScheduler.pstRunningTask;
	gs_stScheduler.pstRunningTask = pstNextTask;
	if( ( pstRunningTask->qwFlags & TASK_FLAGS_IDLE ) == TASK_FLAGS_IDLE )
	{
		gs_stScheduler.qwSpendProcessorTimeInIdleTask += TASK_PROCESSORTIME;
	}
	// 현재 실행중인 태스크를 다시 스케줄링 하는 경우 이 단계를 건너뜀
	if(pstRunningTask != pstNextTask)
	{
		if( pstRunningTask->qwFlags & TASK_FLAGS_ENDTASK )
		{
			kAddListToTail( &( gs_stScheduler.stWaitList ), pstRunningTask );
		}
		else
		{
			kMemCpy( &( pstRunningTask->stContext ), pcContextAddress, sizeof( CONTEXT ) );
			kAddTaskToReadyList( pstRunningTask );
		}
	}
	pstNextTask->qwRunningTime += 1;
	kUnlockForSystemData(bPreviousFlag);
	// 현재 실행중인 태스크를 다시 스케줄링 하는 경우 이 단계를 건너뜀
	if(pstRunningTask != pstNextTask)
	{
		kMemCpy( pcContextAddress, &( pstNextTask->stContext ), sizeof( CONTEXT ) );
	}
    gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;
    return TRUE;
}

/**
 *  프로세서를 사용할 수 있는 시간을 하나 줄임
 */
void kDecreaseProcessorTime( void )
{
    if( gs_stScheduler.iProcessorTime > 0 )
    {
        gs_stScheduler.iProcessorTime--;
    }
}

/**
 *  프로세서를 사용할 수 있는 시간이 다 되었는지 여부를 반환
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
 *  태스크를 종료
 */
BOOL kEndTask( QWORD qwTaskID )
{
    TCB* pstTarget;
    BYTE bPriority;
    BOOL bPreviousFlag;

    // 임계 영역 시작
    bPreviousFlag = kLockForSystemData();

    // 현재 실행중인 태스크이면 EndTask 비트를 설정하고 태스크를 전환
    pstTarget = gs_stScheduler.pstRunningTask;
    if( pstTarget->stLink.qwID == qwTaskID )
    {
        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );

        // 임계 영역 끝
        kUnlockForSystemData( bPreviousFlag );

        kSchedule();

        // 태스크가 전환 되었으므로 아래 코드는 절대 실행되지 않음
        while( 1 ) ;
    }
    // 실행 중인 태스크가 아니면 준비 큐에서 직접 찾아서 대기 리스트에 연결
    else
    {
        // 준비 리스트에서 태스크를 찾지 못하면 직접 태스크를 찾아서 태스크 종료 비트를
        // 설정
        pstTarget = kRemoveTaskFromReadyList( qwTaskID );
        if( pstTarget == NULL )
        {
            // 태스크 ID로 직접 찾아서 설정
            pstTarget = kGetTCBInTCBPool( GETTCBOFFSET( qwTaskID ) );
            if( pstTarget != NULL )
            {
                pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
                SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );
            }
            // 임계 영역 끝
            kUnlockForSystemData( bPreviousFlag );
            return TRUE;
        }

        pstTarget->qwFlags |= TASK_FLAGS_ENDTASK;
        SETPRIORITY( pstTarget->qwFlags, TASK_FLAGS_WAIT );
        kAddListToTail( &( gs_stScheduler.stWaitList ), pstTarget );
    }
    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );
    return TRUE;
}

/**
 *  태스크가 자신을 종료함
 */
void kExitTask( void )
{
    kEndTask( gs_stScheduler.pstRunningTask->stLink.qwID );
}

/**
 *  준비 큐에 있는 모든 태스크의 수를 반환
 */

// 추첨 스케줄링 및 보폭 스케줄링 전용 kGetReadyTaskCount
int kGetReadyTaskCount( void )
{
    int iTotalCount = 0;
    int i;
    BOOL bPreviousFlag;
    bPreviousFlag = kLockForSystemData();
    iTotalCount += kGetListCount( &( gs_stScheduler.vstReadyList ) );
    kUnlockForSystemData(bPreviousFlag);
    return iTotalCount;
}

/**
 *  전체 태스크의 수를 반환
 */
int kGetTaskCount( void )
{
    int iTotalCount;
    BOOL bPreviousFlag;

    // 준비 큐의 태스크 수를 구한 후, 대기 큐의 태스크 수와 현재 수행 중인 태스크 수를 더함
    iTotalCount = kGetReadyTaskCount();

    // 임계 영역 시작
    bPreviousFlag = kLockForSystemData();

    iTotalCount += kGetListCount( &( gs_stScheduler.stWaitList ) ) + 1;

    // 임계 영역 끝
    kUnlockForSystemData( bPreviousFlag );
    return iTotalCount;
}

/**
 *  TCB 풀에서 해당 오프셋의 TCB를 반환
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
 *  태스크가 존재하는지 여부를 반환
 */
BOOL kIsTaskExist( QWORD qwID )
{
    TCB* pstTCB;

    // ID로 TCB를 반환
    pstTCB = kGetTCBInTCBPool( GETTCBOFFSET( qwID ) );
    // TCB가 없거나 ID가 일치하지 않으면 존재하지 않는 것임
    if( ( pstTCB == NULL ) || ( pstTCB->stLink.qwID != qwID ) )
    {
        return FALSE;
    }
    return TRUE;
}

/**
 *  프로세서의 사용률을 반환
 */
QWORD kGetProcessorLoad( void )
{
    return gs_stScheduler.qwProcessorLoad;
}

/**
 *  스레드가 소속된 프로세스를 반환
 */
static TCB* kGetProcessByThread( TCB* pstThread )
{
    TCB* pstProcess;

    // 만약 내가 프로세스이면 자신을 반환
    if( pstThread->qwFlags & TASK_FLAGS_PROCESS )
    {
        return pstThread;
    }

    // 내가 프로세스가 아니라면, 부모 프로세스로 설정된 태스크 ID를 통해
    // TCB 풀에서 태스크 자료구조 추출
    pstProcess = kGetTCBInTCBPool( GETTCBOFFSET( pstThread->qwParentProcessID ) );

    // 만약 프로세스가 없거나, 태스크 ID가 일치하지 않는다면 NULL을 반환
    if( ( pstProcess == NULL ) || ( pstProcess->stLink.qwID != pstThread->qwParentProcessID ) )
    {
        return NULL;
    }

    return pstProcess;
}

//==============================================================================
//  유휴 태스크 관련
//==============================================================================
/**
 *  유휴 태스크
 *      대기 큐에 삭제 대기중인 태스크를 정리
 */
void kIdleTask( void )
{
    TCB* pstTask, * pstChildThread, * pstProcess;
    QWORD qwLastMeasureTickCount, qwLastSpendTickInIdleTask;
    QWORD qwCurrentMeasureTickCount, qwCurrentSpendTickInIdleTask;
    int i, iCount;
    QWORD qwTaskID;
    BOOL bPreviousFlag;
    void* pstThreadLink;

    // 프로세서 사용량 계산을 위해 기준 정보를 저장
    qwLastSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;
    qwLastMeasureTickCount = kGetTickCount();

    while( 1 )
    {
        // 현재 상태를 저장
        qwCurrentMeasureTickCount = kGetTickCount();
        qwCurrentSpendTickInIdleTask = gs_stScheduler.qwSpendProcessorTimeInIdleTask;

        // 프로세서 사용량을 계산
        // 100 - ( 유휴 태스크가 사용한 프로세서 시간 ) * 100 / ( 시스템 전체에서
        // 사용한 프로세서 시간 )
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

        // 현재 상태를 이전 상태에 보관
        qwLastMeasureTickCount = qwCurrentMeasureTickCount;
        qwLastSpendTickInIdleTask = qwCurrentSpendTickInIdleTask;

        // 프로세서의 부하에 따라 쉬게 함
        kHaltProcessorByLoad();

        // 대기 큐에 대기중인 태스크가 있으면 태스크를 종료함
        if( kGetListCount( &( gs_stScheduler.stWaitList ) ) >= 0 )
        {
            while( 1 )
            {
                	// 임계 영역 시작
                bPreviousFlag = kLockForSystemData();
                pstTask = kRemoveListFromHeader( &( gs_stScheduler.stWaitList ) );
                if( pstTask == NULL )
                {
                    // 임계 영역 끝
                    kUnlockForSystemData( bPreviousFlag );
                    break;
                }

                if( pstTask->qwFlags & TASK_FLAGS_PROCESS )
                {
                    // 프로세스를 종료할 때 자식 스레드가 존재하면 스레드를 모두
                    // 종료하고, 다시 자식 스레드 리스트에 삽입
                    iCount = kGetListCount( &( pstTask->stChildThreadList ) );
                    for( i = 0 ; i < iCount ; i++ )
                    {
                        // 스레드 링크의 어드레스에서 꺼내 스레드를 종료시킴
                        pstThreadLink = ( TCB* ) kRemoveListFromHeader(
                                &( pstTask->stChildThreadList ) );
                        if( pstThreadLink == NULL )
                        {
                            break;
                        }

                        // 자식 스레드 리스트에 연결된 정보는 태스크 자료구조에 있는
                        // stThreadLink의 시작 어드레스이므로, 태스크 자료구조의 시작
                        // 어드레스를 구하려면 별도의 계산이 필요함
                        pstChildThread = GETTCBFROMTHREADLINK( pstThreadLink );

                        // 다시 자식 스레드 리스트에 삽입하여 해당 스레드가 종료될 때
                        // 자식 스레드가 프로세스를 찾아 스스로 리스트에서 제거하도록 함
                        kAddListToTail( &( pstTask->stChildThreadList ),
                                &( pstChildThread->stThreadLink ) );

                        // 자식 스레드를 찾아서 종료
                        kEndTask( pstChildThread->stLink.qwID );
                    }

                    // 아직 자식 스레드가 남아있다면 자식 스레드가 다 종료될 때까지
                    // 기다려야 하므로 다시 대기 리스트에 삽입
                    if( kGetListCount( &( pstTask->stChildThreadList ) ) > 0 )
                    {
                        kAddListToTail( &( gs_stScheduler.stWaitList ), pstTask );

                        // 임계 영역 끝
                        kUnlockForSystemData( bPreviousFlag );
                        continue;
                    }
                    // 프로세스를 종료해야 하므로 할당 받은 메모리 영역을 삭제
                    else
                    {
                        // TODO: 추후에 코드 삽입
                    }
                }
                else if( pstTask->qwFlags & TASK_FLAGS_THREAD )
                {
                    // 스레드라면 프로세스의 자식 스레드 리스트에서 제거
                    pstProcess = kGetProcessByThread( pstTask );
                    if( pstProcess != NULL )
                    {
                        kRemoveList( &( pstProcess->stChildThreadList ), pstTask->stLink.qwID );
                    }
                }

                qwTaskID = pstTask->stLink.qwID;
                kFreeTCB( qwTaskID );
                	// 임계 영역 끝
                kUnlockForSystemData( bPreviousFlag );

                kPrintf( "IDLE: Task ID[0x%q] is completely ended.\n",
                        qwTaskID );
            }
        }

        kSchedule();
    }
}

/**
 *  측정된 프로세서 부하에 따라 프로세서를 쉬게 함
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

