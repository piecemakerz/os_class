#include "ConsoleShell.h"
#include "Console.h"
#include "Keyboard.h"
#include "Utility.h"
#include "../../01.Kernel32/Source/Page.h"
#include "PIT.h"
#include "RTC.h"
#include "AssemblyUtility.h"
#include "Task.h"
#include "Synchronization.h"

static int bTry = 0;

// 커맨드 테이블 정의
SHELLCOMMANDENTRY gs_vstCommandTable[] =
{
        { "help", "Show Help", kHelp },
        { "cls", "Clear Screen", kCls },
        { "totalram", "Show Total RAM Size", kShowTotalRAMSize },
        { "strtod", "String To Decial/Hex Convert", kStringToDecimalHexTest },
        { "shutdown", "Shutdown And Reboot OS", kShutdown },
        { "raisefault", "Raise Page Fault And Protection Fault", kRaiseFault },
		{ "settimer", "Set PIT Controller Counter0, ex)settimer 10(ms) 1(periodic)",
		                kSetTimer },
		{ "wait", "Wait ms Using PIT, ex)wait 100(ms)", kWaitUsingPIT },
		{ "rdtsc", "Read Time Stamp Counter", kReadTimeStampCounter },
		{ "cpuspeed", "Measure Processor Speed", kMeasureProcessorSpeed },
		{ "date", "Show Date And Time", kShowDateAndTime },
		{ "createtask", "Create Task, ex)createtask 1(type) 10(count)", kCreateTestTask },
		{ "changepriority", "Change Task Priority, ex)changepriority 1(ID) 2(Priority)",
		                kChangeTaskPriority },
		{ "tasklist", "Show Task List", kShowTaskList },
		{ "killtask", "End Task, ex)killtask 1(ID) or 0xffffffff(All Task)", kKillTask },
		{ "cpuload", "Show Processor Load", kCPULoad },
		{ "testmutex", "Test Mutex Function", kTestMutex },
};                                     

//==============================================================================
//  실제 셸을 구성하는 코드
//==============================================================================
/**
 *  셸의 메인 루프
 */
