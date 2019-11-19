#include "ConsoleShell.h"
#include "Console.h"
#include "Keyboard.h"
#include "Utility.h"
#include "../../01.Kernel32/Source/Page.h"
#include "PIT.h"
#include "RTC.h"
#include "AssemblyUtility.h"
#include "Synchronization.h"

static int bTry = 0;

// Ŀ�ǵ� ���̺� ����
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
		{ "tracescheduler", "Trace Task Scheduling", kTraceScheduler },
};                                     

//==============================================================================
//  ���� ���� �����ϴ� �ڵ�
//==============================================================================
/**
 *  ���� ���� ����
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

    for(int i=0; i<18; i++)
    {
    	kTrieInsert(head, gs_vstCommandTable[i].pcCommand);
    }

    // ������Ʈ ���
    kPrintf( CONSOLESHELL_PROMPTMESSAGE );
    while( 1 )
    {
        // Ű�� ���ŵ� ������ ���
        bKey = kGetCh();
        // Backspace Ű ó��
        if( bKey == KEY_BACKSPACE )
        {
            if( iCommandBufferIndex > 0 )
            {
                // ���� Ŀ�� ��ġ�� �� �� ���� ������ �̵��� ���� ������ ����ϰ� 
                // Ŀ�ǵ� ���ۿ��� ������ ���� ����
                kGetCursor( &iCursorX, &iCursorY );
                kPrintStringXY( iCursorX - 1, iCursorY, " " );
                kSetCursor( iCursorX - 1, iCursorY );
                iCommandBufferIndex--;
            }
        }
        // ���� Ű ó��
        else if( bKey == KEY_ENTER )
        {
            kPrintf( "\n" );
            
            if( iCommandBufferIndex > 0 )
            {
                // Ŀ�ǵ� ���ۿ� �ִ� ����� ����
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
            
            // ������Ʈ ��� �� Ŀ�ǵ� ���� �ʱ�ȭ
            kPrintf( "%s", CONSOLESHELL_PROMPTMESSAGE );            
            kMemSet( vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT );
            iCommandBufferIndex = 0;
        }
        // ����Ʈ Ű, CAPS Lock, NUM Lock, Scroll Lock�� ����
        else if( ( bKey == KEY_LSHIFT ) || ( bKey == KEY_RSHIFT ) ||
                 ( bKey == KEY_CAPSLOCK ) || ( bKey == KEY_NUMLOCK ) ||
                 ( bKey == KEY_SCROLLLOCK ) )
        {
            ;
        }
        else if(bKey == KEY_UP)
        {
        	candidateExists = FALSE;
        	//history ���۰� ������� ����
        		//�̹� ó�� history �ε����� Ȯ���ߴٸ� ����
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
        	//history�� �˻��ϰ� �ִ� ���� �ƴ϶�� ����
        		//�̹� ������ history �ε����� Ȯ���ߴٸ� ����
        	if(historyIdx == -1 || historyIdx == historyCount)
        		continue;

        	historyIdx++;
        	kHistoryPrint(vcCommandBuffer, historyBuffer, lineClear, &iCommandBufferIndex, historyIdx);
			continue;
        }
        else
        {
            // TAB�� �������� ��ȯ
            if( bKey == KEY_TAB )
            {
            	if(iCommandBufferIndex == 0)
            		bKey == ' ';

            	else
            	{
					//��ɾ� �ڵ��Է��� �� �� ��û�� ��� ��ɾ �ִ��� �ڵ��Է����ش�.
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
					//�ĺ� ��ɾ �����ϴ� ���¿��� ��ɾ� �ڵ��Է��� �� �� �̻� ��û�� ��� �ĺ� ��ɾ���� ������ش�.
					//��ɾ� �Է��� �Ϸ�� ��� �ƹ��� ������ ���� �ʴ´�.
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

			// ���ۿ� ������ �������� ���� ����
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
 *  Ŀ�ǵ� ���ۿ� �ִ� Ŀ�ǵ带 ���Ͽ� �ش� Ŀ�ǵ带 ó���ϴ� �Լ��� ����
 */
