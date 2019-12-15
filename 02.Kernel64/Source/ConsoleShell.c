#include "ConsoleShell.h"
#include "Console.h"
#include "Keyboard.h"
#include "Utility.h"
#include "../../01.Kernel32/Source/Page.h"
#include "PIT.h"
#include "RTC.h"
#include "AssemblyUtility.h"
#include "Synchronization.h"
#include "DynamicMemory.h"
#include "HardDisk.h"
#include "FileSystem.h"

static int bTry = 0;

// for print current path
char tWorkingDir[FILESYSTEM_MAXDIRECTORYENTRYCOUNT][FILESYSTEM_MAXFILENAMELENGTH];
char workingDir[FILESYSTEM_MAXDIRECTORYENTRYCOUNT][FILESYSTEM_MAXFILENAMELENGTH];
int tWorkingDirIndex;
int workingDirIndex;

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
        { "testthread", "Test Thread And Process Function", kTestThread },
        { "showmatrix", "Show Matrix Screen", kShowMatrix },
        { "tracescheduler", "Trace Task Scheduling", kTraceScheduler },
        { "showfair", "Show Fairness Graph", kShowFairness },
        { "testpie", "Test PIE Calculation", kTestPIE },
        { "dynamicmeminfo", "Show Dyanmic Memory Information", kShowDyanmicMemoryInformation },
        { "testseqalloc", "Test Sequential Allocation & Free", kTestSequentialAllocation },
        { "testranalloc", "Test Random Allocation & Free", kTestRandomAllocation },
        { "hddinfo", "Show HDD Information", kShowHDDInformation },
        { "readsector", "Read HDD Sector, ex)readsector 0(LBA) 10(count)", kReadSector },
        { "writesector", "Write HDD Sector, ex)writesector 0(LBA) 10(count)", kWriteSector },
        { "mounthdd", "Mount HDD", kMountHDD },
        { "formathdd", "Format HDD", kFormatHDD },
        { "filesysteminfo", "Show File System Information", kShowFileSystemInformation },
        { "createfile", "Create File, ex)createfile a.txt", kCreateFileInDirectory },
        { "deletefile", "Delete File, ex)deletefile a.txt", kDeleteFileInDirectory },
        { "makedir", "Create Directory, ex)createdir a", kCreateDirectoryInDirectory },
        { "rmdir", "Delete Directory, ex)deletedir a", kDeleteDirectoryInDirectory },
        { "dir", "Show Directory", kShowDirectory },
        { "writefile", "Write Data To File, ex) writefile a.txt", kWriteDataToFile },
        { "readfile", "Read Data From File, ex) readfile a.txt", kReadDataFromFile },
        { "testperformance", "Test File Read/WritePerformance", kTestPerformance },
        { "flush", "Flush File System Cache", kFlushCache },
        { "cd", "Change Directory", kChangeDirectoryInDirectory },
};

//==============================================================================
//  ���� ���� �����ϴ� �ڵ�
//==============================================================================
/**
 *  ���� ���� ����
 */