void kStartConsoleShell( void )
{
    char vcCommandBuffer[ CONSOLESHELL_MAXCOMMANDBUFFERCOUNT ];
	char historyBuffer[10][CONSOLESHELL_MAXCOMMANDBUFFERCOUNT];
	char candidateBuffer[CONSOLESHELL_MAXCOMMANDBUFFERCOUNT];
	char tempStr[50];
	char lineClear[80];

    int iCommandBufferIndex = 0;
    int iCursorX, iCursorY;
    int historyIdx = -1;
    int historyCount = 0;
    int tempLen;
    int curIdx;
    int candBufferIdx;
    int tempStrIdx;

    Trie* head;
    Trie* resultTrie = NULL;
    BYTE bKey;
    BOOL candidateExists = FALSE;
    BOOL commandWrote = FALSE;

    kMemSet(lineClear, ' ', 79);
    lineClear[79] = '\0';

    curMallocPos = 0x1436F0;
    head = kMalloc(sizeof(Trie));
    kTrieInitialize(head);

    for(int i=0; i<9; i++)
    {
    	kTrieInsert(head, gs_vstCommandTable[i].pcCommand);
    }

    // 프롬프트 출력
    kPrintf( CONSOLESHELL_PROMPTMESSAGE );
    while( 1 )
    {
        // 키가 수신될 때까지 대기
        bKey = kGetCh();
        // Backspace 키 처리
        if( bKey == KEY_BACKSPACE )
        {
            if( iCommandBufferIndex > 0 )
            {
                // 현재 커서 위치를 얻어서 한 문자 앞으로 이동한 다음 공백을 출력하고 
                // 커맨드 버퍼에서 마지막 문자 삭제
                kGetCursor( &iCursorX, &iCursorY );
                kPrintStringXY( iCursorX - 1, iCursorY, " " );
                kSetCursor( iCursorX - 1, iCursorY );
                iCommandBufferIndex--;
            }
        }
        // 엔터 키 처리
        else if( bKey == KEY_ENTER )
        {
            kPrintf( "\n" );
            
            if( iCommandBufferIndex > 0 )
            {
                // 커맨드 버퍼에 있는 명령을 실행
                vcCommandBuffer[ iCommandBufferIndex++ ] = '\0';
                if(historyCount >= 10)
                {
                	for(int i=0; i<historyCount-1; i++)
                	{
                		kMemSet(historyBuffer+i, 0, CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
                		kMemCpy(historyBuffer+i, historyBuffer+(i+1), CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
                	}
					historyCount--;
                }
                kMemCpy(historyBuffer + historyCount, vcCommandBuffer, iCommandBufferIndex);
                historyCount++;
                kExecuteCommand( vcCommandBuffer );
            }
            
            // 프롬프트 출력 및 커맨드 버퍼 초기화
            kPrintf( "%s", CONSOLESHELL_PROMPTMESSAGE );            
            kMemSet( vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT );
            iCommandBufferIndex = 0;
        }
        // 시프트 키, CAPS Lock, NUM Lock, Scroll Lock은 무시
        else if( ( bKey == KEY_LSHIFT ) || ( bKey == KEY_RSHIFT ) ||
                 ( bKey == KEY_CAPSLOCK ) || ( bKey == KEY_NUMLOCK ) ||
                 ( bKey == KEY_SCROLLLOCK ) )
        {
            ;
        }
        else if(bKey == KEY_UP)
        {
        	candidateExists = FALSE;
        	//history 버퍼가 비었으면 리턴
        		//이미 처음 history 인덱스를 확인했다면 리턴
        	if(historyCount == 0 || historyIdx == 0)
        		continue;

        	if(historyIdx == -1)
        		historyIdx = historyCount;

        	historyIdx--;
        	kHistoryPrint(vcCommandBuffer, historyBuffer, lineClear, &iCommandBufferIndex, historyIdx);
        	continue;
        }
        else if(bKey == KEY_DOWN)
        {
        	candidateExists = FALSE;
        	//history를 검색하고 있는 중이 아니라면 리턴
        		//이미 마지막 history 인덱스를 확인했다면 리턴
        	if(historyIdx == -1 || historyIdx == historyCount)
        		continue;

        	historyIdx++;
        	kHistoryPrint(vcCommandBuffer, historyBuffer, lineClear, &iCommandBufferIndex, historyIdx);
			continue;
        }
        else
        {
            // TAB은 공백으로 전환
            if( bKey == KEY_TAB )
            {
            	if(iCommandBufferIndex == 0)
            		bKey == ' ';

            	else
            	{
					//명령어 자동입력을 한 번 요청한 경우 명령어를 최대한 자동입력해준다.
					if(!candidateExists)
					{
						vcCommandBuffer[ iCommandBufferIndex ] = '\0';
						resultTrie = kTrieFind(head, vcCommandBuffer);
						if(resultTrie == NULL)
							;
						else
						{
							candidateExists = TRUE;
							resultTrie = kTrieFindMostSpecific(resultTrie, vcCommandBuffer, &iCommandBufferIndex);
							kGetCursor( &iCursorX, &iCursorY );
							kSetCursor( 0, iCursorY );
							vcCommandBuffer[iCommandBufferIndex] = '\0';
							kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE );
							kPrintf("%s", vcCommandBuffer);

							historyIdx = -1;
							continue;
						}
					}
					//후보 명령어가 존재하는 상태에서 명령어 자동입력을 두 번 이상 요청한 경우 후보 명령어들을 출력해준다.
					//명령어 입력이 완료된 경우 아무런 동작을 하지 않는다.
					else
					{
						if(resultTrie->finish == FALSE)
						{
							kMemSet(candidateBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
							kMemSet(tempStr, '\0', 50);
							vcCommandBuffer[iCommandBufferIndex] = '\0';
							candBufferIdx = 0;
							tempStrIdx = 0;

							kPrintEveryCandidate(resultTrie, vcCommandBuffer, candidateBuffer, tempStr, &candBufferIdx, &tempStrIdx);
							candidateBuffer[candBufferIdx] = '\0';

							kPrintf("\n");
							kPrintf("%s\n", candidateBuffer);

							kPrintf( "%s", CONSOLESHELL_PROMPTMESSAGE );
							kMemSet( vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT );
							iCommandBufferIndex = 0;
						}
					}
					historyIdx = -1;
					candidateExists = FALSE;
					resultTrie = NULL;
					continue;
				}
            }

			// 버퍼에 공간이 남아있을 때만 가능
			if( iCommandBufferIndex < CONSOLESHELL_MAXCOMMANDBUFFERCOUNT )
			{
				vcCommandBuffer[ iCommandBufferIndex++ ] = bKey;
				kPrintf( "%c", bKey );
			}
        }
        historyIdx = -1;
        candidateExists = FALSE;
        resultTrie = NULL;
    }
}

/*
 *  커맨드 버퍼에 있는 커맨드를 비교하여 해당 커맨드를 처리하는 함수를 수행
 */
void kExecuteCommand( const char* pcCommandBuffer )
{
    int i, iSpaceIndex;
    int iCommandBufferLength, iCommandLength;
    int iCount;
    
    // 공백으로 구분된 커맨드를 추출
    iCommandBufferLength = kStrLen( pcCommandBuffer );
    for( iSpaceIndex = 0 ; iSpaceIndex < iCommandBufferLength ; iSpaceIndex++ )
    {
        if( pcCommandBuffer[ iSpaceIndex ] == ' ' )
        {
            break;
        }
    }
    
    // 커맨드 테이블을 검사해서 동일한 이름의 커맨드가 있는지 확인
    iCount = sizeof( gs_vstCommandTable ) / sizeof( SHELLCOMMANDENTRY );
    for( i = 0 ; i < iCount ; i++ )
    {
        iCommandLength = kStrLen( gs_vstCommandTable[ i ].pcCommand );
        // 커맨드의 길이와 내용이 완전히 일치하는지 검사
        if( ( iCommandLength == iSpaceIndex ) &&
            ( kMemCmp( gs_vstCommandTable[ i ].pcCommand, pcCommandBuffer,
                       iSpaceIndex ) == 0 ) )
        {
            gs_vstCommandTable[ i ].pfFunction( pcCommandBuffer + iSpaceIndex + 1 );
            break;
        }
    }

    // 리스트에서 찾을 수 없다면 에러 출력
    if( i >= iCount )
    {
        kPrintf( "'%s' is not found.\n", pcCommandBuffer );
    }
}

/**
 *  파라미터 자료구조를 초기화
 */
void kInitializeParameter( PARAMETERLIST* pstList, const char* pcParameter )
{
    pstList->pcBuffer = pcParameter;
    pstList->iLength = kStrLen( pcParameter );
    pstList->iCurrentPosition = 0;
}

/**
 *  공백으로 구분된 파라미터의 내용과 길이를 반환
 */
int kGetNextParameter( PARAMETERLIST* pstList, char* pcParameter )
{
    int i;
    int iLength;

    // 더 이상 파라미터가 없으면 나감
    if( pstList->iLength <= pstList->iCurrentPosition )
    {
        return 0;
    }
    
    // 버퍼의 길이만큼 이동하면서 공백을 검색
    for( i = pstList->iCurrentPosition ; i < pstList->iLength ; i++ )
    {
        if( pstList->pcBuffer[ i ] == ' ' )
        {
            break;
        }
    }
    
    // 파라미터를 복사하고 길이를 반환
    kMemCpy( pcParameter, pstList->pcBuffer + pstList->iCurrentPosition, i );
    iLength = i - pstList->iCurrentPosition;
    pcParameter[ iLength ] = '\0';

    // 파라미터의 위치 업데이트
    pstList->iCurrentPosition += iLength + 1;
    return iLength;
}
    
//==============================================================================
//  커맨드를 처리하는 코드
//==============================================================================
/**
 *  셸 도움말을 출력
 */
static void kHelp( const char* pcCommandBuffer )
{
    int i;
    int iCount;
    int iCursorX, iCursorY;
    int iLength, iMaxCommandLength = 0;
    
    
    kPrintf( "=========================================================\n" );
    kPrintf( "                    MINT64 Shell Help                    \n" );
    kPrintf( "=========================================================\n" );
    
    iCount = sizeof( gs_vstCommandTable ) / sizeof( SHELLCOMMANDENTRY );

    // 가장 긴 커맨드의 길이를 계산
    for( i = 0 ; i < iCount ; i++ )
    {
        iLength = kStrLen( gs_vstCommandTable[ i ].pcCommand );
        if( iLength > iMaxCommandLength )
        {
            iMaxCommandLength = iLength;
        }
    }
    
    // 도움말 출력
    for( i = 0 ; i < iCount ; i++ )
    {
        kPrintf( "%s", gs_vstCommandTable[ i ].pcCommand );
        kGetCursor( &iCursorX, &iCursorY );
        kSetCursor( iMaxCommandLength, iCursorY );
        kPrintf( "  - %s\n", gs_vstCommandTable[ i ].pcHelp );
    }
}

/**
 *  화면을 지움 
 */
static void kCls( const char* pcParameterBuffer )
{
    // 맨 윗줄은 디버깅 용으로 사용하므로 화면을 지운 후, 라인 1로 커서 이동
    kClearScreen();
    kSetCursor( 0, 1 );
}

/**
 *  총 메모리 크기를 출력
 */
static void kShowTotalRAMSize( const char* pcParameterBuffer )
{
    kPrintf( "Total RAM Size = %d MB\n", kGetTotalRAMSize() );
}

/**
 *  문자열로 된 숫자를 숫자로 변환하여 화면에 출력
 */
static void kStringToDecimalHexTest( const char* pcParameterBuffer )
{
    char vcParameter[ 100 ];
    int iLength;
    PARAMETERLIST stList;
    int iCount = 0;
    long lValue;
    
    // 파라미터 초기화
    kInitializeParameter( &stList, pcParameterBuffer );
    
    while( 1 )
    {
        // 다음 파라미터를 구함, 파라미터의 길이가 0이면 파라미터가 없는 것이므로
        // 종료
        iLength = kGetNextParameter( &stList, vcParameter );
        if( iLength == 0 )
        {
            break;
        }

        // 파라미터에 대한 정보를 출력하고 16진수인지 10진수인지 판단하여 변환한 후
        // 결과를 printf로 출력
        kPrintf( "Param %d = '%s', Length = %d, ", iCount + 1, 
                 vcParameter, iLength );

        // 0x로 시작하면 16진수, 그외는 10진수로 판단
        if( kMemCmp( vcParameter, "0x", 2 ) == 0 )
        {
            lValue = kAToI( vcParameter + 2, 16 );
            kPrintf( "HEX Value = %q\n", lValue );
        }
        else
        {
            lValue = kAToI( vcParameter, 10 );
            kPrintf( "Decimal Value = %d\n", lValue );
        }
        
        iCount++;
    }
}

/**
 *  PC를 재시작(Reboot)
 */
static void kShutdown( const char* pcParamegerBuffer )
{
    kPrintf( "System Shutdown Start...\n" );
    
    // 키보드 컨트롤러를 통해 PC를 재시작
    kPrintf( "Press Any Key To Reboot PC..." );
    kGetCh();
    kReboot();
}

static inline void invlpg(void* m)
{
    asm volatile ( "invlpg (%0)" : : "b"(m) : "memory" );
}

static void kRaiseFault( const char* pcParamegerBuffer )
{
    // PTENTRY* pstPTEntry = ( PTENTRY* ) 0x142000;
    // PTENTRY* pstEntry = &(pstPTEntry[511]);
    // DWORD* pstPage = 0x1ff000;
    long *ptr = 0x1ff000;
    DWORD data = 0x99;
    if(bTry == 0){
        // invlpg(0x1ff000);
        data = *ptr;
        // pstEntry->dwAttributeAndLowerBaseAddress = 0x2;
        // *pstPage = 1;
        bTry++;
    }
    else if (bTry == 1) {
        *ptr = 0;
        // pstEntry->dwAttributeAndLowerBaseAddress = 0x1;
        // invlpg(0x1ff000);
        // *pstPage = 1;
        bTry++;
    }
}

static void kHistoryPrint(char* vcCommandBuffer, char historyBuffer[][CONSOLESHELL_MAXCOMMANDBUFFERCOUNT],
		char* lineClear, int* iCommandBufferIndex, int historyIdx)
{
	int iCursorX;
	int iCursorY;
	kMemSet( vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT );
	kMemCpy( vcCommandBuffer, historyBuffer[historyIdx], kStrLen(historyBuffer[historyIdx]));
	*iCommandBufferIndex = kStrLen(historyBuffer[historyIdx]);

	kGetCursor( &iCursorX, &iCursorY );
	kSetCursor( 0, iCursorY );
	kPrintf("%s", lineClear);
	kSetCursor( 0, iCursorY );
	kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE );
	kPrintf("%s", vcCommandBuffer );
}

static void* kMalloc(int size) {
	//가장 가까운 align_to의 배수로 반올림하여 설정
	//새로 할당될 메모리 블럭은 size크기만큼의 메모리와 크기를 나타내는 int변수를 추가로 가져야 한다.
    size = (size + sizeof(int) + (align_to - 1)) & ~ (align_to - 1);
    //block = 다음 kMalloc 빈 공간
    free_block* block = free_block_list_head.next;
    //head = 빈 공간 리스트 헤더의 next 변수에 대한 포인터
    free_block** head = &(free_block_list_head.next);
    //만약 다음 kMalloc 빈 공간이 이미 할당되어 있다면
    while (block != 0) {
    		//만약 할당된 빈 공간에 size크기의 메모리를 할당할 수 있다면 할당한다.
        if (block->size >= size) {
        		//할당한 노드를 빈 공간 리스트에서 제거
            *head = block->next;
            	//할당된 빈 공간 반환
            	//데이터는 next포인터 변수를 덮어쓴다.
            return ((char*)block) + sizeof(int);
        }
        	//다음 빈 공간으로 이동
        head = &(block->next);
        block = block->next;
    }

    //데이터를 저장할 수 있는 할당된 빈 공간이 없다면 빈 공간을 size 크기만큼 할당한다.
    block = (free_block*)(curMallocPos);
    block->size = size - sizeof(int);
    curMallocPos += size;

    return ((char*)block) + sizeof(int);
}

static void kFree(void* ptr) {
    free_block* block = (free_block*)(((char*)ptr) - sizeof(int));
    kMemSet((char*)block + sizeof(int), 0, block->size);
    //kFree된 블럭을 빈 공간 리스트의 맨 앞에 추가
    block->next = free_block_list_head.next;
    free_block_list_head.next = block;
}

static void kTrieInitialize(Trie* trie)
{
	trie->finish = FALSE;
	trie->count = 0;
	kMemSet(trie->next, NULL, sizeof(Trie*));
}

static void kTrieInsert(Trie* trie, const char* key)
{
	int curr;
	trie->count++;

	if(*key == '\0')
		trie->finish = TRUE;
	else
	{
		curr = *key - 'a';
		if(trie->next[curr] == NULL){
			trie->next[curr] = (Trie*)kMalloc(sizeof(Trie));
			kTrieInitialize(trie->next[curr]);
		}
		kTrieInsert(trie->next[curr], key+1);
	}
}

static Trie* kTrieFind(Trie* trie, const char* key)
{
	int curr;

	if(*key == '\0')
		return trie;
	curr = *key - 'a';
	if(curr > 26 || curr < 0 || trie->next[curr] == NULL)
		return NULL;
	return kTrieFind(trie->next[curr], key+1);
}

//명령어를 최대한 자동입력하는 함수(most specific 부분명령어 찾기)
//kTrieFind함수의 리턴값을 인자로 받는다.
//1. count가 2 이상일 때, next의 count가 현재 count보다 작아지면 most specific
//2. count가 1일 때, finish 노드까지의 문자열이 most specific
static Trie* kTrieFindMostSpecific(Trie* trie, char* buffer, int* strIndex)
{
	int startCount = trie->count;
	Trie* nextTrie;
	BOOL mostSpecificFound;

	while(trie->finish == FALSE)
	{
		mostSpecificFound = TRUE;
		for(int i=0; i<26; i++)
		{
			nextTrie = trie->next[i];
			if(nextTrie->count == startCount)
			{
				mostSpecificFound = FALSE;
				buffer[(*strIndex)++] = i + 'a';
				trie = nextTrie;
				break;
			}
		}
		if(mostSpecificFound)
			return trie;
	}

	return trie;
}

//trie의 모든 후보 명령어들을 출력한다.
static void kPrintEveryCandidate(Trie* trie, const char* key, char* resultBuffer, char* tempStr, int* bufferIdx, int* tempStrIdx)
{
	int keyLen;

	if(trie->finish == TRUE)
	{
		keyLen = kStrLen(key);
		kMemCpy(resultBuffer + *bufferIdx, key, keyLen);
		(*bufferIdx) += keyLen;

		kMemCpy(resultBuffer + *bufferIdx, tempStr, *tempStrIdx);
		(*bufferIdx) += (*tempStrIdx);
		resultBuffer[(*bufferIdx)++] = ' ';
		return;
	}

	for(int i=0; i<26; i++)
	{
		if(trie->next[i] != NULL)
		{
			tempStr[*tempStrIdx] = i + 'a';
			(*tempStrIdx)++;
			kPrintEveryCandidate(trie->next[i], key, resultBuffer, tempStr, bufferIdx, tempStrIdx);
			(*tempStrIdx)--;
			tempStr[*tempStrIdx] = '\0';
		}
	}
}

/**
 *  PIT 컨트롤러의 카운터 0 설정
 */
static void kSetTimer( const char* pcParameterBuffer )
{
    char vcParameter[ 100 ];
    PARAMETERLIST stList;
    long lValue;
    BOOL bPeriodic;

    // 파라미터 초기화
    kInitializeParameter( &stList, pcParameterBuffer );

    // milisecond 추출
    if( kGetNextParameter( &stList, vcParameter ) == 0 )
    {
        kPrintf( "ex)settimer 10(ms) 1(periodic)\n" );
        return ;
    }
    lValue = kAToI( vcParameter, 10 );

    // Periodic 추출
    if( kGetNextParameter( &stList, vcParameter ) == 0 )
    {
        kPrintf( "ex)settimer 10(ms) 1(periodic)\n" );
        return ;
    }
    bPeriodic = kAToI( vcParameter, 10 );

    kInitializePIT( MSTOCOUNT( lValue ), bPeriodic );
    kPrintf( "Time = %d ms, Periodic = %d Change Complete\n", lValue, bPeriodic );
}

/**
 *  PIT 컨트롤러를 직접 사용하여 ms 동안 대기
 */
static void kWaitUsingPIT( const char* pcParameterBuffer )
{
    char vcParameter[ 100 ];
    int iLength;
    PARAMETERLIST stList;
    long lMillisecond;
    int i;

    // 파라미터 초기화
    kInitializeParameter( &stList, pcParameterBuffer );
    if( kGetNextParameter( &stList, vcParameter ) == 0 )
    {
        kPrintf( "ex)wait 100(ms)\n" );
        return ;
    }

    lMillisecond = kAToI( pcParameterBuffer, 10 );
    kPrintf( "%d ms Sleep Start...\n", lMillisecond );

    // 인터럽트를 비활성화하고 PIT 컨트롤러를 통해 직접 시간을 측정
    kDisableInterrupt();
    for( i = 0 ; i < lMillisecond / 30 ; i++ )
    {
        kWaitUsingDirectPIT( MSTOCOUNT( 30 ) );
    }
    kWaitUsingDirectPIT( MSTOCOUNT( lMillisecond % 30 ) );
    kEnableInterrupt();
    kPrintf( "%d ms Sleep Complete\n", lMillisecond );

    // 타이머 복원
    kInitializePIT( MSTOCOUNT( 1 ), TRUE );
}

/**
 *  타임 스탬프 카운터를 읽음
 */
static void kReadTimeStampCounter( const char* pcParameterBuffer )
{
    QWORD qwTSC;

    qwTSC = kReadTSC();
    kPrintf( "Time Stamp Counter = %q\n", qwTSC );
}

/**
 *  프로세서의 속도를 측정
 */
static void kMeasureProcessorSpeed( const char* pcParameterBuffer )
{
    int i;
    QWORD qwLastTSC, qwTotalTSC = 0;

    kPrintf( "Now Measuring." );

    // 10초 동안 변화한 타임 스탬프 카운터를 이용하여 프로세서의 속도를 간접적으로 측정
    kDisableInterrupt();
    for( i = 0 ; i < 200 ; i++ )
    {
        qwLastTSC = kReadTSC();
        kWaitUsingDirectPIT( MSTOCOUNT( 50 ) );
        qwTotalTSC += kReadTSC() - qwLastTSC;

        kPrintf( "." );
    }
    // 타이머 복원
    kInitializePIT( MSTOCOUNT( 1 ), TRUE );
    kEnableInterrupt();

    kPrintf( "\nCPU Speed = %d MHz\n", qwTotalTSC / 10 / 1000 / 1000 );
}

/**
 *  RTC 컨트롤러에 저장된 일자 및 시간 정보를 표시
 */
static void kShowDateAndTime( const char* pcParameterBuffer )
{
    BYTE bSecond, bMinute, bHour;
    BYTE bDayOfWeek, bDayOfMonth, bMonth;
    WORD wYear;

    // RTC 컨트롤러에서 시간 및 일자를 읽음
    kReadRTCTime( &bHour, &bMinute, &bSecond );
    kReadRTCDate( &wYear, &bMonth, &bDayOfMonth, &bDayOfWeek );

    kPrintf( "Date: %d/%d/%d %s, ", wYear, bMonth, bDayOfMonth,
             kConvertDayOfWeekToString( bDayOfWeek ) );
    kPrintf( "Time: %d:%d:%d\n", bHour, bMinute, bSecond );
}

// TCB 자료구조와 스택 정의
static TCB gs_vstTask[ 2 ] = { 0, };
static QWORD gs_vstStack[ 1024 ] = { 0, };

/**
 *  태스크 전환을 테스트하는 태스크
 */
static void kTestTask( void )
{
    int i = 0;

    while( 1 )
    {
        // 메시지를 출력하고 키 입력을 대기
        kPrintf( "[%d] This message is from kTestTask. Press any key to switch "
                 "kConsoleShell~!!\n", i++ );
        kGetCh();

        // 위에서 키가 입력되면 태스크를 전환
        kSwitchContext( &( gs_vstTask[ 1 ].stContext ), &( gs_vstTask[ 0 ].stContext ) );
    }
}

/**
 *  태스크 1
 *      화면 테두리를 돌면서 문자를 출력
 */
static void kTestTask1( void )
{
    BYTE bData;
    int i = 0, iX = 0, iY = 0, iMargin, j;
    CHARACTER* pstScreen = ( CHARACTER* ) CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;

    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    pstRunningTask = kGetRunningTask();
    iMargin = ( pstRunningTask->stLink.qwID & 0xFFFFFFFF ) % 10;

    // 화면 네 귀퉁이를 돌면서 문자 출력
   for(j = 0; j <  20000; j++)
   {
        switch( i )
        {
        case 0:
            iX++;
            if( iX >= ( CONSOLE_WIDTH - iMargin ) )
            {
                i = 1;
            }
            break;

        case 1:
            iY++;
            if( iY >= ( CONSOLE_HEIGHT - iMargin ) )
            {
                i = 2;
            }
            break;

        case 2:
            iX--;
            if( iX < iMargin )
            {
                i = 3;
            }
            break;

        case 3:
            iY--;
            if( iY < iMargin )
            {
                i = 0;
            }
            break;
        }

        // 문자 및 색깔 지정
        pstScreen[ iY * CONSOLE_WIDTH + iX ].bCharactor = bData;
        pstScreen[ iY * CONSOLE_WIDTH + iX ].bAttribute = bData & 0x0F;
        bData++;

        // 다른 태스크로 전환
       //kSchedule();
    }
   kExitTask();
}

/**
 *  태스크 2
 *      자신의 ID를 참고하여 특정 위치에 회전하는 바람개비를 출력
 */
static void kTestTask2( void )
{
    int i = 0, iOffset;
    CHARACTER* pstScreen = ( CHARACTER* ) CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;
    char vcData[ 4 ] = { '-', '\\', '|', '/' };

    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    pstRunningTask = kGetRunningTask();
    iOffset = ( pstRunningTask->stLink.qwID & 0xFFFFFFFF ) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT -
        ( iOffset % ( CONSOLE_WIDTH * CONSOLE_HEIGHT ) );

    while( 1 )
    {
        // 회전하는 바람개비를 표시
        pstScreen[ iOffset ].bCharactor = vcData[ i % 4 ];
        // 색깔 지정
        pstScreen[ iOffset ].bAttribute = ( iOffset % 15 ) + 1;
        i++;

        // 다른 태스크로 전환
        kSchedule();
    }
}

/**
 *  태스크를 생성해서 멀티 태스킹 수행
 */
void kCreateTestTask( const char* pcParameterBuffer )
{
    PARAMETERLIST stList;
    char vcType[ 30 ];
    char vcCount[ 30 ];
    int i;

    // 파라미터를 추출
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcType );
    kGetNextParameter( &stList, vcCount );

    switch( kAToI( vcType, 10 ) )
    {
    // 타입 1 태스크 생성
    case 1:
        for( i = 0 ; i < kAToI( vcCount, 10 ) ; i++ )
        {
            if( kCreateTask( TASK_FLAGS_LOW, ( QWORD ) kTestTask1 ) == NULL )
            {
                break;
            }
        }

        kPrintf( "Task1 %d Created\n", i );
        break;

    // 타입 2 태스크 생성
    case 2:
    default:
        for( i = 0 ; i < kAToI( vcCount, 10 ) ; i++ )
        {
            if( kCreateTask( TASK_FLAGS_LOW, ( QWORD ) kTestTask2 ) == NULL )
            {
                break;
            }
        }

        kPrintf( "Task2 %d Created\n", i );
        break;
    }
}

