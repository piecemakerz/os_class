#include "ConsoleShell.h"
#include "Console.h"
#include "Keyboard.h"
#include "Utility.h"

// Ŀ�ǵ� ���̺� ����
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

    kMemSet(lineClear, ' ', 79);
    lineClear[79] = '\0';

    curMallocPos = 0x1436F0;
    head = kMalloc(sizeof(Trie));
    kTrieInitialize(head);

    for(int i=0; i<9; i++)
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
            		bKey = ' ';

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
							kTrieFindMostSpecific(resultTrie, vcCommandBuffer, &iCommandBufferIndex);
							kGetCursor( &iCursorX, &iCursorY );
							kSetCursor( 0, iCursorY );
							vcCommandBuffer[iCommandBufferIndex] = '\0';
							kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE );
							kPrintf("%s", vcCommandBuffer);
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
void kCls( const char* pcParameterBuffer )
{
    // �� ������ ����� ������ ����ϹǷ� ȭ���� ���� ��, ���� 1�� Ŀ�� �̵�
    kClearScreen();
    kSetCursor( 0, 1 );
}

/**
 *  �� �޸� ũ�⸦ ���
 */
void kShowTotalRAMSize( const char* pcParameterBuffer )
{
    kPrintf( "Total RAM Size = %d MB\n", kGetTotalRAMSize() );
}

/**
 *  ���ڿ��� �� ���ڸ� ���ڷ� ��ȯ�Ͽ� ȭ�鿡 ���
 */
void kStringToDecimalHexTest( const char* pcParameterBuffer )
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
void kShutdown( const char* pcParamegerBuffer )
{
    kPrintf( "System Shutdown Start...\n" );
    
    // Ű���� ��Ʈ�ѷ��� ���� PC�� �����
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

void kFree(void* ptr) {
    free_block* block = (free_block*)(((char*)ptr) - sizeof(int));
    kMemSet((char*)block + sizeof(int), 0, block->size);
    //kFree�� ���� �� ���� ����Ʈ�� �� �տ� �߰�
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

//��ɾ �ִ��� �ڵ��Է��ϴ� �Լ�(most specific �κи�ɾ� ã��)
//kTrieFind�Լ��� ���ϰ��� ���ڷ� �޴´�.
//1. count�� 2 �̻��� ��, next�� count�� ���� count���� �۾����� most specific
//2. count�� 1�� ��, finish �������� ���ڿ��� most specific
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

//trie�� ��� �ĺ� ��ɾ���� ����Ѵ�.
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