void kExecuteCommand( const char* pcCommandBuffer )
{
    int i, iSpaceIndex;
    int iCommandBufferLength, iCommandLength;
    int iCount;
    
    // �������� ���е� Ŀ�ǵ带 ����
    iCommandBufferLength = kStrLen( pcCommandBuffer );
    for( iSpaceIndex = 0 ; iSpaceIndex < iCommandBufferLength ; iSpaceIndex++ )
    {
        if( pcCommandBuffer[ iSpaceIndex ] == ' ' )
        {
            break;
        }
    }
    
    // Ŀ�ǵ� ���̺��� �˻��ؼ� ������ �̸��� Ŀ�ǵ尡 �ִ��� Ȯ��
    iCount = sizeof( gs_vstCommandTable ) / sizeof( SHELLCOMMANDENTRY );
    for( i = 0 ; i < iCount ; i++ )
    {
        iCommandLength = kStrLen( gs_vstCommandTable[ i ].pcCommand );
        // Ŀ�ǵ��� ���̿� ������ ������ ��ġ�ϴ��� �˻�
        if( ( iCommandLength == iSpaceIndex ) &&
            ( kMemCmp( gs_vstCommandTable[ i ].pcCommand, pcCommandBuffer,
                       iSpaceIndex ) == 0 ) )
        {
            gs_vstCommandTable[ i ].pfFunction( pcCommandBuffer + iSpaceIndex + 1 );
            break;
        }
    }

    // ����Ʈ���� ã�� �� ���ٸ� ���� ���
    if( i >= iCount )
    {
        kPrintf( "'%s' is not found.\n", pcCommandBuffer );
    }
}

/**
 *  �Ķ���� �ڷᱸ���� �ʱ�ȭ
 */
void kInitializeParameter( PARAMETERLIST* pstList, const char* pcParameter )
{
    pstList->pcBuffer = pcParameter;
    pstList->iLength = kStrLen( pcParameter );
    pstList->iCurrentPosition = 0;
}

/**
 *  �������� ���е� �Ķ������ ����� ���̸� ��ȯ
 */
int kGetNextParameter( PARAMETERLIST* pstList, char* pcParameter )
{
    int i;
    int iLength;

    // �� �̻� �Ķ���Ͱ� ������ ����
    if( pstList->iLength <= pstList->iCurrentPosition )
    {
        return 0;
    }
    
    // ������ ���̸�ŭ �̵��ϸ鼭 ������ �˻�
    for( i = pstList->iCurrentPosition ; i < pstList->iLength ; i++ )
    {
        if( pstList->pcBuffer[ i ] == ' ' )
        {
            break;
        }
    }
    
    // �Ķ���͸� �����ϰ� ���̸� ��ȯ
    kMemCpy( pcParameter, pstList->pcBuffer + pstList->iCurrentPosition, i );
    iLength = i - pstList->iCurrentPosition;
    pcParameter[ iLength ] = '\0';

    // �Ķ������ ��ġ ������Ʈ
    pstList->iCurrentPosition += iLength + 1;
    return iLength;
}
    
//==============================================================================
//  Ŀ�ǵ带 ó���ϴ� �ڵ�
//==============================================================================
/**
 *  �� ������ ���
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

    // ���� �� Ŀ�ǵ��� ���̸� ���
    for( i = 0 ; i < iCount ; i++ )
    {
        iLength = kStrLen( gs_vstCommandTable[ i ].pcCommand );
        if( iLength > iMaxCommandLength )
        {
            iMaxCommandLength = iLength;
        }
    }
    
    // ���� ���
    for( i = 0 ; i < iCount ; i++ )
    {
        kPrintf( "%s", gs_vstCommandTable[ i ].pcCommand );
        kGetCursor( &iCursorX, &iCursorY );
        kSetCursor( iMaxCommandLength, iCursorY );
        kPrintf( "  - %s\n", gs_vstCommandTable[ i ].pcHelp );
    }
}

/**
 *  ȭ���� ���� 
 */
static void kCls( const char* pcParameterBuffer )
{
    // �� ������ ����� ������ ����ϹǷ� ȭ���� ���� ��, ���� 1�� Ŀ�� �̵�
    kClearScreen();
    kSetCursor( 0, 1 );
}

