#include "ConsoleShell.h"
#include "Console.h"
#include "Keyboard.h"
#include "Utility.h"

// 커맨드 테이블 정의
SHELLCOMMANDENTRY gs_vstCommandTable[] =
{
        { "help", "Show Help", kHelp },
        { "cls", "Clear Screen", kCls },
        { "totalram", "Show Total RAM Size", kShowTotalRAMSize },
        { "strtod", "String To Decial/Hex Convert", kStringToDecimalHexTest },
        { "shutdown", "Shutdown And Reboot OS", kShutdown },
        { "raisefault", "Raise Page Fault And Protection Fault", kRaiseFault },
		{ "cdummy", "This is Dummy Command", kCDummy},
		{ "rdummy1", "This is Dummy Command", kRDummy1},
		{ "rdummy2", "This is Dummy Command", kRDummy2}
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
                vcCommandBuffer[ iCommandBufferIndex ] = '\0';
                if(historyCount >= 10)
                {
                	for(int i=1; i<historyCount; i++)
                	{
                		kMemCpy(historyBuffer, historyBuffer+i, CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
                	}
					historyCount--;
                }
                kMemCpy(historyBuffer + historyCount, vcCommandBuffer, iCommandBufferIndex+1);
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
            		bKey = ' ';

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
							kTrieFindMostSpecific(resultTrie, vcCommandBuffer, &iCommandBufferIndex);
							kGetCursor( &iCursorX, &iCursorY );
							kSetCursor( 0, iCursorY );
							vcCommandBuffer[iCommandBufferIndex] = '\0';
							kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE );
							kPrintf("%s", vcCommandBuffer);
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
							candBufferIdx = 0;
							tempStrIdx = 0;
							kPrintEveryCandidate(resultTrie, vcCommandBuffer, candidateBuffer, tempStr, &candBufferIdx, &tempStrIdx);
							kPrintf("%s\n", candidateBuffer);

							kPrintf( "%s", CONSOLESHELL_PROMPTMESSAGE );
							kMemSet( vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT );
							iCommandBufferIndex = 0;
            			}
            		}
            		historyIdx = -1;
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
void kHelp( const char* pcCommandBuffer )
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
void kCls( const char* pcParameterBuffer )
{
    // 맨 윗줄은 디버깅 용으로 사용하므로 화면을 지운 후, 라인 1로 커서 이동
    kClearScreen();
    kSetCursor( 0, 1 );
}

/**
 *  총 메모리 크기를 출력
 */
void kShowTotalRAMSize( const char* pcParameterBuffer )
{
    kPrintf( "Total RAM Size = %d MB\n", kGetTotalRAMSize() );
}

/**
 *  문자열로 된 숫자를 숫자로 변환하여 화면에 출력
 */
void kStringToDecimalHexTest( const char* pcParameterBuffer )
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
void kShutdown( const char* pcParamegerBuffer )
{
    kPrintf( "System Shutdown Start...\n" );
    
    // 키보드 컨트롤러를 통해 PC를 재시작
    kPrintf( "Press Any Key To Reboot PC..." );
    kGetCh();
    kReboot();
}

void kRaiseFault( const char* pcParamegerBuffer )
{
    DWORD* pstPage = 0x1ff000;
    *pstPage = 1;
}

void kHistoryPrint(char* vcCommandBuffer, char historyBuffer[][CONSOLESHELL_MAXCOMMANDBUFFERCOUNT],
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
void kCDummy(const char* pcParamegerBuffer)
{

}

void kRDummy1(const char* pcParamegerBuffer)
{

}

void kRDummy2(const char* pcParamegerBuffer)
{

}

void* kMalloc(int size) {
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

void kFree(void* ptr) {
    free_block* block = (free_block*)(((char*)ptr) - sizeof(int));
    kMemSet((char*)block + sizeof(int), 0, block->size);
    //kFree된 블럭을 빈 공간 리스트의 맨 앞에 추가
    block->next = free_block_list_head.next;
    free_block_list_head.next = block;
}

void kTrieInitialize(Trie* trie)
{
	trie->finish = FALSE;
	trie->count = 0;
	kMemSet(trie->next, NULL, sizeof(trie->next));
}

void kTrieInsert(Trie* trie, const char* key)
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

Trie* kTrieFind(Trie* trie, const char* key)
{
	int curr;

	if(*key == '\0')
		return trie;
	curr = *key - 'a';
	if(trie->next[curr] == NULL)
		return NULL;
	return kTrieFind(trie->next[curr], key+1);
}

//명령어를 최대한 자동입력하는 함수(most specific 부분명령어 찾기)
//kTrieFind함수의 리턴값을 인자로 받는다.
//1. count가 2 이상일 때, next의 count가 현재 count보다 작아지면 most specific
//2. count가 1일 때, finish 노드까지의 문자열이 most specific
void kTrieFindMostSpecific(Trie* trie, char* buffer, int* strIndex)
{
	int startCount = trie->count;
	Trie* curTrie = trie;
	Trie* nextTrie;
	BOOL mostSpecificFound;

	while(curTrie->finish == FALSE)
	{
		mostSpecificFound = TRUE;
		for(int i=0; i<26; i++)
		{
			nextTrie = curTrie->next[i];
			if(nextTrie->count == startCount)
			{
				mostSpecificFound = FALSE;
				buffer[(*strIndex)++] = i + 'a';
				curTrie = nextTrie;
				break;
			}
		}
		if(mostSpecificFound)
			return;
	}
}

//trie의 모든 후보 명령어들을 출력한다.
void kPrintEveryCandidate(Trie* trie, const char* key, char* resultBuffer, char* tempStr, int* bufferIdx, int* tempStrIdx)
{
	int keyLen;
	int tempStrLen;
	if(trie->finish == TRUE)
	{
		keyLen = kStrLen(key);
		tempStrLen = kStrLen(tempStr);
		kMemCpy(resultBuffer, key, keyLen);
		(*bufferIdx) += keyLen;
		kMemCpy(resultBuffer + *bufferIdx, tempStr, tempStrLen);
		kMemSet(resultBuffer + *bufferIdx + 1, ' ', 1);
		(*bufferIdx) += (tempStrLen + 1);
		return;
	}

	for(int i=0; i<26; i++)
	{
		if(trie->next[i] != NULL)
		{
			tempStr[(*tempStrIdx)++] = i + 'a';
			kPrintEveryCandidate(trie, key, resultBuffer, tempStr, bufferIdx, tempStrIdx);
			tempStr[(*tempStrIdx)--] = '\0';
		}
	}
}

