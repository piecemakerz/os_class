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
//  실제 셸을 구성하는 코드
//==============================================================================
/**
 *  셸의 메인 루프
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

    kMemSet(lineClear, ' ', 79);
    lineClear[79] = '\0';

    curMallocPos = 0x1436F0;
    head = kMalloc(sizeof(Trie));
    kTrieInitialize(head);

    for (int i = 0; i < 34; i++)
    {
        kTrieInsert(head, gs_vstCommandTable[i].pcCommand);
    }

    // 프롬프트 출력
    kPrintf(CONSOLESHELL_PROMPTMESSAGE);
    while (1)
    {
        // 키가 수신될 때까지 대기
        bKey = kGetCh();
        // Backspace 키 처리
        if (bKey == KEY_BACKSPACE)
        {
            if (iCommandBufferIndex > 0)
            {
                // 현재 커서 위치를 얻어서 한 문자 앞으로 이동한 다음 공백을 출력하고
                // 커맨드 버퍼에서 마지막 문자 삭제
                kGetCursor(&iCursorX, &iCursorY);
                kPrintStringXY(iCursorX - 1, iCursorY, " ");
                kSetCursor(iCursorX - 1, iCursorY);
                iCommandBufferIndex--;
            }
        }
        // 엔터 키 처리
        else if (bKey == KEY_ENTER)
        {
            kPrintf("\n");

            if (iCommandBufferIndex > 0)
            {
                // 커맨드 버퍼에 있는 명령을 실행
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

            // 프롬프트 출력 및 커맨드 버퍼 초기화
            kPrintf("%s", CONSOLESHELL_PROMPTMESSAGE);
            kMemSet(vcCommandBuffer, '\0', CONSOLESHELL_MAXCOMMANDBUFFERCOUNT);
            iCommandBufferIndex = 0;
        }
        // 시프트 키, CAPS Lock, NUM Lock, Scroll Lock은 무시
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
            // TAB은 공백으로 전환
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

            // 버퍼에 공간이 남아있을 때만 가능
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
 *  커맨드 버퍼에 있는 커맨드를 비교하여 해당 커맨드를 처리하는 함수를 수행
 */
void kExecuteCommand(const char* pcCommandBuffer)
{
    int i, iSpaceIndex;
    int iCommandBufferLength, iCommandLength;
    int iCount;

    // 공백으로 구분된 커맨드를 추출
    iCommandBufferLength = kStrLen(pcCommandBuffer);
    for (iSpaceIndex = 0; iSpaceIndex < iCommandBufferLength; iSpaceIndex++)
    {
        if (pcCommandBuffer[iSpaceIndex] == ' ')
        {
            break;
        }
    }

    // 커맨드 테이블을 검사해서 동일한 이름의 커맨드가 있는지 확인
    iCount = sizeof(gs_vstCommandTable) / sizeof(SHELLCOMMANDENTRY);
    for (i = 0; i < iCount; i++)
    {
        iCommandLength = kStrLen(gs_vstCommandTable[i].pcCommand);
        // 커맨드의 길이와 내용이 완전히 일치하는지 검사
        if ((iCommandLength == iSpaceIndex) &&
            (kMemCmp(gs_vstCommandTable[i].pcCommand, pcCommandBuffer,
                iSpaceIndex) == 0))
        {
            gs_vstCommandTable[i].pfFunction(pcCommandBuffer + iSpaceIndex + 1);
            break;
        }
    }

    // 리스트에서 찾을 수 없다면 에러 출력
    if (i >= iCount)
    {
        kPrintf("'%s' is not found.\n", pcCommandBuffer);
    }
}

/**
 *  파라미터 자료구조를 초기화
 */
void kInitializeParameter(PARAMETERLIST* pstList, const char* pcParameter)
{
    pstList->pcBuffer = pcParameter;
    pstList->iLength = kStrLen(pcParameter);
    pstList->iCurrentPosition = 0;
}

/**
 *  공백으로 구분된 파라미터의 내용과 길이를 반환
 */
int kGetNextParameter(PARAMETERLIST* pstList, char* pcParameter)
{
    int i;
    int iLength;

    // 더 이상 파라미터가 없으면 나감
    if (pstList->iLength <= pstList->iCurrentPosition)
    {
        return 0;
    }

    // 버퍼의 길이만큼 이동하면서 공백을 검색
    for (i = pstList->iCurrentPosition; i < pstList->iLength; i++)
    {
        if (pstList->pcBuffer[i] == ' ')
        {
            break;
        }
    }

    // 파라미터를 복사하고 길이를 반환
    kMemCpy(pcParameter, pstList->pcBuffer + pstList->iCurrentPosition, i);
    iLength = i - pstList->iCurrentPosition;
    pcParameter[iLength] = '\0';

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

    // 가장 긴 커맨드의 길이를 계산
    for (i = 0; i < iCount; i++)
    {
        iLength = kStrLen(gs_vstCommandTable[i].pcCommand);
        if (iLength > iMaxCommandLength)
        {
            iMaxCommandLength = iLength;
        }
    }

    // 도움말 출력
    for (i = 0; i < iCount; i++)
    {
        kPrintf("%s", gs_vstCommandTable[i].pcCommand);
        kGetCursor(&iCursorX, &iCursorY);
        kSetCursor(iMaxCommandLength, iCursorY);
        kPrintf("  - %s\n", gs_vstCommandTable[i].pcHelp);
    }
}

/**
 *  화면을 지움
 */
static void kCls(const char* pcParameterBuffer)
{
    // 맨 윗줄은 디버깅 용으로 사용하므로 화면을 지운 후, 라인 1로 커서 이동
    kClearScreen();
    kSetCursor(0, 1);
}