/**
 *  태스크의 우선 순위를 변경
 */
static void kChangeTaskPriority( const char* pcParameterBuffer )
{
    PARAMETERLIST stList;
    char vcID[ 30 ];
    char vcPriority[ 30 ];
    QWORD qwID;
    BYTE bPriority;

    // 파라미터를 추출
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcID );
    kGetNextParameter( &stList, vcPriority );

    // 태스크의 우선 순위를 변경
    if( kMemCmp( vcID, "0x", 2 ) == 0 )
    {
        qwID = kAToI( vcID + 2, 16 );
    }
    else
    {
        qwID = kAToI( vcID, 10 );
    }

    bPriority = kAToI( vcPriority, 10 );

    kPrintf( "Change Task Priority ID [0x%q] Priority[%d] ", qwID, bPriority );
    if( kChangePriority( qwID, bPriority ) == TRUE )
    {
        kPrintf( "Success\n" );
    }
    else
    {
        kPrintf( "Fail\n" );
    }
}

/**
 *  현재 생성된 모든 태스크의 정보를 출력
 */
static void kShowTaskList( const char* pcParameterBuffer )
{
    int i;
    TCB* pstTCB;
    int iCount = 0;

    kPrintf( "=========== Task Total Count [%d] ===========\n", kGetTaskCount() );
    for( i = 0 ; i < TASK_MAXCOUNT ; i++ )
    {
        // TCB를 구해서 TCB가 사용 중이면 ID를 출력
        pstTCB = kGetTCBInTCBPool( i );
        if( ( pstTCB->stLink.qwID >> 32 ) != 0 )
        {
            // 태스크가 10개 출력될 때마다, 계속 태스크 정보를 표시할지 여부를 확인
            if( ( iCount != 0 ) && ( ( iCount % 10 ) == 0 ) )
            {
                kPrintf( "Press any key to continue... ('q' is exit) : " );
                if( kGetCh() == 'q' )
                {
                    kPrintf( "\n" );
                    break;
                }
                kPrintf( "\n" );
            }

            kPrintf( "[%d] Task ID[0x%Q], Priority[%d], Flags[0x%Q]\n", 1 + iCount++,
                     pstTCB->stLink.qwID, GETPRIORITY( pstTCB->qwFlags ),
                     pstTCB->qwFlags);
        }
    }
}