void kStartConsoleShell(void)
{
    char vcCommandBuffer[CONSOLESHELL_MAXCOMMANDBUFFERCOUNT];
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

    // for print current path
    workingDirIndex = 0;

    kMemSet(lineClear, ' ', 79);
    lineClear[79] = '\0';

    curMallocPos = 0x1436F0;
    head = kMalloc(sizeof(Trie));
    kTrieInitialize(head);

    for (int i = 0; i < 34; i++)
    {
        kTrieInsert(head, gs_vstCommandTable[i].pcCommand);
    }

    // ������Ʈ ���
    kPrintf(CONSOLESHELL_PROMPTMESSAGE);
    printCurPath();
    while (1)
    {
        // Ű�� ���ŵ� ������ ���
        bKey = kGetCh();
        // Backspace Ű ó��
        if (bKey == KEY_BACKSPACE)
        {
            if (iCommandBufferIndex > 0)
            {
                // ���� Ŀ�� ��ġ�� �� �� ���� ������ �̵��� ���� ������ ����ϰ�
                // Ŀ�ǵ� ���ۿ��� ������ ���� ����
                kGetCursor(&iCursorX, &iCursorY);
                kPrintStringXY(iCursorX - 1, iCursorY, " ");
                kSetCursor(iCursorX - 1, iCursorY);
                iCommandBufferIndex--;
            }
        }
        // ���� Ű ó��
        else if (bKey == KEY_ENTER)
        {
            kPrintf("\n");

            if (iCommandBufferIndex > 0)
            {
                // Ŀ�ǵ� ���ۿ� �ִ� ����� ����
                vcCommandBuffer[iCommandBufferIndex++] = '\0';
                if (historyCount >= 10)
                {
                    for (int i = 0; i < historyCount - 1; i++)
                    {
                        kMemSet(historyBuffer + i, 0, CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
                        kMemCpy(historyBuffer + i, historyBuffer + (i + 1), CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
                    }
                    historyCount--;
                }
                kMemCpy(historyBuffer + historyCount, vcCommandBuffer, iCommandBufferIndex);
                historyCount++;
                kExecuteCommand(vcCommandBuffer);
            }

            // ������Ʈ ��� �� Ŀ�ǵ� ���� �ʱ�ȭ
            kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE);
            printCurPath();
            kMemSet(vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
            iCommandBufferIndex = 0;
        }
        // ����Ʈ Ű, CAPS Lock, NUM Lock, Scroll Lock�� ����
        else if ((bKey == KEY_LSHIFT) || (bKey == KEY_RSHIFT) ||
            (bKey == KEY_CAPSLOCK) || (bKey == KEY_NUMLOCK) ||
            (bKey == KEY_SCROLLLOCK))
        {
            ;
        }
        else if (bKey == KEY_UP)
        {
            candidateExists = FALSE;

            if (historyCount == 0 || historyIdx == 0)
                continue;

            if (historyIdx == -1)
                historyIdx = historyCount;

            historyIdx--;
            kHistoryPrint(vcCommandBuffer, historyBuffer, lineClear, &iCommandBufferIndex, historyIdx);
            continue;
        }
        else if (bKey == KEY_DOWN)
        {
            candidateExists = FALSE;

            if (historyIdx == -1 || historyIdx == historyCount)
                continue;

            historyIdx++;
            kHistoryPrint(vcCommandBuffer, historyBuffer, lineClear, &iCommandBufferIndex, historyIdx);
            continue;
        }
        else
        {
            // TAB�� �������� ��ȯ
            if (bKey == KEY_TAB)
            {
                if (iCommandBufferIndex == 0)
                    bKey == ' ';

                else
                {
                    if (!candidateExists)
                    {
                        vcCommandBuffer[iCommandBufferIndex] = '\0';
                        resultTrie = kTrieFind(head, vcCommandBuffer);
                        if (resultTrie == NULL)
                            ;
                        else
                        {
                            candidateExists = TRUE;
                            resultTrie = kTrieFindMostSpecific(resultTrie, vcCommandBuffer, &iCommandBufferIndex);
                            kGetCursor(&iCursorX, &iCursorY);
                            kSetCursor(0, iCursorY);
                            vcCommandBuffer[iCommandBufferIndex] = '\0';
                            kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE);
                            printCurPath();
                            kPrintf("%s", vcCommandBuffer);

                            historyIdx = -1;
                            continue;
                        }
                    }
                    else
                    {
                        if (resultTrie->finish == FALSE)
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

                            kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE);
                            printCurPath();
                            kMemSet(vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
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
            if (iCommandBufferIndex < CONSOLESHELL_MAXCOMMANDBUFFERCOUNT)
            {
                vcCommandBuffer[iCommandBufferIndex++] = bKey;
                kPrintf("%c", bKey);
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
void kExecuteCommand(const char* pcCommandBuffer)
{
    int i, iSpaceIndex;
    int iCommandBufferLength, iCommandLength;
    int iCount;

    // �������� ���е� Ŀ�ǵ带 ����
    iCommandBufferLength = kStrLen(pcCommandBuffer);
    for (iSpaceIndex = 0; iSpaceIndex < iCommandBufferLength; iSpaceIndex++)
    {
        if (pcCommandBuffer[iSpaceIndex] == ' ')
        {
            break;
        }
    }

    // Ŀ�ǵ� ���̺��� �˻��ؼ� ������ �̸��� Ŀ�ǵ尡 �ִ��� Ȯ��
    iCount = sizeof(gs_vstCommandTable) / sizeof(SHELLCOMMANDENTRY);
    for (i = 0; i < iCount; i++)
    {
        iCommandLength = kStrLen(gs_vstCommandTable[i].pcCommand);
        // Ŀ�ǵ��� ���̿� ������ ������ ��ġ�ϴ��� �˻�
        if ((iCommandLength == iSpaceIndex) &&
            (kMemCmp(gs_vstCommandTable[i].pcCommand, pcCommandBuffer,
                iSpaceIndex) == 0))
        {
            gs_vstCommandTable[i].pfFunction(pcCommandBuffer + iSpaceIndex + 1);
            break;
        }
    }

    // ����Ʈ���� ã�� �� ���ٸ� ���� ���
    if (i >= iCount)
    {
        kPrintf("'%s' is not found.\n", pcCommandBuffer);
    }
}

/**
 *  �Ķ���� �ڷᱸ���� �ʱ�ȭ
 */
void kInitializeParameter(PARAMETERLIST* pstList, const char* pcParameter)
{
    pstList->pcBuffer = pcParameter;
    pstList->iLength = kStrLen(pcParameter);
    pstList->iCurrentPosition = 0;
}

/**
 *  �������� ���е� �Ķ������ ����� ���̸� ��ȯ
 */
int kGetNextParameter(PARAMETERLIST* pstList, char* pcParameter)
{
    int i;
    int iLength;

    // �� �̻� �Ķ���Ͱ� ������ ����
    if (pstList->iLength <= pstList->iCurrentPosition)
    {
        return 0;
    }

    // ������ ���̸�ŭ �̵��ϸ鼭 ������ �˻�
    for (i = pstList->iCurrentPosition; i < pstList->iLength; i++)
    {
        if (pstList->pcBuffer[i] == ' ')
        {
            break;
        }
    }

    // �Ķ���͸� �����ϰ� ���̸� ��ȯ
    kMemCpy(pcParameter, pstList->pcBuffer + pstList->iCurrentPosition, i);
    iLength = i - pstList->iCurrentPosition;
    pcParameter[iLength] = '\0';

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
static void kHelp(const char* pcCommandBuffer)
{
    int i;
    int iCount;
    int iCursorX, iCursorY;
    int iLength, iMaxCommandLength = 0;


    kPrintf("=========================================================\n");
    kPrintf("                    MINT64 Shell Help                    \n");
    kPrintf("=========================================================\n");

    iCount = sizeof(gs_vstCommandTable) / sizeof(SHELLCOMMANDENTRY);

    // ���� �� Ŀ�ǵ��� ���̸� ���
    for (i = 0; i < iCount; i++)
    {
        iLength = kStrLen(gs_vstCommandTable[i].pcCommand);
        if (iLength > iMaxCommandLength)
        {
            iMaxCommandLength = iLength;
        }
    }

    // ���� ���
    for (i = 0; i < iCount; i++)
    {
        kPrintf("%s", gs_vstCommandTable[i].pcCommand);
        kGetCursor(&iCursorX, &iCursorY);
        kSetCursor(iMaxCommandLength, iCursorY);
        kPrintf("  - %s\n", gs_vstCommandTable[i].pcHelp);
    }
}

/**
 *  ȭ���� ����
 */
static void kCls(const char* pcParameterBuffer)
{
    // �� ������ ����� ������ ����ϹǷ� ȭ���� ���� ��, ���� 1�� Ŀ�� �̵�
    kClearScreen();
    kSetCursor(0, 1);
}

/**
 *  �� �޸� ũ�⸦ ���
 */
static void kShowTotalRAMSize(const char* pcParameterBuffer)
{
    kPrintf("Total RAM Size = %d MB\n", kGetTotalRAMSize());
}

/**
 *  ���ڿ��� �� ���ڸ� ���ڷ� ��ȯ�Ͽ� ȭ�鿡 ���
 */
static void kStringToDecimalHexTest(const char* pcParameterBuffer)
{
    char vcParameter[100];
    int iLength;
    PARAMETERLIST stList;
    int iCount = 0;
    long lValue;

    // �Ķ���� �ʱ�ȭ
    kInitializeParameter(&stList, pcParameterBuffer);

    while (1)
    {
        // ���� �Ķ���͸� ����, �Ķ������ ���̰� 0�̸� �Ķ���Ͱ� ���� ���̹Ƿ�
        // ����
        iLength = kGetNextParameter(&stList, vcParameter);
        if (iLength == 0)
        {
            break;
        }

        // �Ķ���Ϳ� ���� ������ ����ϰ� 16�������� 10�������� �Ǵ��Ͽ� ��ȯ�� ��
        // ����� printf�� ���
        kPrintf("Param %d = '%s', Length = %d, ", iCount + 1,
            vcParameter, iLength);

        // 0x�� �����ϸ� 16����, �׿ܴ� 10������ �Ǵ�
        if (kMemCmp(vcParameter, "0x", 2) == 0)
        {
            lValue = kAToI(vcParameter + 2, 16);
            kPrintf("HEX Value = %q\n", lValue);
        }
        else
        {
            lValue = kAToI(vcParameter, 10);
            kPrintf("Decimal Value = %d\n", lValue);
        }

        iCount++;
    }
}

/**
 *  PC�� �����(Reboot)
 */
static void kShutdown(const char* pcParamegerBuffer)
{
    kPrintf("System Shutdown Start...\n");

    // ���� �ý��� ĳ�ÿ� ��� �ִ� ������ �ϵ� ��ũ�� �ű�
    kPrintf("Cache Flush ... ");
    if (kFlushFileSystemCache() == TRUE)
    {
        kPrintf("Pass\n");
    }
    else
    {
        kPrintf("Fail\n");
    }

    // Ű���� ��Ʈ�ѷ��� ���� PC�� �����
    kPrintf("Press Any Key To Reboot PC...");
    kGetCh();
    kReboot();
}

static inline void invlpg(void* m)
{
    asm volatile ("invlpg (%0)" : : "b"(m) : "memory");
}

static void kRaiseFault(const char* pcParamegerBuffer)
{
    // PTENTRY* pstPTEntry = ( PTENTRY* ) 0x142000;
    // PTENTRY* pstEntry = &(pstPTEntry[511]);
    // DWORD* pstPage = 0x1ff000;
    long* ptr = 0x1ff000;
    DWORD data = 0x99;
    if (bTry == 0) {
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
    kMemSet(vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
    kMemCpy(vcCommandBuffer, historyBuffer[historyIdx], kStrLen(historyBuffer[historyIdx]));
    *iCommandBufferIndex = kStrLen(historyBuffer[historyIdx]);

    kGetCursor(&iCursorX, &iCursorY);
    kSetCursor(0, iCursorY);
    kPrintf("%s", lineClear);
    kSetCursor(0, iCursorY);
    kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE);
    printCurPath();
    kPrintf("%s", vcCommandBuffer);
}

static void* kMalloc(int size) {
    size = (size + sizeof(int) + (align_to - 1)) & ~(align_to - 1);

    free_block* block = free_block_list_head.next;

    free_block** head = &(free_block_list_head.next);

    while (block != 0) {
        if (block->size >= size) {
            *head = block->next;
            return ((char*)block) + sizeof(int);
        }
        head = &(block->next);
        block = block->next;
    }

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

    if (*key == '\0')
        trie->finish = TRUE;
    else
    {
        curr = *key - 'a';
        if (trie->next[curr] == NULL) {
            trie->next[curr] = (Trie*)kMalloc(sizeof(Trie));
            kTrieInitialize(trie->next[curr]);
        }
        kTrieInsert(trie->next[curr], key + 1);
    }
}

static Trie* kTrieFind(Trie* trie, const char* key)
{
    int curr;

    if (*key == '\0')
        return trie;
    curr = *key - 'a';
    if (curr > 26 || curr < 0 || trie->next[curr] == NULL)
        return NULL;
    return kTrieFind(trie->next[curr], key + 1);
}

static Trie* kTrieFindMostSpecific(Trie* trie, char* buffer, int* strIndex)
{
    int startCount = trie->count;
    Trie* nextTrie;
    BOOL mostSpecificFound;

    while (trie->finish == FALSE)
    {
        mostSpecificFound = TRUE;
        for (int i = 0; i < 26; i++)
        {
            nextTrie = trie->next[i];
            if (nextTrie->count == startCount)
            {
                mostSpecificFound = FALSE;
                buffer[(*strIndex)++] = i + 'a';
                trie = nextTrie;
                break;
            }
        }
        if (mostSpecificFound)
            return trie;
    }

    return trie;
}

static void kPrintEveryCandidate(Trie* trie, const char* key, char* resultBuffer, char* tempStr, int* bufferIdx, int* tempStrIdx)
{
    int keyLen;

    if (trie->finish == TRUE)
    {
        keyLen = kStrLen(key);
        kMemCpy(resultBuffer + *bufferIdx, key, keyLen);
        (*bufferIdx) += keyLen;

        kMemCpy(resultBuffer + *bufferIdx, tempStr, *tempStrIdx);
        (*bufferIdx) += (*tempStrIdx);
        resultBuffer[(*bufferIdx)++] = ' ';
        return;
    }

    for (int i = 0; i < 26; i++)
    {
        if (trie->next[i] != NULL)
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
static void kSetTimer(const char* pcParameterBuffer)
{
    char vcParameter[100];
    PARAMETERLIST stList;
    long lValue;
    BOOL bPeriodic;

    // �Ķ���� �ʱ�ȭ
    kInitializeParameter(&stList, pcParameterBuffer);

    // milisecond ����
    if (kGetNextParameter(&stList, vcParameter) == 0)
    {
        kPrintf("ex)settimer 10(ms) 1(periodic)\n");
        return;
    }
    lValue = kAToI(vcParameter, 10);

    // Periodic ����
    if (kGetNextParameter(&stList, vcParameter) == 0)
    {
        kPrintf("ex)settimer 10(ms) 1(periodic)\n");
        return;
    }
    bPeriodic = kAToI(vcParameter, 10);

    kInitializePIT(MSTOCOUNT(lValue), bPeriodic);
    kPrintf("Time = %d ms, Periodic = %d Change Complete\n", lValue, bPeriodic);
}

/**
 *  PIT ��Ʈ�ѷ��� ���� ����Ͽ� ms ���� ���
 */
static void kWaitUsingPIT(const char* pcParameterBuffer)
{
    char vcParameter[100];
    int iLength;
    PARAMETERLIST stList;
    long lMillisecond;
    int i;

    // �Ķ���� �ʱ�ȭ
    kInitializeParameter(&stList, pcParameterBuffer);
    if (kGetNextParameter(&stList, vcParameter) == 0)
    {
        kPrintf("ex)wait 100(ms)\n");
        return;
    }

    lMillisecond = kAToI(pcParameterBuffer, 10);
    kPrintf("%d ms Sleep Start...\n", lMillisecond);

    // ���ͷ�Ʈ�� ��Ȱ��ȭ�ϰ� PIT ��Ʈ�ѷ��� ���� ���� �ð��� ����
    kDisableInterrupt();
    for (i = 0; i < lMillisecond / 30; i++)
    {
        kWaitUsingDirectPIT(MSTOCOUNT(30));
    }
    kWaitUsingDirectPIT(MSTOCOUNT(lMillisecond % 30));
    kEnableInterrupt();
    kPrintf("%d ms Sleep Complete\n", lMillisecond);

    // Ÿ�̸� ����
    kInitializePIT(MSTOCOUNT(1), TRUE);
}

/**
 *  Ÿ�� ������ ī���͸� ����
 */
static void kReadTimeStampCounter(const char* pcParameterBuffer)
{
    QWORD qwTSC;

    qwTSC = kReadTSC();
    kPrintf("Time Stamp Counter = %q\n", qwTSC);
}

/**
 *  ���μ����� �ӵ��� ����
 */
static void kMeasureProcessorSpeed(const char* pcParameterBuffer)
{
    int i;
    QWORD qwLastTSC, qwTotalTSC = 0;

    kPrintf("Now Measuring.");

    // 10�� ���� ��ȭ�� Ÿ�� ������ ī���͸� �̿��Ͽ� ���μ����� �ӵ��� ���������� ����
    kDisableInterrupt();
    for (i = 0; i < 200; i++)
    {
        qwLastTSC = kReadTSC();
        kWaitUsingDirectPIT(MSTOCOUNT(50));
        qwTotalTSC += kReadTSC() - qwLastTSC;

        kPrintf(".");
    }
    // Ÿ�̸� ����
    kInitializePIT(MSTOCOUNT(1), TRUE);
    kEnableInterrupt();

    kPrintf("\nCPU Speed = %d MHz\n", qwTotalTSC / 10 / 1000 / 1000);
}

/**
 *  RTC ��Ʈ�ѷ��� ����� ���� �� �ð� ������ ǥ��
 */
static void kShowDateAndTime(const char* pcParameterBuffer)
{
    BYTE bSecond, bMinute, bHour;
    BYTE bDayOfWeek, bDayOfMonth, bMonth;
    WORD wYear;

    // RTC ��Ʈ�ѷ����� �ð� �� ���ڸ� ����
    kReadRTCTime(&bHour, &bMinute, &bSecond);
    kReadRTCDate(&wYear, &bMonth, &bDayOfMonth, &bDayOfWeek);

    kPrintf("Date: %d/%d/%d %s, ", wYear, bMonth, bDayOfMonth,
        kConvertDayOfWeekToString(bDayOfWeek));
    kPrintf("Time: %d:%d:%d\n", bHour, bMinute, bSecond);
}

static TCB gs_vstTask[2] = { 0, };
static QWORD gs_vstStack[1024] = { 0, };

static void kTestTask(void)
{
    int i = 0;

    while (1)
    {
        kPrintf("[%d] This message is from kTestTask. Press any key to switch "
            "kConsoleShell~!!\n", i++);
        kGetCh();

        kSwitchContext(&(gs_vstTask[1].stContext), &(gs_vstTask[0].stContext));
    }
}

/**
 *  �½�ũ 1
 *      ȭ�� �׵θ��� ���鼭 ���ڸ� ���
 */
static void kTestTask1(void)
{
    BYTE bData;
    int i = 0, iX = 0, iY = 0, iMargin, j;
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;

    // �ڽ��� ID�� �� ȭ�� ���������� ���
    pstRunningTask = kGetRunningTask();
    iMargin = (pstRunningTask->stLink.qwID & 0xFFFFFFFF) % 10;

    // ȭ�� �� �����̸� ���鼭 ���� ���
    for (j = 0; j < 20000; j++)
    {
        switch (i)
        {
        case 0:
            iX++;
            if (iX >= (CONSOLE_WIDTH - iMargin))
            {
                i = 1;
            }
            break;

        case 1:
            iY++;
            if (iY >= (CONSOLE_HEIGHT - iMargin))
            {
                i = 2;
            }
            break;

        case 2:
            iX--;
            if (iX < iMargin)
            {
                i = 3;
            }
            break;

        case 3:
            iY--;
            if (iY < iMargin)
            {
                i = 0;
            }
            break;
        }

        // ���� �� ���� ����
        pstScreen[iY * CONSOLE_WIDTH + iX].bCharactor = bData;
        pstScreen[iY * CONSOLE_WIDTH + iX].bAttribute = bData & 0x0F;
        bData++;

        // �ٸ� �½�ũ�� ��ȯ
        kSchedule();
    }

    kExitTask();
}

/**
 *  �½�ũ 2
 *      �ڽ��� ID�� �����Ͽ� Ư�� ��ġ�� ȸ���ϴ� �ٶ����� ���
 */
static void kTestTask2(void)
{
    int i = 0, iOffset;
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;
    char vcData[4] = { '-', '\\', '|', '/' };

    // �ڽ��� ID�� �� ȭ�� ���������� ���
    pstRunningTask = kGetRunningTask();
    iOffset = (pstRunningTask->stLink.qwID & 0xFFFFFFFF) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT -
        (iOffset % (CONSOLE_WIDTH * CONSOLE_HEIGHT));

    while (1)
    {
        // ȸ���ϴ� �ٶ����� ǥ��
        pstScreen[iOffset].bCharactor = vcData[i % 4];
        // ���� ����
        pstScreen[iOffset].bAttribute = (iOffset % 15) + 1;
        i++;

        // �ٸ� �½�ũ�� ��ȯ
        kSchedule();
    }
}

/**
 *  �½�ũ�� �����ؼ� ��Ƽ �½�ŷ ����
 */
static void kCreateTestTask(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcType[30];
    char vcCount[30];
    int i;

    // �Ķ���͸� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcType);
    kGetNextParameter(&stList, vcCount);

    switch (kAToI(vcType, 10))
    {
        // Ÿ�� 1 �½�ũ ����
    case 1:
        for (i = 0; i < kAToI(vcCount, 10); i++)
        {
            if (kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kTestTask1) == NULL)
            {
                break;
            }
        }

        kPrintf("Task1 %d Created\n", i);
        break;

        // Ÿ�� 2 �½�ũ ����
    case 2:
    default:
        for (i = 0; i < kAToI(vcCount, 10); i++)
        {
            if (kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kTestTask2) == NULL)
            {
                break;
            }
        }
        kPrintf("Task2 %d Created\n", i);
        break;
    }
}

/**
 *  �½�ũ�� �켱 ������ ����
 */
static void kChangeTaskPriority(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcID[30];
    char vcPriority[30];
    QWORD qwID;
    BYTE bPriority;

    // �Ķ���͸� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcID);
    kGetNextParameter(&stList, vcPriority);

    // �½�ũ�� �켱 ������ ����
    if (kMemCmp(vcID, "0x", 2) == 0)
    {
        qwID = kAToI(vcID + 2, 16);
    }
    else
    {
        qwID = kAToI(vcID, 10);
    }

    bPriority = kAToI(vcPriority, 10);

    kPrintf("Change Task Priority ID [0x%q] Priority[%d] ", qwID, bPriority);
    if (kChangePriority(qwID, bPriority) == TRUE)
    {
        kPrintf("Success\n");
    }
    else
    {
        kPrintf("Fail\n");
    }
}

/**
 *  ���� ������ ��� �½�ũ�� ������ ���
 */
static void kShowTaskList(const char* pcParameterBuffer)
{
    int i;
    TCB* pstTCB;
    int iCount = 0;

    kPrintf("=========== Task Total Count [%d] ===========\n", kGetTaskCount());
    for (i = 0; i < TASK_MAXCOUNT; i++)
    {
        // TCB�� ���ؼ� TCB�� ��� ���̸� ID�� ���
        pstTCB = kGetTCBInTCBPool(i);
        if ((pstTCB->stLink.qwID >> 32) != 0)
        {
            // �½�ũ�� 10�� ��µ� ������, ��� �½�ũ ������ ǥ������ ���θ� Ȯ��
            if ((iCount != 0) && ((iCount % 10) == 0))
            {
                kPrintf("Press any key to continue... ('q' is exit) : ");
                if (kGetCh() == 'q')
                {
                    kPrintf("\n");
                    break;
                }
                kPrintf("\n");
            }

            kPrintf("[%d] Task ID[0x%Q], Priority[%d], Flags[0x%Q], Thread[%d]\n", 1 + iCount++,
                pstTCB->stLink.qwID, GETPRIORITY(pstTCB->qwFlags),
                pstTCB->qwFlags, kGetListCount(&(pstTCB->stChildThreadList)));
            kPrintf("    Parent PID[0x%Q], Memory Address[0x%Q], Size[0x%Q]\n",
                pstTCB->qwParentProcessID, pstTCB->pvMemoryAddress, pstTCB->qwMemorySize);
        }
    }
}


/**
 *  �½�ũ�� ����
 */
static void kKillTask(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcID[30];
    QWORD qwID;
    TCB* pstTCB;
    int i;

    // �Ķ���͸� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcID);

    // �½�ũ�� ����
    if (kMemCmp(vcID, "0x", 2) == 0)
    {
        qwID = kAToI(vcID + 2, 16);
    }
    else
    {
        qwID = kAToI(vcID, 10);
    }

    // Ư�� ID�� �����ϴ� ���
    if (qwID != 0xFFFFFFFF)
    {
        pstTCB = kGetTCBInTCBPool(GETTCBOFFSET(qwID));
        qwID = pstTCB->stLink.qwID;

        // �ý��� �׽�Ʈ�� ����
        if (((qwID >> 32) != 0) && ((pstTCB->qwFlags & TASK_FLAGS_SYSTEM) == 0x00))
        {
            kPrintf("Kill Task ID [0x%q] ", qwID);
            if (kEndTask(qwID) == TRUE)
            {
                kPrintf("Success\n");
            }
            else
            {
                kPrintf("Fail\n");
            }
        }
        else
        {
            kPrintf("Task does not exist or task is system task\n");
        }
    }
    // �ܼ� �а� ���� �½�ũ�� �����ϰ� ��� �½�ũ ����
    else
    {
        for (i = 0; i < TASK_MAXCOUNT; i++)
        {
            pstTCB = kGetTCBInTCBPool(i);
            qwID = pstTCB->stLink.qwID;

            // �ý��� �׽�Ʈ�� ���� ��Ͽ��� ����
            if (((qwID >> 32) != 0) && ((pstTCB->qwFlags & TASK_FLAGS_SYSTEM) == 0x00))
            {
                kPrintf("Kill Task ID [0x%q] ", qwID);
                if (kEndTask(qwID) == TRUE)
                {
                    kPrintf("Success\n");
                }
                else
                {
                    kPrintf("Fail\n");
                }
            }
        }
    }
}

/**
 *  ���μ����� ������ ǥ��
 */
static void kCPULoad(const char* pcParameterBuffer)
{
    kPrintf("Processor Load : %d%%\n", kGetProcessorLoad());
}

// ���ؽ� �׽�Ʈ�� ���ؽ��� ����
static MUTEX gs_stMutex;
static volatile QWORD gs_qwAdder;

/**
 *  ���ؽ��� �׽�Ʈ�ϴ� �½�ũ
 */
static void kPrintNumberTask(void)
{
    int i;
    int j;
    QWORD qwTickCount;

    // 50ms ���� ����Ͽ� �ܼ� ���� ����ϴ� �޽����� ��ġ�� �ʵ��� ��
    qwTickCount = kGetTickCount();
    while ((kGetTickCount() - qwTickCount) < 50)
    {
        kSchedule();
    }

    // ������ ���鼭 ���ڸ� ���
    for (i = 0; i < 5; i++)
    {
        kLock(&(gs_stMutex));
        kPrintf("Task ID [0x%Q] Value[%d]\n", kGetRunningTask()->stLink.qwID,
            gs_qwAdder);

        gs_qwAdder += 1;
        kUnlock(&(gs_stMutex));

        // ���μ��� �Ҹ� �ø����� �߰��� �ڵ�
        for (j = 0; j < 30000; j++);
    }

    // ��� �½�ũ�� ������ ������ 1��(100ms) ���� ���
    qwTickCount = kGetTickCount();
    while ((kGetTickCount() - qwTickCount) < 1000)
    {
        kSchedule();
    }

    // �½�ũ ����
    //kExitTask();
}

/**
 *  ���ؽ��� �׽�Ʈ�ϴ� �½�ũ ����
 */
static void kTestMutex(const char* pcParameterBuffer)
{
    int i;

    gs_qwAdder = 1;

    // ���ؽ� �ʱ�ȭ
    kInitializeMutex(&gs_stMutex);

    for (i = 0; i < 3; i++)
    {
        // ���ؽ��� �׽�Ʈ�ϴ� �½�ũ�� 3�� ����
        kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kPrintNumberTask);
    }
    kPrintf("Wait Util %d Task End...\n", i);
    kGetCh();
}

/**
 *  �½�ũ 2�� �ڽ��� ������� �����ϴ� �½�ũ
 */
static void kCreateThreadTask(void)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kTestTask2);
    }

    while (1)
    {
        kSleep(1);
    }
}

/**
 *  �����带 �׽�Ʈ�ϴ� �½�ũ ����
 */
static void kTestThread(const char* pcParameterBuffer)
{
    TCB* pstProcess;

    pstProcess = kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_PROCESS, (void*)0xEEEEEEEE, 0x1000,
        (QWORD)kCreateThreadTask);
    if (pstProcess != NULL)
    {
        kPrintf("Process [0x%Q] Create Success\n", pstProcess->stLink.qwID);
    }
    else
    {
        kPrintf("Process Create Fail\n");
    }
}

// ������ �߻���Ű�� ���� ����
static volatile QWORD gs_qwRandomValue = 0;

/**
 *  ������ ������ ��ȯ
 */
QWORD kRandom(void)
{
    gs_qwRandomValue = (gs_qwRandomValue * 412153 + 5571031) >> 16;
    return gs_qwRandomValue;
}

/**
 *  ö�ڸ� �귯������ �ϴ� ������
 */
static void kDropCharactorThread(void)
{
    int iX, iY;
    int i;
    char vcText[2] = { 0, };

    iX = kRandom() % CONSOLE_WIDTH;

    while (1)
    {
        // ��� �����
        kSleep(kRandom() % 20);

        if ((kRandom() % 20) < 16)
        {
            vcText[0] = ' ';
            for (i = 0; i < CONSOLE_HEIGHT - 1; i++)
            {
                kPrintStringXY(iX, i, vcText);
                kSleep(50);
            }
        }
        else
        {
            for (i = 0; i < CONSOLE_HEIGHT - 1; i++)
            {
                vcText[0] = i + kRandom();
                kPrintStringXY(iX, i, vcText);
                kSleep(50);
            }
        }
    }
}

/**
 *  �����带 �����Ͽ� ��Ʈ���� ȭ��ó�� �����ִ� ���μ���
 */
static void kMatrixProcess(void)
{
    int i;

    for (i = 0; i < 300; i++)
    {
        if (kCreateTask(TASK_FLAGS_THREAD | TASK_FLAGS_LOW, 0, 0,
            (QWORD)kDropCharactorThread) == NULL)
        {
            break;
        }

        kSleep(kRandom() % 5 + 5);
    }

    kPrintf("%d Thread is created\n", i);

    // Ű�� �ԷµǸ� ���μ��� ����
    kGetCh();
}

/**
 *  ��Ʈ���� ȭ���� ������
 */
static void kShowMatrix(const char* pcParameterBuffer)
{
    TCB* pstProcess;

    pstProcess = kCreateTask(TASK_FLAGS_PROCESS | TASK_FLAGS_LOW, (void*)0xE00000, 0xE00000,
        (QWORD)kMatrixProcess);
    if (pstProcess != NULL)
    {
        kPrintf("Matrix Process [0x%Q] Create Success\n");

        // �½�ũ�� ���� �� ������ ���
        while ((pstProcess->stLink.qwID >> 32) != 0)
        {
            kSleep(100);
        }
    }
    else
    {
        kPrintf("Matrix Process Create Fail\n");
    }
}

void kTraceScheduler(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcCount[30];

    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcCount);

    trace_task_sequence = kAToI(vcCount, 10);
    while (trace_task_sequence > 0)
    {
        kSchedule();
    }
}
#define COLOR(i) i%13+2

static void kFairnessGraph() {
    // ��¿� ���Ǵ� ����
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;
    int vmemLabelPos = CONSOLE_WIDTH * 2;
    int vmemGraphPos = CONSOLE_WIDTH * 3;

    // �׷��� ��� ���ۿ� ���õ� ����
    int graphCursor = 0; // ���� ��ü���� �����ؾ� �ϴ� ������ ���� column�ǹ�
    short graph[1600];   // 10��
    short line = 20;     // 10�� (���� �����ϰ� ������ϴ� ����)
    short cells = (line) * 80;//��� ���� ������ #���� �ʱ�ȭ
    for (int i = 0; i < cells; i++) {
        graph[i] = 0;
        pstScreen[vmemGraphPos + i].bCharactor = '#';
        pstScreen[vmemGraphPos + i].bAttribute = 0; // ������
    }

    // ���α׷� ����
    {
        char fairnessgraph[] = "fairness graph";
        for (int i = 0; i < 15; i++) {
            pstScreen[80 + i].bCharactor = fairnessgraph[i];
        }

        char pressanykeytostop[] = "press any key to stop...";
        for (int i = 0; i < 25; i++) {
            pstScreen[80 * 24 + i].bCharactor = pressanykeytostop[i];
        }

        char totalRun[] = "total runs:";
        for (int i = 0; i < 12; i++) {
            pstScreen[80 * 23 + 56 + i].bCharactor = totalRun[i];
        }
    }

    // runningTime ���� ���õ� ���� 
    int tasks[1025]; // ������ 1
    short activetasks[1025];
    short activeCount = 0;
    long totalRunningTime = 0; // �� ���� ����ð�
    TCB* pstTCB;    // TCB ������

    // �۾��ð� �ʱ�ȭ
    for (int i = 0; i < TASK_MAXCOUNT; i++) {
        tasks[i] = -1; // �����ٸ��� ���μ��� ����� ��� ���� ���� ����

        pstTCB = kGetTCBInTCBPool(i);
        if ((pstTCB->stLink.qwID >> 32) != 0) { // �����ٸ��� ���μ��� ��� ȹ��
            pstTCB->qwRunningTime = 0;          // �ð� �ʱ�ȭ
            tasks[i] = 0;
            activetasks[activeCount++] = i;     // active��� ����
        }
    }
    // ��¹����ʱ�ȭ

    // ������ ȿ�� ����ϱ�
    int offset = 0;
    while (1) {
        if (graphCursor == 79) return;
        // �����ٷ��� ���۵Ǹ� �� �̻� ���� ���۵Ǵ� �½�ũ�� �������� ��ĵ �ʿ� ����

        // ���� Ƚ�� ��������
        for (int i = 0; i < activeCount; i++) {
            int taskID = activetasks[i];
            pstTCB = kGetTCBInTCBPool(taskID);
            if ((pstTCB->stLink.qwID >> 32) != 0) { // �����ٸ��� ���μ��� ��� ȹ��
                tasks[taskID] += pstTCB->qwRunningTime;  // �̹� ȸ���� �����ٸ��� Ƚ�� ����
                totalRunningTime += pstTCB->qwRunningTime;
                pstTCB->qwRunningTime = 0;          // Ƚ�� �ʱ�ȭ
            }
        }
        // ��� ���� Ƚ��
        pstScreen[80 * 23 + 70].bCharactor = totalRunningTime >= 100000 ? totalRunningTime / 100000 + '0' : ' ';
        pstScreen[80 * 23 + 71].bCharactor = totalRunningTime >= 10000 ? totalRunningTime / 10000 + '0' : ' ';
        pstScreen[80 * 23 + 72].bCharactor = totalRunningTime >= 1000 ? totalRunningTime / 1000 + '0' : ' ';
        pstScreen[80 * 23 + 73].bCharactor = totalRunningTime >= 100 ? (totalRunningTime % 1000) / 100 + '0' : ' ';
        pstScreen[80 * 23 + 74].bCharactor = totalRunningTime >= 10 ? (totalRunningTime % 100) / 10 + '0' : ' ';
        pstScreen[80 * 23 + 75].bCharactor = totalRunningTime % 10 + '0';
        // ���� Ƚ�� ����ϱ�
        int vmemLabelCursor = vmemLabelPos; // label �����ġ ����
        if (totalRunningTime != 0) {         //
            // ��� �½�ũ�� ���� ���� �ð��� ��ĵ�Ѵ�
            int j = 0;
            for (int i = 0; i < activeCount && j < line; i++) {
                int taskID = activetasks[i];
                int runningTime = tasks[taskID];
                int percentage = ((runningTime * 100) / totalRunningTime); // 100% ������ ���
                if (vmemLabelCursor < 230) {
                    pstScreen[vmemLabelCursor].bCharactor = ' ';
                    pstScreen[vmemLabelCursor + 1].bCharactor = taskID >= 1000 ? taskID / 1000 + '0' : ' ';
                    pstScreen[vmemLabelCursor + 1].bAttribute = COLOR(taskID);
                    pstScreen[vmemLabelCursor + 2].bCharactor = taskID >= 100 ? (taskID % 1000) / 100 + '0' : ' ';
                    pstScreen[vmemLabelCursor + 2].bAttribute = COLOR(taskID);
                    pstScreen[vmemLabelCursor + 3].bCharactor = taskID >= 10 ? (taskID % 100) / 10 + '0' : ' ';
                    pstScreen[vmemLabelCursor + 3].bAttribute = COLOR(taskID);
                    pstScreen[vmemLabelCursor + 4].bCharactor = taskID % 10 + '0';
                    pstScreen[vmemLabelCursor + 4].bAttribute = COLOR(taskID);
                    pstScreen[vmemLabelCursor + 6].bCharactor = percentage >= 10 ? percentage / 10 + '0' : '0';
                    pstScreen[vmemLabelCursor + 6].bAttribute = COLOR(taskID);
                    pstScreen[vmemLabelCursor + 7].bCharactor = percentage > 0 ? percentage % 10 + '0' : '0';
                    pstScreen[vmemLabelCursor + 7].bAttribute = COLOR(taskID);
                    pstScreen[vmemLabelCursor + 9].bCharactor = '%';
                    pstScreen[vmemLabelCursor + 9].bAttribute = COLOR(taskID);
                    vmemLabelCursor += 10;
                }
                else {// ���� 1�ٱ����� ���, �������� ����
                    pstScreen[vmemLabelCursor + 1].bCharactor = '.';
                    pstScreen[vmemLabelCursor + 2].bCharactor = '.';
                    pstScreen[vmemLabelCursor + 3].bCharactor = '.';
                }
                // ��� ���Ǹ� ����, padding�� ����
                percentage = runningTime * line / totalRunningTime; // ���� ���� ����
                for (int k = 0; k < percentage && (k + j) < line; k++) {
                    graph[graphCursor + (k + j) * 80] = COLOR(taskID);
                }
                j += percentage;
            }
            // ���ڶ� ������ ��ĭ���� ä��taskID
            for (; j < line; j++) {
                graph[graphCursor + j * 80] = 0;
            }

            // �׷��� ����ϱ�
            // �̹� ȸ���� �׷��� ������ ���� ������ ���� ����Ű�� Ŀ��
            // int cursor= (graphCursor + 1) % 80;
            graphCursor = (graphCursor + 1) % 80;
            for (int i = 0; i < line; i++)
                for (int j = 0; j < 80; j++) {
                    pstScreen[vmemGraphPos + i * 80 + j].bAttribute = graph[i * 80 + (graphCursor + j) % 80];
                }
            kSchedule();

        }
        if (graphCursor == 79)
            return;
    }
}

static void kShowFairness(void) {
    kClearScreen();
    TCB* pstProcess;
    pstProcess = kCreateTask(TASK_FLAGS_PROCESS | TASK_FLAGS_LOW, (void*)0xE00000, 0xE00000,
        (QWORD)kFairnessGraph);
    kSleep(1000);
    kSetCursor(60, 25);
    kGetCh();
    kEndTask(pstProcess->stLink.qwID);
    return;
}


/**
 *  FPU�� �׽�Ʈ�ϴ� �½�ũ
 */
static void kFPUTestTask(void)
{
    double dValue1;
    double dValue2;
    TCB* pstRunningTask;
    QWORD qwCount = 0;
    QWORD qwRandomValue;
    int i;
    int iOffset;
    char vcData[4] = { '-', '\\', '|', '/' };
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;

    pstRunningTask = kGetRunningTask();

    // �ڽ��� ID�� �� ȭ�� ���������� ���
    iOffset = (pstRunningTask->stLink.qwID & 0xFFFFFFFF) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT -
        (iOffset % (CONSOLE_WIDTH * CONSOLE_HEIGHT));

    // ������ ������ �ݺ��ϸ鼭 ������ ����� ����
    while (1)
    {
        dValue1 = 1;
        dValue2 = 1;

        // �׽�Ʈ�� ���� ������ ����� 2�� �ݺ��ؼ� ����
        for (i = 0; i < 10; i++)
        {
            qwRandomValue = kRandom();
            dValue1 *= (double)qwRandomValue;
            dValue2 *= (double)qwRandomValue;

            kSleep(1);

            qwRandomValue = kRandom();
            dValue1 /= (double)qwRandomValue;
            dValue2 /= (double)qwRandomValue;
        }

        if (dValue1 != dValue2)
        {
            kPrintf("Value Is Not Same~!!! [%f] != [%f]\n", dValue1, dValue2);
            break;
        }
        qwCount++;

        // ȸ���ϴ� �ٶ����� ǥ��
        pstScreen[iOffset].bCharactor = vcData[qwCount % 4];

        // ���� ����
        pstScreen[iOffset].bAttribute = (iOffset % 15) + 1;
    }
}

/**
 *  ������(PIE)�� ���
 */
static void kTestPIE(const char* pcParameterBuffer)
{
    double dResult;
    int i;

    kPrintf("PIE Cacluation Test\n");
    kPrintf("Result: 355 / 113 = ");
    dResult = (double)355 / 113;
    kPrintf("%d.%d%d\n", (QWORD)dResult, ((QWORD)(dResult * 10) % 10),
        ((QWORD)(dResult * 100) % 10));

    // �Ǽ��� ����ϴ� �½�ũ�� ����
    for (i = 0; i < 100; i++)
    {
        kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kFPUTestTask);
    }
}

/**
 *  ���� �޸� ������ ǥ��
 */
static void kShowDyanmicMemoryInformation(const char* pcParameterBuffer)
{
    QWORD qwStartAddress, qwTotalSize, qwMetaSize, qwUsedSize;

    kGetDynamicMemoryInformation(&qwStartAddress, &qwTotalSize, &qwMetaSize,
        &qwUsedSize);

    kPrintf("============ Dynamic Memory Information ============\n");
    kPrintf("Start Address: [0x%Q]\n", qwStartAddress);
    kPrintf("Total Size:    [0x%Q]byte, [%d]MB\n", qwTotalSize,
        qwTotalSize / 1024 / 1024);
    kPrintf("Meta Size:     [0x%Q]byte, [%d]KB\n", qwMetaSize,
        qwMetaSize / 1024);
    kPrintf("Used Size:     [0x%Q]byte, [%d]KB\n", qwUsedSize, qwUsedSize / 1024);
}

/**
 *  ��� ��� ����Ʈ�� ����� ���������� �Ҵ��ϰ� �����ϴ� �׽�Ʈ
 */
static void kTestSequentialAllocation(const char* pcParameterBuffer)
{
    DYNAMICMEMORY* pstMemory;
    long i, j, k;
    QWORD* pqwBuffer;

    kPrintf("============ Dynamic Memory Test ============\n");
    pstMemory = kGetDynamicMemoryManager();

    for (i = 0; i < pstMemory->iMaxLevelCount; i++)
    {
        kPrintf("Block List [%d] Test Start\n", i);
        kPrintf("Allocation And Compare: ");

        // ��� ����� �Ҵ� �޾Ƽ� ���� ä�� �� �˻�
        for (j = 0; j < (pstMemory->iBlockCountOfSmallestBlock >> i); j++)
        {
            pqwBuffer = kAllocateMemory(DYNAMICMEMORY_MIN_SIZE << i);
            if (pqwBuffer == NULL)
            {
                kPrintf("\nAllocation Fail\n");
                return;
            }

            // ���� ä�� �� �ٽ� �˻�
            for (k = 0; k < (DYNAMICMEMORY_MIN_SIZE << i) / 8; k++)
            {
                pqwBuffer[k] = k;
            }

            for (k = 0; k < (DYNAMICMEMORY_MIN_SIZE << i) / 8; k++)
            {
                if (pqwBuffer[k] != k)
                {
                    kPrintf("\nCompare Fail\n");
                    return;
                }
            }
            // ���� ������ . ���� ǥ��
            kPrintf(".");
        }

        kPrintf("\nFree: ");
        // �Ҵ� ���� ����� ��� ��ȯ
        for (j = 0; j < (pstMemory->iBlockCountOfSmallestBlock >> i); j++)
        {
            if (kFreeMemory((void*)(pstMemory->qwStartAddress +
                (DYNAMICMEMORY_MIN_SIZE << i) * j)) == FALSE)
            {
                kPrintf("\nFree Fail\n");
                return;
            }
            // ���� ������ . ���� ǥ��
            kPrintf(".");
        }
        kPrintf("\n");
    }
    kPrintf("Test Complete~!!!\n");
}

/**
 *  ���Ƿ� �޸𸮸� �Ҵ��ϰ� �����ϴ� ���� �ݺ��ϴ� �½�ũ
 */
static void kRandomAllocationTask(void)
{
    TCB* pstTask;
    QWORD qwMemorySize;
    char vcBuffer[200];
    BYTE* pbAllocationBuffer;
    int i, j;
    int iY;

    pstTask = kGetRunningTask();
    iY = (pstTask->stLink.qwID) % 15 + 9;

    for (j = 0; j < 10; j++)
    {
        // 1KB ~ 32M���� �Ҵ��ϵ��� ��
        do
        {
            qwMemorySize = ((kRandom() % (32 * 1024)) + 1) * 1024;
            pbAllocationBuffer = kAllocateMemory(qwMemorySize);

            // ���� ���۸� �Ҵ� ���� ���ϸ� �ٸ� �½�ũ�� �޸𸮸� ����ϰ� 
            // ���� �� �����Ƿ� ��� ����� �� �ٽ� �õ�
            if (pbAllocationBuffer == 0)
            {
                kSleep(1);
            }
        } while (pbAllocationBuffer == 0);

        kSPrintf(vcBuffer, "|Address: [0x%Q] Size: [0x%Q] Allocation Success",
            pbAllocationBuffer, qwMemorySize);
        // �ڽ��� ID�� Y ��ǥ�� �Ͽ� �����͸� ���
        kPrintStringXY(20, iY, vcBuffer);
        kSleep(200);

        // ���۸� ������ ������ ������ �����͸� �Ȱ��� ä�� 
        kSPrintf(vcBuffer, "|Address: [0x%Q] Size: [0x%Q] Data Write...     ",
            pbAllocationBuffer, qwMemorySize);
        kPrintStringXY(20, iY, vcBuffer);
        for (i = 0; i < qwMemorySize / 2; i++)
        {
            pbAllocationBuffer[i] = kRandom() & 0xFF;
            pbAllocationBuffer[i + (qwMemorySize / 2)] = pbAllocationBuffer[i];
        }
        kSleep(200);

        // ä�� �����Ͱ� ���������� �ٽ� Ȯ��
        kSPrintf(vcBuffer, "|Address: [0x%Q] Size: [0x%Q] Data Verify...   ",
            pbAllocationBuffer, qwMemorySize);
        kPrintStringXY(20, iY, vcBuffer);
        for (i = 0; i < qwMemorySize / 2; i++)
        {
            if (pbAllocationBuffer[i] != pbAllocationBuffer[i + (qwMemorySize / 2)])
            {
                kPrintf("Task ID[0x%Q] Verify Fail\n", pstTask->stLink.qwID);
                kExitTask();
            }
        }
        kFreeMemory(pbAllocationBuffer);
        kSleep(200);
    }

    kExitTask();
}

/**
 *  �½�ũ�� ���� �� �����Ͽ� ������ �޸𸮸� �Ҵ��ϰ� �����ϴ� ���� �ݺ��ϴ� �׽�Ʈ
 */
static void kTestRandomAllocation(const char* pcParameterBuffer)
{
    int i;

    for (i = 0; i < 1000; i++)
    {
        kCreateTask(TASK_FLAGS_LOWEST | TASK_FLAGS_THREAD, 0, 0, (QWORD)kRandomAllocationTask);
    }
}

/**
 *  �ϵ� ��ũ�� ������ ǥ��
 */
static void kShowHDDInformation(const char* pcParameterBuffer)
{
    HDDINFORMATION stHDD;
    char vcBuffer[100];

    // �ϵ� ��ũ�� ������ ����
    if (kReadHDDInformation(TRUE, TRUE, &stHDD) == FALSE)
    {
        kPrintf("HDD Information Read Fail\n");
        return;
    }

    kPrintf("============ Primary Master HDD Information ============\n");

    // �� ��ȣ ���
    kMemCpy(vcBuffer, stHDD.vwModelNumber, sizeof(stHDD.vwModelNumber));
    vcBuffer[sizeof(stHDD.vwModelNumber) - 1] = '\0';
    kPrintf("Model Number:\t %s\n", vcBuffer);

    // �ø��� ��ȣ ���
    kMemCpy(vcBuffer, stHDD.vwSerialNumber, sizeof(stHDD.vwSerialNumber));
    vcBuffer[sizeof(stHDD.vwSerialNumber) - 1] = '\0';
    kPrintf("Serial Number:\t %s\n", vcBuffer);

    // ���, �Ǹ���, �Ǹ��� �� ���� ���� ���
    kPrintf("Head Count:\t %d\n", stHDD.wNumberOfHead);
    kPrintf("Cylinder Count:\t %d\n", stHDD.wNumberOfCylinder);
    kPrintf("Sector Count:\t %d\n", stHDD.wNumberOfSectorPerCylinder);

    // �� ���� �� ���
    kPrintf("Total Sector:\t %d Sector, %dMB\n", stHDD.dwTotalSectors,
        stHDD.dwTotalSectors / 2 / 1024);
}

/**
 *  �ϵ� ��ũ�� �Ķ���ͷ� �Ѿ�� LBA ��巹������ ���� �� ��ŭ ����
 */
static void kReadSector(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcLBA[50], vcSectorCount[50];
    DWORD dwLBA;
    int iSectorCount;
    char* pcBuffer;
    int i, j;
    BYTE bData;
    BOOL bExit = FALSE;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� LBA ��巹���� ���� �� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    if ((kGetNextParameter(&stList, vcLBA) == 0) ||
        (kGetNextParameter(&stList, vcSectorCount) == 0))
    {
        kPrintf("ex) readsector 0(LBA) 10(count)\n");
        return;
    }
    dwLBA = kAToI(vcLBA, 10);
    iSectorCount = kAToI(vcSectorCount, 10);

    // ���� ����ŭ �޸𸮸� �Ҵ� �޾� �б� ����
    pcBuffer = kAllocateMemory(iSectorCount * 512);
    if (kReadHDDSector(TRUE, TRUE, dwLBA, iSectorCount, pcBuffer) == iSectorCount)
    {
        kPrintf("LBA [%d], [%d] Sector Read Success~!!", dwLBA, iSectorCount);
        // ������ ������ ������ ���
        for (j = 0; j < iSectorCount; j++)
        {
            for (i = 0; i < 512; i++)
            {
                if (!((j == 0) && (i == 0)) && ((i % 256) == 0))
                {
                    kPrintf("\nPress any key to continue... ('q' is exit) : ");
                    if (kGetCh() == 'q')
                    {
                        bExit = TRUE;
                        break;
                    }
                }

                if ((i % 16) == 0)
                {
                    kPrintf("\n[LBA:%d, Offset:%d]\t| ", dwLBA + j, i);
                }

                // ��� �� �ڸ��� ǥ���Ϸ��� 16���� ���� ��� 0�� �߰�����
                bData = pcBuffer[j * 512 + i] & 0xFF;
                if (bData < 16)
                {
                    kPrintf("0");
                }
                kPrintf("%X ", bData);
            }

            if (bExit == TRUE)
            {
                break;
            }
        }
        kPrintf("\n");
    }
    else
    {
        kPrintf("Read Fail\n");
    }

    kFreeMemory(pcBuffer);
}

/**
 *  �ϵ� ��ũ�� �Ķ���ͷ� �Ѿ�� LBA ��巹������ ���� �� ��ŭ ��
 */
static void kWriteSector(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcLBA[50], vcSectorCount[50];
    DWORD dwLBA;
    int iSectorCount;
    char* pcBuffer;
    int i, j;
    BOOL bExit = FALSE;
    BYTE bData;
    static DWORD s_dwWriteCount = 0;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� LBA ��巹���� ���� �� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    if ((kGetNextParameter(&stList, vcLBA) == 0) ||
        (kGetNextParameter(&stList, vcSectorCount) == 0))
    {
        kPrintf("ex) writesector 0(LBA) 10(count)\n");
        return;
    }
    dwLBA = kAToI(vcLBA, 10);
    iSectorCount = kAToI(vcSectorCount, 10);

    s_dwWriteCount++;

    // ���۸� �Ҵ� �޾� �����͸� ä��. 
    // ������ 4 ����Ʈ�� LBA ��巹���� 4 ����Ʈ�� ���Ⱑ ����� Ƚ���� ����
    pcBuffer = kAllocateMemory(iSectorCount * 512);
    for (j = 0; j < iSectorCount; j++)
    {
        for (i = 0; i < 512; i += 8)
        {
            *(DWORD*)&(pcBuffer[j * 512 + i]) = dwLBA + j;
            *(DWORD*)&(pcBuffer[j * 512 + i + 4]) = s_dwWriteCount;
        }
    }

    // ���� ����
    if (kWriteHDDSector(TRUE, TRUE, dwLBA, iSectorCount, pcBuffer) != iSectorCount)
    {
        kPrintf("Write Fail\n");
        return;
    }
    kPrintf("LBA [%d], [%d] Sector Write Success~!!", dwLBA, iSectorCount);

    // ������ ������ ������ ���
    for (j = 0; j < iSectorCount; j++)
    {
        for (i = 0; i < 512; i++)
        {
            if (!((j == 0) && (i == 0)) && ((i % 256) == 0))
            {
                kPrintf("\nPress any key to continue... ('q' is exit) : ");
                if (kGetCh() == 'q')
                {
                    bExit = TRUE;
                    break;
                }
            }

            if ((i % 16) == 0)
            {
                kPrintf("\n[LBA:%d, Offset:%d]\t| ", dwLBA + j, i);
            }

            // ��� �� �ڸ��� ǥ���Ϸ��� 16���� ���� ��� 0�� �߰�����
            bData = pcBuffer[j * 512 + i] & 0xFF;
            if (bData < 16)
            {
                kPrintf("0");
            }
            kPrintf("%X ", bData);
        }

        if (bExit == TRUE)
        {
            break;
        }
    }
    kPrintf("\n");
    kFreeMemory(pcBuffer);
}

/**
 *  �ϵ� ��ũ�� ����
 */
static void kMountHDD(const char* pcParameterBuffer)
{
    if (kMount() == FALSE)
    {
        kPrintf("HDD Mount Fail\n");
        return;
    }
    kPrintf("HDD Mount Success\n");
}

/**
 *  �ϵ� ��ũ�� ���� �ý����� ����(����)
 */
static void kFormatHDD(const char* pcParameterBuffer)
{
    if (kFormat() == FALSE)
    {
        kPrintf("HDD Format Fail\n");
        return;
    }
    kPrintf("HDD Format Success\n");
}

/**
 *  ���� �ý��� ������ ǥ��
 */
static void kShowFileSystemInformation(const char* pcParameterBuffer)
{
    FILESYSTEMMANAGER stManager;

    kGetFileSystemInformation(&stManager);

    kPrintf("================== File System Information ==================\n");
    kPrintf("Mouted:\t\t\t\t\t %d\n", stManager.bMounted);
    kPrintf("Reserved Sector Count:\t\t\t %d Sector\n", stManager.dwReservedSectorCount);
    kPrintf("Cluster Link Table Start Address:\t %d Sector\n",
        stManager.dwClusterLinkAreaStartAddress);
    kPrintf("Cluster Link Table Size:\t\t %d Sector\n", stManager.dwClusterLinkAreaSize);
    kPrintf("Data Area Start Address:\t\t %d Sector\n", stManager.dwDataAreaStartAddress);
    kPrintf("Total Cluster Count:\t\t\t %d Cluster\n", stManager.dwTotalClusterCount);
}

/**
 *  ���� ���͸��� �� ������ ����
 */
static void kCreateFileInDirectory(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    DWORD dwCluster;
    int i;
    FILE* pstFile;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    pstFile = fopen(vcFileName, "w");
    if (pstFile == NULL)
    {
        kPrintf("File Create Fail\n");
        return;
    }
    fclose(pstFile);
    kPrintf("File Create Success\n");
}

/**
 *   ���� ���͸����� ������ ����
 */
static void kDeleteFileInDirectory(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    if (remove(vcFileName) != 0)
    {
        kPrintf("File Not Found or File Opened\n");
        return;
    }

    kPrintf("File Delete Success\n");
}

static void kCreateDirectoryInDirectory(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    DWORD dwCluster;
    int i;
    FILE* pstFile;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    pstFile = makedir(vcFileName);
    if (pstFile == NULL)
    {
        kPrintf("Directory Create Fail\n");
        return;
    }
    fclose(pstFile);
    kPrintf("Directory Create Success\n");
}

static void kDeleteDirectoryInDirectory(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    if (rmdir(vcFileName) != 0)
    {
        kPrintf("Directory Not Found or Directory Opened\n");
        return;
    }

    kPrintf("Directory Delete Success\n");
}

/**
 *  ���� ���͸��� ���� ����� ǥ��
 */
static void kShowDirectory(const char* pcParameterBuffer)
{
    DIR* pstDirectory;
    int i, iCount, iTotalCount;
    DIRECTORYENTRY stEntry;
    char vcBuffer[400];
    char vcTempValue[50];
    DWORD dwTotalByte;
    DWORD dwUsedClusterCount;
    FILESYSTEMMANAGER stManager;
    DIRECTORYENTRY dir;
    // ���� �ý��� ������ ����
    kGetFileSystemInformation(&stManager);

    // ���� ���͸��� ��
    pstDirectory = opendir(".");

    if (pstDirectory == NULL)
    {
        kPrintf("Current Directory Open Fail\n");
        return;
    }

    // ���� ������ ���鼭 ���͸��� �ִ� ������ ������ ��ü ������ ����� ũ�⸦ ���
    iTotalCount = 0;
    dwTotalByte = 0;
    dwUsedClusterCount = 0;
    while (1)
    {
        kMemSet(&stEntry, 0, sizeof(DIRECTORYENTRY));
        // ���͸����� ��Ʈ�� �ϳ��� ����
        if(readdir((void*)&stEntry, pstDirectory) == FALSE)
        {
            break;
        }
        iTotalCount++;
        dwTotalByte += stEntry.dwFileSize;

        // ������ ���� Ŭ�������� ������ ���
        if (stEntry.dwFileSize == 0)
        {
            // ũ�Ⱑ 0�̶� Ŭ������ 1���� �Ҵ�Ǿ� ����
            dwUsedClusterCount++;
        }
        else
        {
            // Ŭ������ ������ �ø��Ͽ� ����
            dwUsedClusterCount += (stEntry.dwFileSize +
                (FILESYSTEM_CLUSTERSIZE - 1)) / FILESYSTEM_CLUSTERSIZE;
        }
    }

    // ���� ������ ������ ǥ���ϴ� ����
    rewinddir(pstDirectory);
    iCount = 0;
    while (1)
    {
        kMemSet(&stEntry, 0, sizeof(DIRECTORYENTRY));
        // ���͸����� ��Ʈ�� �ϳ��� ����
        if (readdir((void*)&stEntry, pstDirectory) == FALSE)
        {
            break;
        }

        // ���� �������� �ʱ�ȭ �� �� �� ��ġ�� ���� ����
        kMemSet(vcBuffer, ' ', sizeof(vcBuffer) - 1);
        vcBuffer[sizeof(vcBuffer) - 1] = '\0';

        // ���� �̸� ����
        kMemCpy(vcBuffer, stEntry.d_name,
            kStrLen(stEntry.d_name));

        // ���� ���� ����
        if (stEntry.bType == FILESYSTEM_TYPE_FILE)
        {
            kSPrintf(vcTempValue, "%4d Byte", stEntry.dwFileSize);
            kMemCpy(vcBuffer + 20, vcTempValue, kStrLen(vcTempValue));
            kSPrintf(vcTempValue, "0x%#X Cluster", stEntry.dwStartClusterIndex);
            kMemCpy(vcBuffer + 35, vcTempValue, kStrLen(vcTempValue) + 1);
        }
        else
        {
            kSPrintf(vcTempValue, "Directory");
            kMemCpy(vcBuffer + 20, vcTempValue, kStrLen(vcTempValue));
            kSPrintf(vcTempValue, "             ");
        kMemCpy(vcBuffer + 35, vcTempValue, kStrLen(vcTempValue) + 1);
        }
        // kMemCpy(vcBuffer + 25, vcTempValue, kStrLen(vcTempValue));

        // ������ ���� Ŭ������ ����
        if(stEntry.bType = FILESYSTEM_TYPE_FILE)
        {
			kSPrintf(vcTempValue, "%#3X Cluster ", stEntry.dwStartClusterIndex);
			kMemCpy(vcBuffer + 35, vcTempValue, kStrLen(vcTempValue) + 1);
        }

        kPrintf("%s    ", vcBuffer);

        kPrintf("%4d-%2d-%2d %02d:%02d:%02d\n", stEntry.year, stEntry.month, stEntry.dayOfMonth, stEntry.hour, stEntry.minute, stEntry.second);
        if ((iCount != 0) && ((iCount % 20) == 0))
        {
            kPrintf("Press any key to continue... ('q' is exit) : ");
            if (kGetCh() == 'q')
            {
                kPrintf("\n");
                break;
            }
        }
        iCount++;
    }

    // �� ������ ������ ������ �� ũ�⸦ ���
    kPrintf("\t\tTotal File Count: %d\n", iTotalCount);
    kPrintf("\t\tTotal File Size: %d KByte (%d Cluster)\n", dwTotalByte,
        dwUsedClusterCount);

    // ���� Ŭ������ ���� �̿��ؼ� ���� ������ ���
    kPrintf("\t\tFree Space: %d KByte (%d Cluster)\n",
        (stManager.dwTotalClusterCount - dwUsedClusterCount) *
        FILESYSTEM_CLUSTERSIZE / 1024, stManager.dwTotalClusterCount -
        dwUsedClusterCount);

    // ���͸��� ����
    closedir(pstDirectory);
}


/**
 *  ������ �����Ͽ� Ű����� �Էµ� �����͸� ��
 */
static void kWriteDataToFile(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    FILE* fp;
    int iEnterCount;
    BYTE bKey;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    // ���� ����
    fp = fopen(vcFileName, "w");
    if (fp == NULL)
    {
        kPrintf("%s File Open Fail\n", vcFileName);
        return;
    }

    // ���� Ű�� �������� 3�� ������ ������ ������ ���Ͽ� ��
    iEnterCount = 0;
    while (1)
    {
        bKey = kGetCh();
        // ���� Ű�̸� ���� 3�� �������°� Ȯ���Ͽ� ������ ���� ����
        if (bKey == KEY_ENTER)
        {
            iEnterCount++;
            if (iEnterCount >= 3)
            {
                break;
            }
        }
        // ���� Ű�� �ƴ϶�� ���� Ű �Է� Ƚ���� �ʱ�ȭ
        else
        {
            iEnterCount = 0;
        }

        kPrintf("%c", bKey);
        if (fwrite(&bKey, 1, 1, fp) != 1)
        {
            kPrintf("File Wirte Fail\n");
            break;
        }
    }

    kPrintf("File Create Success\n");
    fclose(fp);
}

/**
 *  ������ ��� �����͸� ����
 */
static void kReadDataFromFile(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    FILE* fp;
    int iEnterCount;
    BYTE bKey;

    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    // ���� ����
    fp = fopen(vcFileName, "r");
    if (fp == NULL)
    {
        kPrintf("%s File Open Fail\n", vcFileName);
        return;
    }

    // ������ ������ ����ϴ� ���� �ݺ�
    iEnterCount = 0;
    while (1)
    {
        if (fread(&bKey, 1, 1, fp) != 1)
        {
            break;
        }
        kPrintf("%c", bKey);

        // ���� ���� Ű�̸� ���� Ű Ƚ���� ������Ű�� 20���α��� ����ߴٸ�
        // �� ������� ���θ� ���
        if (bKey == KEY_ENTER)
        {
            iEnterCount++;

            if ((iEnterCount != 0) && ((iEnterCount % 20) == 0))
            {
                kPrintf("Press any key to continue... ('q' is exit) : ");
                if (kGetCh() == 'q')
                {
                    kPrintf("\n");
                    break;
                }
                kPrintf("\n");
                iEnterCount = 0;
            }
        }
    }
    fclose(fp);
}

// ������ �а� ���� �ӵ��� ����
static void kTestPerformance(const char* pcParameterBuffer)
{
    FILE* pstFile;
    DWORD dwClusterTestFileSize;
    DWORD dwOneByteTestFileSize;
    QWORD qwLastTickCount;
    DWORD i;
    BYTE* pbBuffer;

    // Ŭ�����ʹ� 1MB���� ������ �׽�Ʈ
    dwClusterTestFileSize = 1024 * 1024;
    // 1����Ʈ�� �а� ���� �׽�Ʈ�� �ð��� ���� �ɸ��Ƿ� 16KB�� �׽�Ʈ
    dwOneByteTestFileSize = 16 * 1024;

    // �׽�Ʈ�� ���� �޸� �Ҵ�
    pbBuffer = kAllocateMemory(dwClusterTestFileSize);

    if (pbBuffer == NULL)
    {
        kPrintf("Memory Allcate Fail\n");
        return;
    }

    // ���۸� �ʱ�ȭ
    kMemSet(pbBuffer, 0, FILESYSTEM_CLUSTERSIZE);

    kPrintf("===================== File I/O Performance Test =====================\n");

    //==================================================================================
    // Ŭ������ ������ ������ ���������� ���� �׽�Ʈ
    //==================================================================================
    kPrintf("1.Sequential Read/Write Test(Cluster Size)\n");

    // ������ �׽�Ʈ ������ �����ϰ� ���� ����
    remove("performance.txt");
    pstFile = fopen("performance.txt", "w");
    if (pstFile == NULL)
    {
        kPrintf("File Open Fail\n");
        kFreeMemory(pbBuffer);
        return;
    }

    qwLastTickCount = kGetTickCount();
    // Ŭ������ ������ ���� �׽�Ʈ
    for (i = 0; i < (dwClusterTestFileSize / FILESYSTEM_CLUSTERSIZE); i++)
    {
        if (fwrite(pbBuffer, 1, FILESYSTEM_CLUSTERSIZE, pstFile) !=
            FILESYSTEM_CLUSTERSIZE)
        {
            kPrintf("Write Fail\n");
            // ������ �ݰ� �޸𸮸� ������
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // �ð� ���
    kPrintf("   Sequential Write(Cluster Size): %d ms\n", kGetTickCount() -
        qwLastTickCount);

    //===================================================================================
    // Ŭ������ ������ ������ ���������� �д� �׽�Ʈ
    //===================================================================================
    // ������ ó������ �̵�
    fseek(pstFile, 0, SEEK_SET);

    qwLastTickCount = kGetTickCount();
    // Ŭ������ ������ �д� �׽�Ʈ
    for (i = 0; i < (dwClusterTestFileSize / FILESYSTEM_CLUSTERSIZE); i++)
    {
        if (fread(pbBuffer, 1, FILESYSTEM_CLUSTERSIZE, pstFile) !=
            FILESYSTEM_CLUSTERSIZE)
        {
            kPrintf("Read Fail\n");
            // ������ �ݰ� �޸𸮸� ������
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // �ð� ���
    kPrintf("   Seqeuntial Read(Cluster Size): %d ms\n", kGetTickCount() -
        qwLastTickCount);

    //===================================================================================
    // 1����Ʈ ������ ������ ���������� ���� �׽�Ʈ
    //===================================================================================
    kPrintf("2.Sequential Read/Write Test(1 Byte)\n");

    // ������ �׽�Ʈ ������ �����ϰ� ���� ����
    remove("performance.txt");
    pstFile = fopen("performance.txt", "w");
    if (pstFile == NULL)
    {
        kPrintf("File Open Fail\n");
        kFreeMemory(pbBuffer);
        return;
    }

    qwLastTickCount = kGetTickCount();
    // 1����Ʈ ������ ���� �׽�Ʈ
    for (i = 0; i < dwOneByteTestFileSize; i++)
    {
        if (fwrite(pbBuffer, 1, 1, pstFile) != 1)
        {
            kPrintf("Write Fail\n");
            // ������ �ݰ� �޸𸮸� ������
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // �ð� ���
    kPrintf("   Sequential Write(1Byte): %d ms\n", kGetTickCount() -
        qwLastTickCount);
    //===================================================================================
    // 1����Ʈ ������ ������ ���������� �д� �׽�Ʈ
    //===================================================================================
    // ������ ó������ �̵�
    fseek(pstFile, 0, SEEK_SET);

    qwLastTickCount = kGetTickCount();
    // 1����Ʈ ������ �д� �׽�Ʈ
    for (i = 0; i < dwOneByteTestFileSize; i++)
    {
        if (fread(pbBuffer, 1, 1, pstFile) != 1)
        {
            kPrintf("Read Fail\n");
            // ������ �ݰ� �޸𸮸� ������
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // �ð� ���
    kPrintf("    Seqeuntial Read(1 Byte): %d ms\n", kGetTickCount() -
        qwLastTickCount);

    // ������ �ݰ� �޸𸮸� ������
    fclose(pstFile);
    kFreeMemory(pbBuffer);
}

// ���� �ý����� ĳ�� ���ۿ� �ִ� �����͸� ��� �ϵ� ��ũ�� ��
static void kFlushCache(const char* pcParameterBuffer)
{
    QWORD qwTickCount;

    qwTickCount = kGetTickCount();
    kPrintf("Cache Flush... ");
    if (kFlushFileSystemCache() == TRUE)
    {
        kPrintf("Pass\n");
    }
    else
    {
        kPrintf("Fail\n");
    }
    kPrintf("Total Time = %d ms\n", kGetTickCount() - qwTickCount);
}

static void kChangeDirectoryInDirectory(const char* pcParameterBuffer)
{
	DIR* pstCurrentDirectory;
	DIR* pstMovedDirectory;
    PARAMETERLIST stList;
    char vcFileName[200];
    char tempBuffer[25];
    int iLength;
    DWORD dwCluster;
    int i, j;
    BOOL fileEnd;
    // for print current path
    cpyDirArray(1);
    // �Ķ���� ����Ʈ�� �ʱ�ȭ�Ͽ� ���� �̸��� ����
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short Directory Name\n");
        return;
    }

    i = 0;
    fileEnd = FALSE;

    // ���� ��� �̵�
    if(vcFileName[0] == '/')
    {
    		// ��Ʈ ���͸� ����
    	pstCurrentDirectory = kOpenRootDirectory();
    	if (pstCurrentDirectory == NULL)
		{
			kPrintf("Root Directory Open Fail\n");
			return;
		}

        // for print current path
        tWorkingDirIndex = 0;

    	i++;

    		// ��Ʈ ���͸��θ� �̵��ϴ� ���
    	if(vcFileName[i] == '\0')
    	{
    		closedir(pstCurrentDirectory);
            workingDirIndex = 0;
    		kPrintf("Directory Change Success\n");
    		return;
    	}
    }
    // ��� ��� �̵�
    else
    {
		// ���� ���͸� ����
		pstCurrentDirectory = kOpenDirectory(".");
		if (pstCurrentDirectory == NULL)
		{
			kPrintf("Current Directory Open Fail\n");
			return;
		}

		// �̹� ���� ���͸��� �������Ƿ� �� �տ� '.'�� ���� �����Ѵ�
		if(vcFileName[0] == '.')
		{
			// ���� ���͸��θ� �̵��ϴ� ����̸� �ƹ��� ������ ���� �ʴ´�.
			if(vcFileName[1] == '\0')
			{
				closedir(pstCurrentDirectory);
				kPrintf("Directory Change Success\n");
				return;
			}
			else if(vcFileName[1] == '/')
			{
				i += 2;
			}
			// ��Ʈ ���͸��� ��� �θ� ���͸��� �ڱ� �ڽ��� ����Ű�Ƿ�
			// �̿� ���� ����ó���� ���ش�.
			else if(vcFileName[1] == '.' &&	(pstCurrentDirectory->stDirectoryHandle.dwStartClusterIndex == 1))
			{
				// ���� ���͸��θ� �̵��ϴ� ����̸� �ƹ��� ������ ���� �ʴ´�.
				if(vcFileName[2] == '\0')
				{
					closedir(pstCurrentDirectory);
                    if(workingDirIndex != 0)
                    {
                        workingDirIndex--;
                    }
					kPrintf("Directory Change Success\n");
					return;
				}
				else if(vcFileName[2] == '/')
				{
					i += 3;
				}
			}
		}
    }

	// ���� ���͸� ���������� ����
	while(TRUE)
	{
		j = 0;
		kMemSet(tempBuffer, 0, sizeof(tempBuffer));
		while((vcFileName[i] != '/') && (vcFileName[i] != '\0'))
		{
			 tempBuffer[j] = vcFileName[i];

             // for print current path
             tWorkingDir[tWorkingDirIndex][j] = vcFileName[i];

			 i++;
			 j++;
		}

        // for print current path
        tWorkingDir[tWorkingDirIndex][j] = '\0';
        if(tWorkingDir[tWorkingDirIndex][0] == '.' && tWorkingDir[tWorkingDirIndex][1] == '.' && tWorkingDir[tWorkingDirIndex][2] == '\0')
        {
            if(tWorkingDirIndex != 0)
            {
                tWorkingDirIndex--;
            }
        }
        else if(tWorkingDir[tWorkingDirIndex][0] == '.' && tWorkingDir[tWorkingDirIndex][1] == '\0')
        {
            // nothing
        }
        else if(tWorkingDir[tWorkingDirIndex][0] == '\0')
        {
            // nothing
        }
        else
        {
            tWorkingDirIndex++;
        }

		if(vcFileName[i] == '\0')
		{
			fileEnd = TRUE;
		}
		i++;
		tempBuffer[j] = '\0';
		pstMovedDirectory = cd(tempBuffer);
		if (pstMovedDirectory == NULL)
		{
			kPrintf("Directory Change Fail\n");
			return;
		}

		closedir(pstCurrentDirectory);
		pstCurrentDirectory = pstMovedDirectory;

		if(fileEnd == TRUE)
		{
			break;
		}
	}

	closedir(pstCurrentDirectory);
    cpyDirArray(0);
	kPrintf("Directory Change Success\n");
}

// for print current path
void printCurPath()
{
    if(workingDirIndex == 0)
    {
        kPrintf("/");
    }
    else{
        for(int i = 0; i < workingDirIndex; i++)
        {
            kPrintf("/%s", workingDir[i]);
        }
    }
    kPrintf(">");
}

void cpyDirArray(int switchNum)
{
    int i;
    int j;

    if(switchNum == 0)
    {
        if(tWorkingDirIndex == 0)
        {
            workingDirIndex = 0;
        }
        else
        {
            workingDirIndex = tWorkingDirIndex;
            for(i = 0; i < tWorkingDirIndex; i++)
            {
                for(j = 0; j < FILESYSTEM_MAXFILENAMELENGTH; j++)
                {
                    workingDir[i][j] = tWorkingDir[i][j];
                    if(workingDir[i][j] == '\0')
                    {
                        break;
                    }
                }
            }
        }
    }
    else if(switchNum == 1)
    {
        if(workingDirIndex == 0)
        {
            tWorkingDirIndex = 0;
        }
        else
        {
            tWorkingDirIndex = workingDirIndex;
            for(i = 0; i < workingDirIndex; i++)
            {
                for(j = 0; j < FILESYSTEM_MAXFILENAMELENGTH; j++)
                {
                    tWorkingDir[i][j] = workingDir[i][j];
                    if(tWorkingDir[i][j] == '\0')
                    {
                        break;
                    }
                }
            }
        }
    }
}