/**
 *  총 메모리 크기를 출력
 */
static void kShowTotalRAMSize(const char* pcParameterBuffer)
{
    kPrintf("Total RAM Size = %d MB\n", kGetTotalRAMSize());
}

/**
 *  문자열로 된 숫자를 숫자로 변환하여 화면에 출력
 */
static void kStringToDecimalHexTest(const char* pcParameterBuffer)
{
    char vcParameter[100];
    int iLength;
    PARAMETERLIST stList;
    int iCount = 0;
    long lValue;

    // 파라미터 초기화
    kInitializeParameter(&stList, pcParameterBuffer);

    while (1)
    {
        // 다음 파라미터를 구함, 파라미터의 길이가 0이면 파라미터가 없는 것이므로
        // 종료
        iLength = kGetNextParameter(&stList, vcParameter);
        if (iLength == 0)
        {
            break;
        }

        // 파라미터에 대한 정보를 출력하고 16진수인지 10진수인지 판단하여 변환한 후
        // 결과를 printf로 출력
        kPrintf("Param %d = '%s', Length = %d, ", iCount + 1,
            vcParameter, iLength);

        // 0x로 시작하면 16진수, 그외는 10진수로 판단
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
 *  PC를 재시작(Reboot)
 */
static void kShutdown(const char* pcParamegerBuffer)
{
    kPrintf("System Shutdown Start...\n");

    // 파일 시스템 캐시에 들어 있는 내용을 하드 디스크로 옮김
    kPrintf("Cache Flush ... ");
    if (kFlushFileSystemCache() == TRUE)
    {
        kPrintf("Pass\n");
    }
    else
    {
        kPrintf("Fail\n");
    }

    // 키보드 컨트롤러를 통해 PC를 재시작
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
    //kFree占쏙옙 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占쏙옙 占쌌울옙 占쌩곤옙
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
 *  PIT 컨트롤러의 카운터 0 설정
 */
static void kSetTimer(const char* pcParameterBuffer)
{
    char vcParameter[100];
    PARAMETERLIST stList;
    long lValue;
    BOOL bPeriodic;

    // 파라미터 초기화
    kInitializeParameter(&stList, pcParameterBuffer);

    // milisecond 추출
    if (kGetNextParameter(&stList, vcParameter) == 0)
    {
        kPrintf("ex)settimer 10(ms) 1(periodic)\n");
        return;
    }
    lValue = kAToI(vcParameter, 10);

    // Periodic 추출
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
 *  PIT 컨트롤러를 직접 사용하여 ms 동안 대기
 */
static void kWaitUsingPIT(const char* pcParameterBuffer)
{
    char vcParameter[100];
    int iLength;
    PARAMETERLIST stList;
    long lMillisecond;
    int i;

    // 파라미터 초기화
    kInitializeParameter(&stList, pcParameterBuffer);
    if (kGetNextParameter(&stList, vcParameter) == 0)
    {
        kPrintf("ex)wait 100(ms)\n");
        return;
    }

    lMillisecond = kAToI(pcParameterBuffer, 10);
    kPrintf("%d ms Sleep Start...\n", lMillisecond);

    // 인터럽트를 비활성화하고 PIT 컨트롤러를 통해 직접 시간을 측정
    kDisableInterrupt();
    for (i = 0; i < lMillisecond / 30; i++)
    {
        kWaitUsingDirectPIT(MSTOCOUNT(30));
    }
    kWaitUsingDirectPIT(MSTOCOUNT(lMillisecond % 30));
    kEnableInterrupt();
    kPrintf("%d ms Sleep Complete\n", lMillisecond);

    // 타이머 복원
    kInitializePIT(MSTOCOUNT(1), TRUE);
}

/**
 *  타임 스탬프 카운터를 읽음
 */
static void kReadTimeStampCounter(const char* pcParameterBuffer)
{
    QWORD qwTSC;

    qwTSC = kReadTSC();
    kPrintf("Time Stamp Counter = %q\n", qwTSC);
}

/**
 *  프로세서의 속도를 측정
 */
static void kMeasureProcessorSpeed(const char* pcParameterBuffer)
{
    int i;
    QWORD qwLastTSC, qwTotalTSC = 0;

    kPrintf("Now Measuring.");

    // 10초 동안 변화한 타임 스탬프 카운터를 이용하여 프로세서의 속도를 간접적으로 측정
    kDisableInterrupt();
    for (i = 0; i < 200; i++)
    {
        qwLastTSC = kReadTSC();
        kWaitUsingDirectPIT(MSTOCOUNT(50));
        qwTotalTSC += kReadTSC() - qwLastTSC;

        kPrintf(".");
    }
    // 타이머 복원
    kInitializePIT(MSTOCOUNT(1), TRUE);
    kEnableInterrupt();

    kPrintf("\nCPU Speed = %d MHz\n", qwTotalTSC / 10 / 1000 / 1000);
}

/**
 *  RTC 컨트롤러에 저장된 일자 및 시간 정보를 표시
 */
static void kShowDateAndTime(const char* pcParameterBuffer)
{
    BYTE bSecond, bMinute, bHour;
    BYTE bDayOfWeek, bDayOfMonth, bMonth;
    WORD wYear;

    // RTC 컨트롤러에서 시간 및 일자를 읽음
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
 *  태스크 1
 *      화면 테두리를 돌면서 문자를 출력
 */
static void kTestTask1(void)
{
    BYTE bData;
    int i = 0, iX = 0, iY = 0, iMargin, j;
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;

    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    pstRunningTask = kGetRunningTask();
    iMargin = (pstRunningTask->stLink.qwID & 0xFFFFFFFF) % 10;

    // 화면 네 귀퉁이를 돌면서 문자 출력
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

        // 문자 및 색깔 지정
        pstScreen[iY * CONSOLE_WIDTH + iX].bCharactor = bData;
        pstScreen[iY * CONSOLE_WIDTH + iX].bAttribute = bData & 0x0F;
        bData++;

        // 다른 태스크로 전환
        kSchedule();
    }

    kExitTask();
}

/**
 *  태스크 2
 *      자신의 ID를 참고하여 특정 위치에 회전하는 바람개비를 출력
 */
static void kTestTask2(void)
{
    int i = 0, iOffset;
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;
    TCB* pstRunningTask;
    char vcData[4] = { '-', '\\', '|', '/' };

    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    pstRunningTask = kGetRunningTask();
    iOffset = (pstRunningTask->stLink.qwID & 0xFFFFFFFF) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT -
        (iOffset % (CONSOLE_WIDTH * CONSOLE_HEIGHT));

    while (1)
    {
        // 회전하는 바람개비를 표시
        pstScreen[iOffset].bCharactor = vcData[i % 4];
        // 색깔 지정
        pstScreen[iOffset].bAttribute = (iOffset % 15) + 1;
        i++;

        // 다른 태스크로 전환
        kSchedule();
    }
}

/**
 *  태스크를 생성해서 멀티 태스킹 수행
 */
static void kCreateTestTask(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcType[30];
    char vcCount[30];
    int i;

    // 파라미터를 추출
    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcType);
    kGetNextParameter(&stList, vcCount);

    switch (kAToI(vcType, 10))
    {
        // 타입 1 태스크 생성
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

        // 타입 2 태스크 생성
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
 *  태스크의 우선 순위를 변경
 */
static void kChangeTaskPriority(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcID[30];
    char vcPriority[30];
    QWORD qwID;
    BYTE bPriority;

    // 파라미터를 추출
    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcID);
    kGetNextParameter(&stList, vcPriority);

    // 태스크의 우선 순위를 변경
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
 *  현재 생성된 모든 태스크의 정보를 출력
 */
static void kShowTaskList(const char* pcParameterBuffer)
{
    int i;
    TCB* pstTCB;
    int iCount = 0;

    kPrintf("=========== Task Total Count [%d] ===========\n", kGetTaskCount());
    for (i = 0; i < TASK_MAXCOUNT; i++)
    {
        // TCB를 구해서 TCB가 사용 중이면 ID를 출력
        pstTCB = kGetTCBInTCBPool(i);
        if ((pstTCB->stLink.qwID >> 32) != 0)
        {
            // 태스크가 10개 출력될 때마다, 계속 태스크 정보를 표시할지 여부를 확인
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
 *  태스크를 종료
 */
static void kKillTask(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcID[30];
    QWORD qwID;
    TCB* pstTCB;
    int i;

    // 파라미터를 추출
    kInitializeParameter(&stList, pcParameterBuffer);
    kGetNextParameter(&stList, vcID);

    // 태스크를 종료
    if (kMemCmp(vcID, "0x", 2) == 0)
    {
        qwID = kAToI(vcID + 2, 16);
    }
    else
    {
        qwID = kAToI(vcID, 10);
    }

    // 특정 ID만 종료하는 경우
    if (qwID != 0xFFFFFFFF)
    {
        pstTCB = kGetTCBInTCBPool(GETTCBOFFSET(qwID));
        qwID = pstTCB->stLink.qwID;

        // 시스템 테스트는 제외
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
    // 콘솔 셸과 유휴 태스크를 제외하고 모든 태스크 종료
    else
    {
        for (i = 0; i < TASK_MAXCOUNT; i++)
        {
            pstTCB = kGetTCBInTCBPool(i);
            qwID = pstTCB->stLink.qwID;

            // 시스템 테스트는 삭제 목록에서 제외
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
 *  프로세서의 사용률을 표시
 */
static void kCPULoad(const char* pcParameterBuffer)
{
    kPrintf("Processor Load : %d%%\n", kGetProcessorLoad());
}

// 뮤텍스 테스트용 뮤텍스와 변수
static MUTEX gs_stMutex;
static volatile QWORD gs_qwAdder;

/**
 *  뮤텍스를 테스트하는 태스크
 */
static void kPrintNumberTask(void)
{
    int i;
    int j;
    QWORD qwTickCount;

    // 50ms 정도 대기하여 콘솔 셸이 출력하는 메시지와 겹치지 않도록 함
    qwTickCount = kGetTickCount();
    while ((kGetTickCount() - qwTickCount) < 50)
    {
        kSchedule();
    }

    // 루프를 돌면서 숫자를 출력
    for (i = 0; i < 5; i++)
    {
        kLock(&(gs_stMutex));
        kPrintf("Task ID [0x%Q] Value[%d]\n", kGetRunningTask()->stLink.qwID,
            gs_qwAdder);

        gs_qwAdder += 1;
        kUnlock(&(gs_stMutex));

        // 프로세서 소모를 늘리려고 추가한 코드
        for (j = 0; j < 30000; j++);
    }

    // 모든 태스크가 종료할 때까지 1초(100ms) 정도 대기
    qwTickCount = kGetTickCount();
    while ((kGetTickCount() - qwTickCount) < 1000)
    {
        kSchedule();
    }

    // 태스크 종료
    //kExitTask();
}

/**
 *  뮤텍스를 테스트하는 태스크 생성
 */
static void kTestMutex(const char* pcParameterBuffer)
{
    int i;

    gs_qwAdder = 1;

    // 뮤텍스 초기화
    kInitializeMutex(&gs_stMutex);

    for (i = 0; i < 3; i++)
    {
        // 뮤텍스를 테스트하는 태스크를 3개 생성
        kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kPrintNumberTask);
    }
    kPrintf("Wait Util %d Task End...\n", i);
    kGetCh();
}

/**
 *  태스크 2를 자신의 스레드로 생성하는 태스크
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
 *  스레드를 테스트하는 태스크 생성
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

// 난수를 발생시키기 위한 변수
static volatile QWORD gs_qwRandomValue = 0;

/**
 *  임의의 난수를 반환
 */
QWORD kRandom(void)
{
    gs_qwRandomValue = (gs_qwRandomValue * 412153 + 5571031) >> 16;
    return gs_qwRandomValue;
}

/**
 *  철자를 흘러내리게 하는 스레드
 */
static void kDropCharactorThread(void)
{
    int iX, iY;
    int i;
    char vcText[2] = { 0, };

    iX = kRandom() % CONSOLE_WIDTH;

    while (1)
    {
        // 잠시 대기함
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
 *  스레드를 생성하여 매트릭스 화면처럼 보여주는 프로세스
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

    // 키가 입력되면 프로세스 종료
    kGetCh();
}

/**
 *  매트릭스 화면을 보여줌
 */
static void kShowMatrix(const char* pcParameterBuffer)
{
    TCB* pstProcess;

    pstProcess = kCreateTask(TASK_FLAGS_PROCESS | TASK_FLAGS_LOW, (void*)0xE00000, 0xE00000,
        (QWORD)kMatrixProcess);
    if (pstProcess != NULL)
    {
        kPrintf("Matrix Process [0x%Q] Create Success\n");

        // 태스크가 종료 될 때까지 대기
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
    // 출력에 사용되는 변수
    CHARACTER* pstScreen = (CHARACTER*)CONSOLE_VIDEOMEMORYADDRESS;
    int vmemLabelPos = CONSOLE_WIDTH * 2;
    int vmemGraphPos = CONSOLE_WIDTH * 3;

    // 그래프 출력 버퍼와 관련된 변수
    int graphCursor = 0; // 루프 전체에서 유지해야 하는 값으로 현재 column의미
    short graph[1600];   // 10줄
    short line = 20;     // 10줄 (위와 동일하게 맞춰야하는 변수)
    short cells = (line) * 80;//모든 셀을 검은색 #으로 초기화
    for (int i = 0; i < cells; i++) {
        graph[i] = 0;
        pstScreen[vmemGraphPos + i].bCharactor = '#';
        pstScreen[vmemGraphPos + i].bAttribute = 0; // 검은색
    }

    // 프로그램 설명
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

    // runningTime 계산과 관련된 변수 
    int tasks[1025]; // 여유분 1
    short activetasks[1025];
    short activeCount = 0;
    long totalRunningTime = 0; // 총 누적 실행시간
    TCB* pstTCB;    // TCB 포인터

    // 작업시간 초기화
    for (int i = 0; i < TASK_MAXCOUNT; i++) {
        tasks[i] = -1; // 스케줄링된 프로세스 목록을 얻기 위해 필터 설정

        pstTCB = kGetTCBInTCBPool(i);
        if ((pstTCB->stLink.qwID >> 32) != 0) { // 스케줄링된 프로세스 목록 획득
            pstTCB->qwRunningTime = 0;          // 시간 초기화
            tasks[i] = 0;
            activetasks[activeCount++] = i;     // active목록 저장
        }
    }
    // 출력버퍼초기화

    // 전광판 효과 사용하기
    int offset = 0;
    while (1) {
        if (graphCursor == 79) return;
        // 스케줄러가 시작되면 더 이상 새로 시작되는 태스크는 없음으로 스캔 필요 없음

        // 수행 횟수 가져오기
        for (int i = 0; i < activeCount; i++) {
            int taskID = activetasks[i];
            pstTCB = kGetTCBInTCBPool(taskID);
            if ((pstTCB->stLink.qwID >> 32) != 0) { // 스케줄링된 프로세스 목록 획득
                tasks[taskID] += pstTCB->qwRunningTime;  // 이번 회차에 스케줄링된 횟수 누적
                totalRunningTime += pstTCB->qwRunningTime;
                pstTCB->qwRunningTime = 0;          // 횟수 초기화
            }
        }
        // 모든 수행 횟수
        pstScreen[80 * 23 + 70].bCharactor = totalRunningTime >= 100000 ? totalRunningTime / 100000 + '0' : ' ';
        pstScreen[80 * 23 + 71].bCharactor = totalRunningTime >= 10000 ? totalRunningTime / 10000 + '0' : ' ';
        pstScreen[80 * 23 + 72].bCharactor = totalRunningTime >= 1000 ? totalRunningTime / 1000 + '0' : ' ';
        pstScreen[80 * 23 + 73].bCharactor = totalRunningTime >= 100 ? (totalRunningTime % 1000) / 100 + '0' : ' ';
        pstScreen[80 * 23 + 74].bCharactor = totalRunningTime >= 10 ? (totalRunningTime % 100) / 10 + '0' : ' ';
        pstScreen[80 * 23 + 75].bCharactor = totalRunningTime % 10 + '0';
        // 수행 횟수 출력하기
        int vmemLabelCursor = vmemLabelPos; // label 출력위치 설정
        if (totalRunningTime != 0) {         //
            // 모든 태스크의 누적 수행 시간을 스캔한다
            int j = 0;
            for (int i = 0; i < activeCount && j < line; i++) {
                int taskID = activetasks[i];
                int runningTime = tasks[taskID];
                int percentage = ((runningTime * 100) / totalRunningTime); // 100% 비율로 출력
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
                else {// 라벨은 1줄까지만 출력, 나머지는 생략
                    pstScreen[vmemLabelCursor + 1].bCharactor = '.';
                    pstScreen[vmemLabelCursor + 2].bCharactor = '.';
                    pstScreen[vmemLabelCursor + 3].bCharactor = '.';
                }
                // 출력 편의를 위해, padding값 적용
                percentage = runningTime * line / totalRunningTime; // 라인 비율 저장
                for (int k = 0; k < percentage && (k + j) < line; k++) {
                    graph[graphCursor + (k + j) * 80] = COLOR(taskID);
                }
                j += percentage;
            }
            // 모자란 비율은 빈칸으로 채움taskID
            for (; j < line; j++) {
                graph[graphCursor + j * 80] = 0;
            }

            // 그래프 출력하기
            // 이번 회차의 그래프 버퍼의 가장 오래된 값을 가리키는 커서
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
 *  FPU를 테스트하는 태스크
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

    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    iOffset = (pstRunningTask->stLink.qwID & 0xFFFFFFFF) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT -
        (iOffset % (CONSOLE_WIDTH * CONSOLE_HEIGHT));

    // 루프를 무한히 반복하면서 동일한 계산을 수행
    while (1)
    {
        dValue1 = 1;
        dValue2 = 1;

        // 테스트를 위해 동일한 계산을 2번 반복해서 실행
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

        // 회전하는 바람개비를 표시
        pstScreen[iOffset].bCharactor = vcData[qwCount % 4];

        // 색깔 지정
        pstScreen[iOffset].bAttribute = (iOffset % 15) + 1;
    }
}

/**
 *  원주율(PIE)를 계산
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

    // 실수를 계산하는 태스크를 생성
    for (i = 0; i < 100; i++)
    {
        kCreateTask(TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, (QWORD)kFPUTestTask);
    }
}

/**
 *  동적 메모리 정보를 표시
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
 *  모든 블록 리스트의 블록을 순차적으로 할당하고 해제하는 테스트
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

        // 모든 블록을 할당 받아서 값을 채운 후 검사
        for (j = 0; j < (pstMemory->iBlockCountOfSmallestBlock >> i); j++)
        {
            pqwBuffer = kAllocateMemory(DYNAMICMEMORY_MIN_SIZE << i);
            if (pqwBuffer == NULL)
            {
                kPrintf("\nAllocation Fail\n");
                return;
            }

            // 값을 채운 후 다시 검사
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
            // 진행 과정을 . 으로 표시
            kPrintf(".");
        }

        kPrintf("\nFree: ");
        // 할당 받은 블록을 모두 반환
        for (j = 0; j < (pstMemory->iBlockCountOfSmallestBlock >> i); j++)
        {
            if (kFreeMemory((void*)(pstMemory->qwStartAddress +
                (DYNAMICMEMORY_MIN_SIZE << i) * j)) == FALSE)
            {
                kPrintf("\nFree Fail\n");
                return;
            }
            // 진행 과정을 . 으로 표시
            kPrintf(".");
        }
        kPrintf("\n");
    }
    kPrintf("Test Complete~!!!\n");
}

/**
 *  임의로 메모리를 할당하고 해제하는 것을 반복하는 태스크
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
        // 1KB ~ 32M까지 할당하도록 함
        do
        {
            qwMemorySize = ((kRandom() % (32 * 1024)) + 1) * 1024;
            pbAllocationBuffer = kAllocateMemory(qwMemorySize);

            // 만일 버퍼를 할당 받지 못하면 다른 태스크가 메모리를 사용하고 
            // 있을 수 있으므로 잠시 대기한 후 다시 시도
            if (pbAllocationBuffer == 0)
            {
                kSleep(1);
            }
        } while (pbAllocationBuffer == 0);

        kSPrintf(vcBuffer, "|Address: [0x%Q] Size: [0x%Q] Allocation Success",
            pbAllocationBuffer, qwMemorySize);
        // 자신의 ID를 Y 좌표로 하여 데이터를 출력
        kPrintStringXY(20, iY, vcBuffer);
        kSleep(200);

        // 버퍼를 반으로 나눠서 랜덤한 데이터를 똑같이 채움 
        kSPrintf(vcBuffer, "|Address: [0x%Q] Size: [0x%Q] Data Write...     ",
            pbAllocationBuffer, qwMemorySize);
        kPrintStringXY(20, iY, vcBuffer);
        for (i = 0; i < qwMemorySize / 2; i++)
        {
            pbAllocationBuffer[i] = kRandom() & 0xFF;
            pbAllocationBuffer[i + (qwMemorySize / 2)] = pbAllocationBuffer[i];
        }
        kSleep(200);

        // 채운 데이터가 정상적인지 다시 확인
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
 *  태스크를 여러 개 생성하여 임의의 메모리를 할당하고 해제하는 것을 반복하는 테스트
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
 *  하드 디스크의 정보를 표시
 */
static void kShowHDDInformation(const char* pcParameterBuffer)
{
    HDDINFORMATION stHDD;
    char vcBuffer[100];

    // 하드 디스크의 정보를 읽음
    if (kReadHDDInformation(TRUE, TRUE, &stHDD) == FALSE)
    {
        kPrintf("HDD Information Read Fail\n");
        return;
    }

    kPrintf("============ Primary Master HDD Information ============\n");

    // 모델 번호 출력
    kMemCpy(vcBuffer, stHDD.vwModelNumber, sizeof(stHDD.vwModelNumber));
    vcBuffer[sizeof(stHDD.vwModelNumber) - 1] = '\0';
    kPrintf("Model Number:\t %s\n", vcBuffer);

    // 시리얼 번호 출력
    kMemCpy(vcBuffer, stHDD.vwSerialNumber, sizeof(stHDD.vwSerialNumber));
    vcBuffer[sizeof(stHDD.vwSerialNumber) - 1] = '\0';
    kPrintf("Serial Number:\t %s\n", vcBuffer);

    // 헤드, 실린더, 실린더 당 섹터 수를 출력
    kPrintf("Head Count:\t %d\n", stHDD.wNumberOfHead);
    kPrintf("Cylinder Count:\t %d\n", stHDD.wNumberOfCylinder);
    kPrintf("Sector Count:\t %d\n", stHDD.wNumberOfSectorPerCylinder);

    // 총 섹터 수 출력
    kPrintf("Total Sector:\t %d Sector, %dMB\n", stHDD.dwTotalSectors,
        stHDD.dwTotalSectors / 2 / 1024);
}

/**
 *  하드 디스크에 파라미터로 넘어온 LBA 어드레스에서 섹터 수 만큼 읽음
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

    // 파라미터 리스트를 초기화하여 LBA 어드레스와 섹터 수 추출
    kInitializeParameter(&stList, pcParameterBuffer);
    if ((kGetNextParameter(&stList, vcLBA) == 0) ||
        (kGetNextParameter(&stList, vcSectorCount) == 0))
    {
        kPrintf("ex) readsector 0(LBA) 10(count)\n");
        return;
    }
    dwLBA = kAToI(vcLBA, 10);
    iSectorCount = kAToI(vcSectorCount, 10);

    // 섹터 수만큼 메모리를 할당 받아 읽기 수행
    pcBuffer = kAllocateMemory(iSectorCount * 512);
    if (kReadHDDSector(TRUE, TRUE, dwLBA, iSectorCount, pcBuffer) == iSectorCount)
    {
        kPrintf("LBA [%d], [%d] Sector Read Success~!!", dwLBA, iSectorCount);
        // 데이터 버퍼의 내용을 출력
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

                // 모두 두 자리로 표시하려고 16보다 작은 경우 0을 추가해줌
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
 *  하드 디스크에 파라미터로 넘어온 LBA 어드레스에서 섹터 수 만큼 씀
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

    // 파라미터 리스트를 초기화하여 LBA 어드레스와 섹터 수 추출
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

    // 버퍼를 할당 받아 데이터를 채움. 
    // 패턴은 4 바이트의 LBA 어드레스와 4 바이트의 쓰기가 수행된 횟수로 생성
    pcBuffer = kAllocateMemory(iSectorCount * 512);
    for (j = 0; j < iSectorCount; j++)
    {
        for (i = 0; i < 512; i += 8)
        {
            *(DWORD*)&(pcBuffer[j * 512 + i]) = dwLBA + j;
            *(DWORD*)&(pcBuffer[j * 512 + i + 4]) = s_dwWriteCount;
        }
    }

    // 쓰기 수행
    if (kWriteHDDSector(TRUE, TRUE, dwLBA, iSectorCount, pcBuffer) != iSectorCount)
    {
        kPrintf("Write Fail\n");
        return;
    }
    kPrintf("LBA [%d], [%d] Sector Write Success~!!", dwLBA, iSectorCount);

    // 데이터 버퍼의 내용을 출력
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

            // 모두 두 자리로 표시하려고 16보다 작은 경우 0을 추가해줌
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
 *  하드 디스크를 연결
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
 *  하드 디스크에 파일 시스템을 생성(포맷)
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
 *  파일 시스템 정보를 표시
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
 *  현재 디렉터리에 빈 파일을 생성
 */
static void kCreateFileInDirectory(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    DWORD dwCluster;
    int i;
    FILE* pstFile;

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
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
 *   현재 디렉터리에서 파일을 삭제
 */
static void kDeleteFileInDirectory(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
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

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
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

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
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
 *  현재 디렉터리의 파일 목록을 표시
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
    // 파일 시스템 정보를 얻음
    kGetFileSystemInformation(&stManager);

    // 현재 디렉터리를 엶
    pstDirectory = opendir(".");

    if (pstDirectory == NULL)
    {
        kPrintf("Current Directory Open Fail\n");
        return;
    }

    // 먼저 루프를 돌면서 디렉터리에 있는 파일의 개수와 전체 파일이 사용한 크기를 계산
    iTotalCount = 0;
    dwTotalByte = 0;
    dwUsedClusterCount = 0;
    while (1)
    {
        kMemSet(&stEntry, 0, sizeof(DIRECTORYENTRY));
        // 디렉터리에서 엔트리 하나를 읽음
        if(readdir((void*)&stEntry, pstDirectory) == FALSE)
        {
            break;
        }
        iTotalCount++;
        dwTotalByte += stEntry.dwFileSize;

        // 실제로 사용된 클러스터의 개수를 계산
        if (stEntry.dwFileSize == 0)
        {
            // 크기가 0이라도 클러스터 1개는 할당되어 있음
            dwUsedClusterCount++;
        }
        else
        {
            // 클러스터 개수를 올림하여 더함
            dwUsedClusterCount += (stEntry.dwFileSize +
                (FILESYSTEM_CLUSTERSIZE - 1)) / FILESYSTEM_CLUSTERSIZE;
        }
    }

    // 실제 파일의 내용을 표시하는 루프
    rewinddir(pstDirectory);
    iCount = 0;
    while (1)
    {
        kMemSet(&stEntry, 0, sizeof(DIRECTORYENTRY));
        // 디렉터리에서 엔트리 하나를 읽음
        if (readdir((void*)&stEntry, pstDirectory) == FALSE)
        {
            break;
        }

        // 전부 공백으로 초기화 한 후 각 위치에 값을 대입
        kMemSet(vcBuffer, ' ', sizeof(vcBuffer) - 1);
        vcBuffer[sizeof(vcBuffer) - 1] = '\0';

        // 파일 이름 삽입
        kMemCpy(vcBuffer, stEntry.d_name,
            kStrLen(stEntry.d_name));

        // 파일 길이 삽입
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

        // 파일의 시작 클러스터 삽입
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

    // 총 파일의 개수와 파일의 총 크기를 출력
    kPrintf("\t\tTotal File Count: %d\n", iTotalCount);
    kPrintf("\t\tTotal File Size: %d KByte (%d Cluster)\n", dwTotalByte,
        dwUsedClusterCount);

    // 남은 클러스터 수를 이용해서 여유 공간을 출력
    kPrintf("\t\tFree Space: %d KByte (%d Cluster)\n",
        (stManager.dwTotalClusterCount - dwUsedClusterCount) *
        FILESYSTEM_CLUSTERSIZE / 1024, stManager.dwTotalClusterCount -
        dwUsedClusterCount);

    // 디렉터리를 닫음
    closedir(pstDirectory);
}


/**
 *  파일을 생성하여 키보드로 입력된 데이터를 씀
 */
static void kWriteDataToFile(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    FILE* fp;
    int iEnterCount;
    BYTE bKey;

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    // 파일 생성
    fp = fopen(vcFileName, "w");
    if (fp == NULL)
    {
        kPrintf("%s File Open Fail\n", vcFileName);
        return;
    }

    // 엔터 키가 연속으로 3번 눌러질 때까지 내용을 파일에 씀
    iEnterCount = 0;
    while (1)
    {
        bKey = kGetCh();
        // 엔터 키이면 연속 3번 눌러졌는가 확인하여 루프를 빠져 나감
        if (bKey == KEY_ENTER)
        {
            iEnterCount++;
            if (iEnterCount >= 3)
            {
                break;
            }
        }
        // 엔터 키가 아니라면 엔터 키 입력 횟수를 초기화
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
 *  파일을 열어서 데이터를 읽음
 */
static void kReadDataFromFile(const char* pcParameterBuffer)
{
    PARAMETERLIST stList;
    char vcFileName[50];
    int iLength;
    FILE* fp;
    int iEnterCount;
    BYTE bKey;

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
    kInitializeParameter(&stList, pcParameterBuffer);
    iLength = kGetNextParameter(&stList, vcFileName);
    vcFileName[iLength] = '\0';
    if ((iLength > (FILESYSTEM_MAXFILENAMELENGTH - 1)) || (iLength == 0))
    {
        kPrintf("Too Long or Too Short File Name\n");
        return;
    }

    // 파일 생성
    fp = fopen(vcFileName, "r");
    if (fp == NULL)
    {
        kPrintf("%s File Open Fail\n", vcFileName);
        return;
    }

    // 파일의 끝까지 출력하는 것을 반복
    iEnterCount = 0;
    while (1)
    {
        if (fread(&bKey, 1, 1, fp) != 1)
        {
            break;
        }
        kPrintf("%c", bKey);

        // 만약 엔터 키이면 엔터 키 횟수를 증가시키고 20라인까지 출력했다면
        // 더 출력할지 여부를 물어봄
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

// 파일을 읽고 쓰는 속도를 측정
static void kTestPerformance(const char* pcParameterBuffer)
{
    FILE* pstFile;
    DWORD dwClusterTestFileSize;
    DWORD dwOneByteTestFileSize;
    QWORD qwLastTickCount;
    DWORD i;
    BYTE* pbBuffer;

    // 클러스터는 1MB까지 파일을 테스트
    dwClusterTestFileSize = 1024 * 1024;
    // 1바이트씩 읽고 쓰는 테스트는 시간이 많이 걸리므로 16KB만 테스트
    dwOneByteTestFileSize = 16 * 1024;

    // 테스트용 버퍼 메모리 할당
    pbBuffer = kAllocateMemory(dwClusterTestFileSize);

    if (pbBuffer == NULL)
    {
        kPrintf("Memory Allcate Fail\n");
        return;
    }

    // 버퍼를 초기화
    kMemSet(pbBuffer, 0, FILESYSTEM_CLUSTERSIZE);

    kPrintf("===================== File I/O Performance Test =====================\n");

    //==================================================================================
    // 클러스터 단위로 파일을 순차적으로 쓰는 테스트
    //==================================================================================
    kPrintf("1.Sequential Read/Write Test(Cluster Size)\n");

    // 기존의 테스트 파일을 제거하고 새로 만듦
    remove("performance.txt");
    pstFile = fopen("performance.txt", "w");
    if (pstFile == NULL)
    {
        kPrintf("File Open Fail\n");
        kFreeMemory(pbBuffer);
        return;
    }

    qwLastTickCount = kGetTickCount();
    // 클러스터 단위로 쓰는 테스트
    for (i = 0; i < (dwClusterTestFileSize / FILESYSTEM_CLUSTERSIZE); i++)
    {
        if (fwrite(pbBuffer, 1, FILESYSTEM_CLUSTERSIZE, pstFile) !=
            FILESYSTEM_CLUSTERSIZE)
        {
            kPrintf("Write Fail\n");
            // 파일을 닫고 메모리를 해제함
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // 시간 출력
    kPrintf("   Sequential Write(Cluster Size): %d ms\n", kGetTickCount() -
        qwLastTickCount);

    //===================================================================================
    // 클러스터 단위로 파일을 순차적으로 읽는 테스트
    //===================================================================================
    // 파일의 처음으로 이동
    fseek(pstFile, 0, SEEK_SET);

    qwLastTickCount = kGetTickCount();
    // 클러스터 단위로 읽는 테스트
    for (i = 0; i < (dwClusterTestFileSize / FILESYSTEM_CLUSTERSIZE); i++)
    {
        if (fread(pbBuffer, 1, FILESYSTEM_CLUSTERSIZE, pstFile) !=
            FILESYSTEM_CLUSTERSIZE)
        {
            kPrintf("Read Fail\n");
            // 파일을 닫고 메모리를 해제함
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // 시간 출력
    kPrintf("   Seqeuntial Read(Cluster Size): %d ms\n", kGetTickCount() -
        qwLastTickCount);

    //===================================================================================
    // 1바이트 단위로 파일을 순차적으로 쓰는 테스트
    //===================================================================================
    kPrintf("2.Sequential Read/Write Test(1 Byte)\n");

    // 기존의 테스트 파일을 제거하고 새로 만듦
    remove("performance.txt");
    pstFile = fopen("performance.txt", "w");
    if (pstFile == NULL)
    {
        kPrintf("File Open Fail\n");
        kFreeMemory(pbBuffer);
        return;
    }

    qwLastTickCount = kGetTickCount();
    // 1바이트 단위로 쓰는 테스트
    for (i = 0; i < dwOneByteTestFileSize; i++)
    {
        if (fwrite(pbBuffer, 1, 1, pstFile) != 1)
        {
            kPrintf("Write Fail\n");
            // 파일을 닫고 메모리를 해제함
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // 시간 출력
    kPrintf("   Sequential Write(1Byte): %d ms\n", kGetTickCount() -
        qwLastTickCount);
    //===================================================================================
    // 1바이트 단위로 파일을 순차적으로 읽는 테스트
    //===================================================================================
    // 파일의 처음으로 이동
    fseek(pstFile, 0, SEEK_SET);

    qwLastTickCount = kGetTickCount();
    // 1바이트 단위로 읽는 테스트
    for (i = 0; i < dwOneByteTestFileSize; i++)
    {
        if (fread(pbBuffer, 1, 1, pstFile) != 1)
        {
            kPrintf("Read Fail\n");
            // 파일을 닫고 메모리를 해제함
            fclose(pstFile);
            kFreeMemory(pbBuffer);
            return;
        }
    }
    // 시간 출력
    kPrintf("    Seqeuntial Read(1 Byte): %d ms\n", kGetTickCount() -
        qwLastTickCount);

    // 파일을 닫고 메모리를 해제함
    fclose(pstFile);
    kFreeMemory(pbBuffer);
}

// 파일 시스템의 캐시 버퍼에 있는 데이터를 모두 하드 디스크에 씀
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
    // 파라미터 리스트를 초기화하여 파일 이름을 추출
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

    // 절대 경로 이동
    if(vcFileName[0] == '/')
    {
    		// 루트 디렉터리 열기
    	pstCurrentDirectory = kOpenRootDirectory();
    	if (pstCurrentDirectory == NULL)
		{
			kPrintf("Root Directory Open Fail\n");
			return;
		}

    	i++;

    		// 루트 디렉터리로만 이동하는 경우
    	if(vcFileName[i] == '\0')
    	{
    		closedir(pstCurrentDirectory);
    		kPrintf("Directory Change Success\n");
    		return;
    	}
    }
    // 상대 경로 이동
    else
    {
		// 현재 디렉터리 열기
		pstCurrentDirectory = kOpenDirectory(".");
		if (pstCurrentDirectory == NULL)
		{
			kPrintf("Current Directory Open Fail\n");
			return;
		}

		// 이미 현재 디렉터리를 열었으므로 맨 앞에 '.'가 오면 무시한다
		if(vcFileName[0] == '.')
		{
			// 현재 디렉터리로만 이동하는 경우이면 아무런 동작을 하지 않는다.
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
			// 루트 디렉터리의 경우 부모 디렉터리가 자기 자신을 가리키므로
			// 이에 대한 예외처리를 해준다.
			else if(vcFileName[1] == '.' &&	(pstCurrentDirectory->stDirectoryHandle.dwStartClusterIndex == 1))
			{
				// 현재 디렉터리로만 이동하는 경우이면 아무런 동작을 하지 않는다.
				if(vcFileName[2] == '\0')
				{
					closedir(pstCurrentDirectory);
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

	// 하위 디렉터리 순차적으로 열기
	while(TRUE)
	{
		j = 0;
		kMemSet(tempBuffer, 0, sizeof(tempBuffer));
		while((vcFileName[i] != '/') && (vcFileName[i] != '\0'))
		{
			 tempBuffer[j] = vcFileName[i];
			 i++;
			 j++;
		}

		if(vcFileName[i] == '\0')
		{
			fileEnd = TRUE;
		}
		i++;
		tempBuffer[j] = '\0';
		pstMovedDirectory = cd(pstCurrentDirectory, tempBuffer);
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
	kPrintf("Directory Change Success\n");
}