/**
 *  태스크를 종료
 */
static void kKillTask( const char* pcParameterBuffer )
{
    PARAMETERLIST stList;
    char vcID[ 30 ];
    QWORD qwID;
    TCB* pstTCB;
    int i;

    // 파라미터를 추출
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcID );

    // 태스크를 종료
    if( kMemCmp( vcID, "0x", 2 ) == 0 )
    {
        qwID = kAToI( vcID + 2, 16 );
    }
    else
    {
        qwID = kAToI( vcID, 10 );
    }

    // 특정 ID만 종료하는 경우
	if( qwID != 0xFFFFFFFF )
	{
		kPrintf( "Kill Task ID [0x%q] ", qwID );
		if( kEndTask( qwID ) == TRUE )
		{
			kPrintf( "Success\n" );
		}
		else
		{
			kPrintf( "Fail\n" );
		}
	}
	// 콘솔 셸과 유휴 태스크를 제외하고 모든 태스크 종료
	else
	{
		for( i = 2 ; i < TASK_MAXCOUNT ; i++ )
		{
			pstTCB = kGetTCBInTCBPool( i );
			qwID = pstTCB->stLink.qwID;
			if( ( qwID >> 32 ) != 0 )
			{
				kPrintf( "Kill Task ID [0x%q] ", qwID );
				if( kEndTask( qwID ) == TRUE )
				{
					kPrintf( "Success\n" );
				}
				else
				{
					kPrintf( "Fail\n" );
				}
			}
		}
	}
}