/**
 *  �� �޸� ũ�⸦ ���
 */
static void kShowTotalRAMSize( const char* pcParameterBuffer )
{
    kPrintf( "Total RAM Size = %d MB\n", kGetTotalRAMSize() );
}

/**
 *  ���ڿ��� �� ���ڸ� ���ڷ� ��ȯ�Ͽ� ȭ�鿡 ���
 */
static void kStringToDecimalHexTest( const char* pcParameterBuffer )
{
    char vcParameter[ 100 ];
    int iLength;
    PARAMETERLIST stList;
    int iCount = 0;
    long lValue;
    
    // �Ķ���� �ʱ�ȭ
    kInitializeParameter( &stList, pcParameterBuffer );
    
    while( 1 )
    {
        // ���� �Ķ���͸� ����, �Ķ������ ���̰� 0�̸� �Ķ���Ͱ� ���� ���̹Ƿ�
        // ����
        iLength = kGetNextParameter( &stList, vcParameter );
        if( iLength == 0 )
        {
            break;
        }

        // �Ķ���Ϳ� ���� ������ ����ϰ� 16�������� 10�������� �Ǵ��Ͽ� ��ȯ�� ��
        // ����� printf�� ���
        kPrintf( "Param %d = '%s', Length = %d, ", iCount + 1, 
                 vcParameter, iLength );

        // 0x�� �����ϸ� 16����, �׿ܴ� 10������ �Ǵ�
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
 *  PC�� �����(Reboot)
 */
static void kShutdown( const char* pcParamegerBuffer )
{
    kPrintf( "System Shutdown Start...\n" );
    
    // Ű���� ��Ʈ�ѷ��� ���� PC�� �����
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
	//���� ����� align_to�� ����� �ݿø��Ͽ� ����
	//���� �Ҵ�� �޸� ���� sizeũ�⸸ŭ�� �޸𸮿� ũ�⸦ ��Ÿ���� int������ �߰��� ������ �Ѵ�.
    size = (size + sizeof(int) + (align_to - 1)) & ~ (align_to - 1);
    //block = ���� kMalloc �� ����
    free_block* block = free_block_list_head.next;
    //head = �� ���� ����Ʈ ����� next ������ ���� ������
    free_block** head = &(free_block_list_head.next);
    //���� ���� kMalloc �� ������ �̹� �Ҵ�Ǿ� �ִٸ�
    while (block != 0) {
    		//���� �Ҵ�� �� ������ sizeũ���� �޸𸮸� �Ҵ��� �� �ִٸ� �Ҵ��Ѵ�.
        if (block->size >= size) {
        		//�Ҵ��� ��带 �� ���� ����Ʈ���� ����
            *head = block->next;
            	//�Ҵ�� �� ���� ��ȯ
            	//�����ʹ� next������ ������ �����.
            return ((char*)block) + sizeof(int);
        }
        	//���� �� �������� �̵�
        head = &(block->next);
        block = block->next;
    }

    //�����͸� ������ �� �ִ� �Ҵ�� �� ������ ���ٸ� �� ������ size ũ�⸸ŭ �Ҵ��Ѵ�.
    block = (free_block*)(curMallocPos);
    block->size = size - sizeof(int);
    curMallocPos += size;

    return ((char*)block) + sizeof(int);
}

static void kFree(void* ptr) {
    free_block* block = (free_block*)(((char*)ptr) - sizeof(int));
    kMemSet((char*)block + sizeof(int), 0, block->size);
    //kFree�� ���� �� ���� ����Ʈ�� �� �տ� �߰�
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

//��ɾ �ִ��� �ڵ��Է��ϴ� �Լ�(most specific �κи�ɾ� ã��)
//kTrieFind�Լ��� ���ϰ��� ���ڷ� �޴´�.
//1. count�� 2 �̻��� ��, next�� count�� ���� count���� �۾����� most specific
//2. count�� 1�� ��, finish �������� ���ڿ��� most specific
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

//trie�� ��� �ĺ� ��ɾ���� ����Ѵ�.
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
 *  PIT ��Ʈ�ѷ��� ī���� 0 ����
 */
static void kSetTimer( const char* pcParameterBuffer )
{
    char vcParameter[ 100 ];
    PARAMETERLIST stList;
    long lValue;
    BOOL bPeriodic;

    // �Ķ���� �ʱ�ȭ
    kInitializeParameter( &stList, pcParameterBuffer );

    // milisecond ����
    if( kGetNextParameter( &stList, vcParameter ) == 0 )
    {
        kPrintf( "ex)settimer 10(ms) 1(periodic)\n" );
        return ;
    }
    lValue = kAToI( vcParameter, 10 );

    // Periodic ����
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
 *  PIT ��Ʈ�ѷ��� ���� ����Ͽ� ms ���� ���
 */
static void kWaitUsingPIT( const char* pcParameterBuffer )
{
    char vcParameter[ 100 ];
    int iLength;
    PARAMETERLIST stList;
    long lMillisecond;
    int i;

    // �Ķ���� �ʱ�ȭ
    kInitializeParameter( &stList, pcParameterBuffer );
    if( kGetNextParameter( &stList, vcParameter ) == 0 )
    {
        kPrintf( "ex)wait 100(ms)\n" );
        return ;
    }

    lMillisecond = kAToI( pcParameterBuffer, 10 );
    kPrintf( "%d ms Sleep Start...\n", lMillisecond );

    // ���ͷ�Ʈ�� ��Ȱ��ȭ�ϰ� PIT ��Ʈ�ѷ��� ���� ���� �ð��� ����
    kDisableInterrupt();
    for( i = 0 ; i < lMillisecond / 30 ; i++ )
    {
        kWaitUsingDirectPIT( MSTOCOUNT( 30 ) );
    }
    kWaitUsingDirectPIT( MSTOCOUNT( lMillisecond % 30 ) );
    kEnableInterrupt();
    kPrintf( "%d ms Sleep Complete\n", lMillisecond );

    // Ÿ�̸� ����
    kInitializePIT( MSTOCOUNT( 1 ), TRUE );
}

/**
 *  Ÿ�� ������ ī���͸� ����
 */
static void kReadTimeStampCounter( const char* pcParameterBuffer )
{
    QWORD qwTSC;

    qwTSC = kReadTSC();
    kPrintf( "Time Stamp Counter = %q\n", qwTSC );
}

/**
 *  ���μ����� �ӵ��� ����
 */
static void kMeasureProcessorSpeed( const char* pcParameterBuffer )
{
    int i;
    QWORD qwLastTSC, qwTotalTSC = 0;

    kPrintf( "Now Measuring." );

    // 10�� ���� ��ȭ�� Ÿ�� ������ ī���͸� �̿��Ͽ� ���μ����� �ӵ��� ���������� ����
    kDisableInterrupt();
    for( i = 0 ; i < 200 ; i++ )
    {
        qwLastTSC = kReadTSC();
        kWaitUsingDirectPIT( MSTOCOUNT( 50 ) );
        qwTotalTSC += kReadTSC() - qwLastTSC;

        kPrintf( "." );
    }
    // Ÿ�̸� ����
    kInitializePIT( MSTOCOUNT( 1 ), TRUE );
    kEnableInterrupt();

    kPrintf( "\nCPU Speed = %d MHz\n", qwTotalTSC / 10 / 1000 / 1000 );
}

/**
 *  RTC ��Ʈ�ѷ��� ����� ���� �� �ð� ������ ǥ��
 */
static void kShowDateAndTime( const char* pcParameterBuffer )
{
    BYTE bSecond, bMinute, bHour;
    BYTE bDayOfWeek, bDayOfMonth, bMonth;
    WORD wYear;

    // RTC ��Ʈ�ѷ����� �ð� �� ���ڸ� ����
    kReadRTCTime( &bHour, &bMinute, &bSecond );
    kReadRTCDate( &wYear, &bMonth, &bDayOfMonth, &bDayOfWeek );

    kPrintf( "Date: %d/%d/%d %s, ", wYear, bMonth, bDayOfMonth,
             kConvertDayOfWeekToString( bDayOfWeek ) );
    kPrintf( "Time: %d:%d:%d\n", bHour, bMinute, bSecond );
}

// TCB �ڷᱸ���� ���� ����
static TCB gs_vstTask[ 2 ] = { 0, };
static QWORD gs_vstStack[ 1024 ] = { 0, };

/**
 *  �½�ũ ��ȯ�� �׽�Ʈ�ϴ� �½�ũ
 */
static void kTestTask( void )
{
    int i = 0;

    while( 1 )
    {
        // �޽����� ����ϰ� Ű �Է��� ���
        kPrintf( "[%d] This message is from kTestTask. Press any key to switch "
                 "kConsoleShell~!!\n", i++ );
        kGetCh();

        // ������ Ű�� �ԷµǸ� �½�ũ�� ��ȯ
        kSwitchContext( &( gs_vstTask[ 1 ].stContext ), &( gs_vstTask[ 0 ].stContext ) );
    }
}

/**
 *  �½�ũ 1
 *      ȭ�� �׵θ��� ���鼭 ���ڸ� ���
 */
static void kTestTask1( void )
{
    BYTE bData;
    int i = 0, iX = 0, iY = 0, iMargin, j;
    CHARACTER* pstScreen = ( CHARACTER* ) CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;

    // �ڽ��� ID�� �� ȭ�� ���������� ���
    pstRunningTask = kGetRunningTask();
    iMargin = ( pstRunningTask->stLink.qwID & 0xFFFFFFFF ) % 10;

    // ȭ�� �� �����̸� ���鼭 ���� ���
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

        // ���� �� ���� ����
        pstScreen[ iY * CONSOLE_WIDTH + iX ].bCharactor = bData;
        pstScreen[ iY * CONSOLE_WIDTH + iX ].bAttribute = bData & 0x0F;
        bData++;

        // �ٸ� �½�ũ�� ��ȯ
       //kSchedule();
    }
   kExitTask();
}

/**
 *  �½�ũ 2
 *      �ڽ��� ID�� �����Ͽ� Ư�� ��ġ�� ȸ���ϴ� �ٶ����� ���
 */
static void kTestTask2( void )
{
    int i = 0, iOffset;
    CHARACTER* pstScreen = ( CHARACTER* ) CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;
    char vcData[ 4 ] = { '-', '\\', '|', '/' };

    // �ڽ��� ID�� �� ȭ�� ���������� ���
    pstRunningTask = kGetRunningTask();
    iOffset = ( pstRunningTask->stLink.qwID & 0xFFFFFFFF ) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT -
        ( iOffset % ( CONSOLE_WIDTH * CONSOLE_HEIGHT ) );

    while( 1 )
    {
        // ȸ���ϴ� �ٶ����� ǥ��
        pstScreen[ iOffset ].bCharactor = vcData[ i % 4 ];
        // ���� ����
        pstScreen[ iOffset ].bAttribute = ( iOffset % 15 ) + 1;
        i++;

        // �ٸ� �½�ũ�� ��ȯ
        kSchedule();
    }
}

/**
 *  �½�ũ�� �����ؼ� ��Ƽ �½�ŷ ����
 */
void kCreateTestTask( const char* pcParameterBuffer )
{
    PARAMETERLIST stList;
    char vcType[ 30 ];
    char vcCount[ 30 ];
    int i;

    // �Ķ���͸� ����
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcType );
    kGetNextParameter( &stList, vcCount );

    switch( kAToI( vcType, 10 ) )
    {
    // Ÿ�� 1 �½�ũ ����
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

    // Ÿ�� 2 �½�ũ ����
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
 *  �½�ũ�� �켱 ������ ����
 */
static void kChangeTaskPriority( const char* pcParameterBuffer )
{
    PARAMETERLIST stList;
    char vcID[ 30 ];
    char vcPriority[ 30 ];
    QWORD qwID;
    BYTE bPriority;

    // �Ķ���͸� ����
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcID );
    kGetNextParameter( &stList, vcPriority );

    // �½�ũ�� �켱 ������ ����
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
 *  ���� ������ ��� �½�ũ�� ������ ���
 */
static void kShowTaskList( const char* pcParameterBuffer )
{
    int i;
    TCB* pstTCB;
    int iCount = 0;

    kPrintf( "=========== Task Total Count [%d] ===========\n", kGetTaskCount() );
    for( i = 0 ; i < TASK_MAXCOUNT ; i++ )
    {
        // TCB�� ���ؼ� TCB�� ��� ���̸� ID�� ���
        pstTCB = kGetTCBInTCBPool( i );
        if( ( pstTCB->stLink.qwID >> 32 ) != 0 )
        {
            // �½�ũ�� 10�� ��µ� ������, ��� �½�ũ ������ ǥ������ ���θ� Ȯ��
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
 *  �½�ũ�� ����
 */
static void kKillTask( const char* pcParameterBuffer )
{
    PARAMETERLIST stList;
    char vcID[ 30 ];
    QWORD qwID;
    TCB* pstTCB;
    int i;

    // �Ķ���͸� ����
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcID );

    // �½�ũ�� ����
    if( kMemCmp( vcID, "0x", 2 ) == 0 )
    {
        qwID = kAToI( vcID + 2, 16 );
    }
    else
    {
        qwID = kAToI( vcID, 10 );
    }

    // Ư�� ID�� �����ϴ� ���
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
	// �ܼ� �а� ���� �½�ũ�� �����ϰ� ��� �½�ũ ����
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
 *  ���μ����� ������ ǥ��
 */
static void kCPULoad( const char* pcParameterBuffer )
{
    kPrintf( "Processor Load : %d%%\n", kGetProcessorLoad() );
}

// ���ؽ� �׽�Ʈ�� ���ؽ��� ����
static MUTEX gs_stMutex;
static volatile QWORD gs_qwAdder;

/**
 *  ���ؽ��� �׽�Ʈ�ϴ� �½�ũ
 */
static void kPrintNumberTask( void )
{
    int i;
    int j;
    QWORD qwTickCount;

    // 50ms ���� ����Ͽ� �ܼ� ���� ����ϴ� �޽����� ��ġ�� �ʵ��� ��
    qwTickCount = kGetTickCount();
    while( ( kGetTickCount() - qwTickCount ) < 50 )
    {
        kSchedule();
    }

    // ������ ���鼭 ���ڸ� ���
    for( i = 0 ; i < 5 ; i++ )
    {
        kLock( &( gs_stMutex ) );
        kPrintf( "Task ID [0x%Q] Value[%d]\n", kGetRunningTask()->stLink.qwID,
                gs_qwAdder );

        gs_qwAdder += 1;
        kUnlock( & ( gs_stMutex ) );

        // ���μ��� �Ҹ� �ø����� �߰��� �ڵ�
        for( j = 0 ; j < 30000 ; j++ ) ;
    }

    // ��� �½�ũ�� ������ ������ 1��(100ms) ���� ���
    qwTickCount = kGetTickCount();
    while( ( kGetTickCount() - qwTickCount ) < 1000 )
    {
        kSchedule();
    }

    // �½�ũ ����
    kExitTask();
}

/**
 *  ���ؽ��� �׽�Ʈ�ϴ� �½�ũ ����
 */
static void kTestMutex( const char* pcParameterBuffer )
{
    int i;

    gs_qwAdder = 1;

    // ���ؽ� �ʱ�ȭ
    kInitializeMutex( &gs_stMutex );

    for( i = 0 ; i < 3 ; i++ )
    {
        // ���ؽ��� �׽�Ʈ�ϴ� �½�ũ�� 3�� ����
        kCreateTask( TASK_FLAGS_LOW, ( QWORD ) kPrintNumberTask );
    }
    kPrintf( "Wait Util %d Task End...\n", i );
    kGetCh();
}

void kTraceScheduler(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcCount[ 30 ];

    // �Ķ���͸� ����
    kInitializeParameter( &stList, pcParameterBuffer );
    kGetNextParameter( &stList, vcCount );

	trace_task_sequence = kAToI(vcCount, 10);
	while(trace_task_sequence > 0)
	{
		kSchedule();
	}
}