/**
 *  프로세서의 사용률을 표시
 */
static void kCPULoad( const char* pcParameterBuffer )
{
    kPrintf( "Processor Load : %d%%\n", kGetProcessorLoad() );
}

// 뮤텍스 테스트용 뮤텍스와 변수
static MUTEX gs_stMutex;
static volatile QWORD gs_qwAdder;

/**
 *  뮤텍스를 테스트하는 태스크
 */
static void kPrintNumberTask( void )
{
    int i;
    int j;
    QWORD qwTickCount;

    // 50ms 정도 대기하여 콘솔 셸이 출력하는 메시지와 겹치지 않도록 함
    qwTickCount = kGetTickCount();
    while( ( kGetTickCount() - qwTickCount ) < 50 )
    {
        kSchedule();
    }

    // 루프를 돌면서 숫자를 출력
    for( i = 0 ; i < 5 ; i++ )
    {
        kLock( &( gs_stMutex ) );
        kPrintf( "Task ID [0x%Q] Value[%d]\n", kGetRunningTask()->stLink.qwID,
                gs_qwAdder );

        gs_qwAdder += 1;
        kUnlock( & ( gs_stMutex ) );

        // 프로세서 소모를 늘리려고 추가한 코드
        for( j = 0 ; j < 30000 ; j++ ) ;
    }

    // 모든 태스크가 종료할 때까지 1초(100ms) 정도 대기
    qwTickCount = kGetTickCount();
    while( ( kGetTickCount() - qwTickCount ) < 1000 )
    {
        kSchedule();
    }

    // 태스크 종료
    kExitTask();
}

/**
 *  뮤텍스를 테스트하는 태스크 생성
 */
static void kTestMutex( const char* pcParameterBuffer )
{
    int i;

    gs_qwAdder = 1;

    // 뮤텍스 초기화
    kInitializeMutex( &gs_stMutex );

    for( i = 0 ; i < 3 ; i++ )
    {
        // 뮤텍스를 테스트하는 태스크를 3개 생성
        kCreateTask( TASK_FLAGS_LOW, ( QWORD ) kPrintNumberTask );
    }
    kPrintf( "Wait Util %d Task End...\n", i );
    kGetCh();
}
