#include "FileSystem.h"
#include "HardDisk.h"
#include "DynamicMemory.h"
#include "Task.h"
#include "Utility.h"
#include "Console.h"

// 파일 시스템 자료구조
static FILESYSTEMMANAGER   gs_stFileSystemManager;
// 파일 시스템 임시 버퍼. 1 블럭 크기
static BYTE gs_vbTempBuffer[FILESYSTEM_SECTORSPERCLUSTER * 512];
// 세그먼트가 디스크에 쓰이는 블록 갯수
static DWORD SEGMENTBLOCKSIZE = (sizeof(SEGMENT) + FILESYSTEM_CLUSTERSIZE - 1) / FILESYSTEM_CLUSTERSIZE;
// 메모리에서는 총 2개의 세그먼트를 관리한다.
// 하나의 세그먼트가 가득차면 다른 세그먼트를 사용하며,
// 가득 찬 세그먼트를 디스크에 Write하는 태스크 호출을 요청한다.
static SEGMENT segments[2];
// 현재 작업중인 세그먼트 번호. 0번과 1번으로 나뉜다.
static int curWorkingSegment;
// 하드 디스크 제어에 관련된 함수 포인터 선언
fReadHDDInformation gs_pfReadHDDInformation = NULL;
fReadHDDSector gs_pfReadHDDSector = NULL;
fWriteHDDSector gs_pfWriteHDDSector = NULL;

/**
 *  하드 디스크에 파일 시스템을 생성
 */
BOOL kFormat(void)
{
	HDDINFORMATION* pstHDD;
	MBR* pstMBR;
	DWORD dwTotalSectorCount, dwRemainSectorCount;
	DWORD dwMaxClusterCount, dwClsuterCount;
	DWORD i;

	// 동기화 처리
	kLock(&(gs_stFileSystemManager.stMutex));

	//==========================================================================
	//  하드 디스크 정보를 읽어서 메타 영역의 크기와 클러스터의 개수를 계산
	//==========================================================================
	// 하드 디스크의 정보를 얻어서 하드 디스크의 총 섹터 수를 구함
	pstHDD = (HDDINFORMATION*)gs_vbTempBuffer;
	if (gs_pfReadHDDInformation(TRUE, TRUE, pstHDD) == FALSE)
	{
		// 동기화 처리
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}
	dwTotalSectorCount = pstHDD->dwTotalSectors;

	// 사용 가능한 디스크 섹터 수를 블록 크기로 나누어 실제 블록의 개수를 구함
	// MBR 영역 & 헤드 체크 포인트 영역 제외
	dwRemainSectorCount = dwTotalSectorCount - FILESYSTEM_SECTORSPERCLUSTER - 1;
	dwClsuterCount = dwRemainSectorCount / FILESYSTEM_SECTORSPERCLUSTER;
	//==========================================================================
	// 계산된 정보를 MBR에 덮어 쓰고, 루트 디렉터리 영역까지 모두 0으로 초기화하여
	// 파일 시스템을 생성
	//==========================================================================
	// MBR 영역 읽기
	if (gs_pfReadHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
	{
		// 동기화 처리
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// 파티션 정보와 파일 시스템 정보 설정
	pstMBR = (MBR*)gs_vbTempBuffer;
	kMemSet(pstMBR->vstPartition, 0, sizeof(pstMBR->vstPartition));
	pstMBR->dwSignature = FILESYSTEM_SIGNATURE;
	pstMBR->dwReservedSectorCount = 0;
	pstMBR->dwTotalClusterCount = dwClsuterCount;

	// MBR 영역에 1 섹터를 씀
	if (gs_pfWriteHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
	{
		// 동기화 처리
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// MBR 이후부터 루트 디렉터리까지 모두 0으로 초기화
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);

	// 맨 앞 체크포인트와 루트 디렉터리 블록 0으로 초기화
	for (i = 0; i < (FILESYSTEM_SECTORSPERCLUSTER * 2); i++)
	{
		if (gs_pfWriteHDDSector(TRUE, TRUE, i + 1, 1, gs_vbTempBuffer) == FALSE)
		{
			// 동기화 처리
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return FALSE;
		}
	}
	// 동기화 처리
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return TRUE;
}

/**
 *  파일 시스템을 초기화
 */
BOOL kInitializeFileSystem(void)
{
	// 자료구조 초기화와 동기화 객체 초기화
	kMemSet(&gs_stFileSystemManager, 0, sizeof(gs_stFileSystemManager));
	kInitializeMutex(&(gs_stFileSystemManager.stMutex));

	// 하드 디스크를 초기화
	if (kInitializeHDD() == TRUE)
	{
		// 초기화가 성공하면 함수 포인터를 하드 디스크용 함수로 설정
		gs_pfReadHDDInformation = kReadHDDInformation;
		gs_pfReadHDDSector = kReadHDDSector;
		gs_pfWriteHDDSector = kWriteHDDSector;
	}
	else
	{
		return FALSE;
	}

	// 파일 시스템 연결
	if (kMount() == FALSE)
	{
		return FALSE;
	}

	// 핸들을 위한 공간을 할당
	gs_stFileSystemManager.pstHandlePool = (FILE*)kAllocateMemory(
		FILESYSTEM_HANDLE_MAXCOUNT * sizeof(FILE));

	// 메모리 할당이 실패하면 하드 디스크가 인식되지 않은 것으로 설정
	if (gs_stFileSystemManager.pstHandlePool == NULL)
	{
		gs_stFileSystemManager.bMounted = FALSE;
		return FALSE;
	}

	// 핸들 풀을 모두 0으로 설정하여 초기화
	kMemSet(gs_stFileSystemManager.pstHandlePool, 0,
		FILESYSTEM_HANDLE_MAXCOUNT * sizeof(FILE));

	return TRUE;
}

//==============================================================================
//  저수준 함수(Low Level Function)
//==============================================================================
/**
 *  하드 디스크의 MBR을 읽어서 MINT 파일 시스템인지 확인
 *      MINT 파일 시스템이라면 파일 시스템에 관련된 각종 정보를 읽어서
 *      자료구조에 삽입
 */
BOOL kMount(void)
{
	MBR* pstMBR;

	// 동기화 처리
	kLock(&(gs_stFileSystemManager.stMutex));

	// MBR을 읽음
	if (gs_pfReadHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
	{
		// 동기화 처리
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// 시그너처를 확인하여 같다면 자료구조에 각 영역에 대한 정보 삽입
	pstMBR = (MBR*)gs_vbTempBuffer;
	if (pstMBR->dwSignature != FILESYSTEM_SIGNATURE)
	{
		// 동기화 처리
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// 파일 시스템 인식 성공
	gs_stFileSystemManager.bMounted = TRUE;

	// 각 영역의 시작 LBA 어드레스와 섹터 수를 계산
	gs_stFileSystemManager.dwReservedSectorCount = pstMBR->dwReservedSectorCount;

	// 예약된 영역 뒤에 헤드 체크 포인터가 존재한다.
	gs_stFileSystemManager.dwHeadCheckPointStartAddress = pstMBR->dwReservedSectorCount + 1;

	// 루트 디렉토리는 디스크 상의 정해진 위치에 있으므로, 일종의 메타데이터처럼 간주한다.
	gs_stFileSystemManager.dwRootDirStartAddress = pstMBR->dwReservedSectorCount + 1 + FILESYSTEM_SECTORSPERCLUSTER;
	gs_stFileSystemManager.dwDataAreaStartAddress = pstMBR->dwReservedSectorCount + 1 + (FILESYSTEM_SECTORSPERCLUSTER * 2);

	// 루트 디렉터리 영역은 데이터 블록으로 간주하지 않음.
	gs_stFileSystemManager.dwTotalClusterCount = pstMBR->dwTotalClusterCount - 1;

	// 다음에 디스크에 쓰기를 시작할 디스크 주소(섹터 단위 오프셋)
	gs_stFileSystemManager.dwNextAllocateClusterOffset = gs_stFileSystemManager.dwDataAreaStartAddress;

	// 메모리에서 사용하는 변수들 초기화
	kMemSet(segments, 0, sizeof(SEGMENT));
	curWorkingSegment = 0;
	segments[0].segmentStartOffset = gs_stFileSystemManager.dwDataAreaStartAddress;
	segments[1].segmentStartOffset = segments[0].segmentStartOffset +
		(SEGMENTBLOCKSIZE * FILESYSTEM_SECTORSPERCLUSTER);

	// 동기화 처리
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return TRUE;
}

/**
 *  파일 시스템에 연결된 하드 디스크의 정보를 반환
 */
BOOL kGetHDDInformation(HDDINFORMATION* pstInformation)
{
	BOOL bResult;

	// 동기화 처리
	kLock(&(gs_stFileSystemManager.stMutex));

	bResult = gs_pfReadHDDInformation(TRUE, TRUE, pstInformation);

	// 동기화 처리
	kUnlock(&(gs_stFileSystemManager.stMutex));

	return bResult;
}

/**
 *  데이터 영역의 오프셋에서 한 클러스터를 읽음
	디스크의 섹터 단위 오프셋을 입력받음
 */
static BOOL kReadCluster(DWORD dwOffset, BYTE* pbBuffer)
{
	// 데이터 영역의 시작 어드레스를 더함
	return gs_pfReadHDDSector(TRUE, TRUE, dwOffset, FILESYSTEM_SECTORSPERCLUSTER, pbBuffer);
}

/**
 *  데이터 영역의 오프셋에 한 클러스터를 씀
	디스크의 섹터 단위 오프셋을 입력받음
 */
static BOOL kWriteCluster(DWORD dwOffset, BYTE* pbBuffer)
{
	// 데이터 영역의 시작 어드레스를 더함
	return gs_pfWriteHDDSector(TRUE, TRUE, dwOffset, FILESYSTEM_SECTORSPERCLUSTER, pbBuffer);
}

// 루트 디렉터리를 읽음
static BOOL kReadRootDirectory(BYTE* pbBuffer)
{
	return kReadCluster(gs_stFileSystemManager.dwRootDirStartAddress, pbBuffer);
}

// 루트 디렉터리를 씀
static BOOL kWriteRootDirectory(BYTE* pbBuffer)
{
	return kWriteCluster(gs_stFileSystemManager.dwRootDirStartAddress, pbBuffer);
}

// 헤드 체크포인트를 읽음
static BOOL kReadHeadCheckPoint(BYTE* pbBuffer)
{
	return kReadCluster(gs_stFileSystemManager.dwHeadCheckPointStartAddress, pbBuffer);
}

// 헤드 체크포인트를 씀
static BOOL kWriteHeadCheckPoint(BYTE* pbBuffer)
{
	return kWriteCluster(gs_stFileSystemManager.dwHeadCheckPointStartAddress, pbBuffer);
}

// 디스크에서 아이노드를 읽음
static BOOL kReadINode(DWORD inumber, BYTE* pbBuffer)
{
	CHECKPOINT checkPoint;
	DWORD iNodeOffset;

	kMemSet(pbBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(pbBuffer);
	kMemCpy(&checkPoint, pbBuffer, sizeof(CHECKPOINT));

	iNodeOffset = checkPoint.iMap[inumber].iNodeOffset;
	kMemSet(pbBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadCluster(iNodeOffset, pbBuffer);
	return TRUE;
}

// segmentNum 세그먼트에 블록 1개 쓰기
// 아이노드 맵 블록인 경우 디스크 상 체크포인트를 갱신한다.
// 현재 세그먼트가 가득 차면 디스크에 쓰고 세그먼트를 전환한다.
// 인자로 받은 INODE는 최신 데이터로 갱신되었다고 가정한다.
// type - 데이터(0), 아이노드(1), 아이노드 맵 조각(2)
static BOOL kWriteSegment(DWORD segmentNum, DWORD type, INODE* inode, BYTE* pbBuffer)
{
	SEGMENT* curSegment;
	BYTE* curDataRegion;
	SEGMENTSUMMARYBLOCK* SS;
	DWORD* curTypeArr;
	// 바이트 단위 데이터 영역 인덱스
	DWORD curSegmentIdx;
	// 현재 세그먼트 데이터 블록 수
	DWORD curBlockCnt;
	CHECKPOINT checkPoint;

	curSegment = &segments[segmentNum];
	SS = &(curSegment->SS);
	curTypeArr = curSegment->dataBlockType;
	curBlockCnt = curSegment->curBlockCnt;
	curSegmentIdx = curBlockCnt * FILESYSTEM_CLUSTERSIZE;
	curDataRegion = curSegment->dataRegion;

	// 세그먼트에 블록 단위로 데이터 쓰기
	kMemCpy((char*)curDataRegion + curSegmentIdx, pbBuffer, FILESYSTEM_CLUSTERSIZE);
	// 쓴 데이터 블록의 타입 할당
	curTypeArr[curBlockCnt] = type;

	// 데이터 블록인 경우 세그먼트의 SS 블록을 갱신해준다.
	if (type == 0)
	{
		SS->inumbers[curBlockCnt] = inode->iNumber;
		SS->offsets[curBlockCnt] = inode->curDataBlocks - 1;
	}

	// 아이노드 맵 블록인 경우 디스크 상 체크포인트를 갱신한다.
	if (type == 2)
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadHeadCheckPoint(gs_vbTempBuffer);
		kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));
		checkPoint.iMap[inode->iNumber].iNumber = inode->iNumber;
		checkPoint.iMap[inode->iNumber].iNodeOffset = curSegment->segmentStartOffset
			+ (curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &checkPoint, sizeof(CHECKPOINT));
		kWriteHeadCheckPoint(gs_vbTempBuffer);
	}

	// 세그먼트에 저장된 블록 수 증가
	curSegment->curBlockCnt++;

	// 현재 세그먼트가 가득 찼다면 세그먼트를 디스크에 쓰고 비운다.
	// 이후 작업 세그먼트를 다른 세그먼트로 교체한다.
	if (curSegment->curBlockCnt >= FILESYSTEM_MAXSEGMENTCLUSTERCOUNT)
	{
		// 다른 세그먼트 초기화
		kMemSet(&segments[!curWorkingSegment], 0, sizeof(SEGMENT));
		// 다른 세그먼트 시작 주소 할당
		segments[!curWorkingSegment].segmentStartOffset = segments[curWorkingSegment].nextSegmentOffset;
		// 다른 세그먼트의 다음 세그먼트 주소 할당
		segments[!curWorkingSegment].nextSegmentOffset = segments[!curWorkingSegment].segmentStartOffset +
			(SEGMENTBLOCKSIZE * FILESYSTEM_SECTORSPERCLUSTER);
		// 세그먼트를 디스크에 통째로 Write
		kWriteSegmentToDisk(curWorkingSegment);
		// 세그먼트 교체
		curWorkingSegment = !curWorkingSegment;
	}

	return TRUE;
}

// 세그먼트를 디스크에 기록하기
// 체크포인트를 갱신해준다.
static BOOL kWriteSegmentToDisk(DWORD segmentNum)
{
	SEGMENT* segment = &(segments[segmentNum]);
	CHECKPOINT* checkPoint;
	// 현재까지 디스크에 쓰인 세그먼트 구조체의 블록 수
	DWORD curWriteSegmentBlocks = 0;

	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	checkPoint = (CHECKPOINT*)gs_vbTempBuffer;
	// 가장 최근 세그먼트 시작 위치 갱신
	checkPoint->tailSegmentOffset = segment->segmentStartOffset;
	kWriteHeadCheckPoint(gs_vbTempBuffer);

	// 세그먼트 한 블록씩 디스크에 기록
	// 세그먼트에 실제로 할당된 데이터 블록 갯수와 상관 없이 무조건 최대 세그먼트 크기만큼 기록
	while(curWriteSegmentBlocks < SEGMENTBLOCKSIZE)
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		if (curWriteSegmentBlocks == SEGMENTBLOCKSIZE - 1)
		{
			kMemCpy(gs_vbTempBuffer, (char*)segment + (curWriteSegmentBlocks * FILESYSTEM_CLUSTERSIZE),
				SEGMENTBLOCKSIZE * FILESYSTEM_CLUSTERSIZE - sizeof(SEGMENT));
		}
		else
		{
			kMemCpy(gs_vbTempBuffer, (char*)segment + (curWriteSegmentBlocks * FILESYSTEM_CLUSTERSIZE), FILESYSTEM_CLUSTERSIZE);
		}

		// 블럭 단위로 디스크에 Write
		kWriteCluster(gs_stFileSystemManager.dwNextAllocateClusterOffset, gs_vbTempBuffer);
		gs_stFileSystemManager.dwNextAllocateClusterOffset += FILESYSTEM_SECTORSPERCLUSTER;
		curWriteSegmentBlocks += 1;
	}

	return TRUE;
}

// 빈 아이노드 검색
// 빈 아이노드에 해당하는 아이노드 번호 반환
// 빈 아이노드를 찾지 못했다면 0 반환
static DWORD kFindFreeINode(void)
{
	DWORD i;
	CHECKPOINT checkPoint;
	INODEMAPBLOCK* inodeMap;
	INODE* curInode;

	// 파일 시스템을 인식하지 못했으면 실패
	if (gs_stFileSystemManager.bMounted == FALSE)
	{
		return -1;
	}

	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));
	inodeMap = checkPoint.iMap;

	// 루프를 돌면서 빈 아이노드 검색
	for (i = 1; i <= 128; i++)
	{
		// 아이넘버가 0번이면 빈 아이노드이다.
		if (inodeMap[i].iNumber == 0)
		{
			return i;
		}
	}

	return 0;
}

/**
 *  루트 디렉터리에서 빈 디렉터리 엔트리를 반환
	디렉터리 엔트리의 아이넘버가 0이면 빈 엔트리이다.
 */
static int kFindFreeDirectoryEntry(void)
{
	DIRECTORYENTRY* pstEntry;
	int i;

	// 파일 시스템을 인식하지 못했으면 실패
	if (gs_stFileSystemManager.bMounted == FALSE)
	{
		return -1;
	}

	// 루트 디렉터리를 읽음
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return -1;
	}

	// 루트 디렉터리 안에서 루프를 돌면서 빈 엔트리, 즉 아이노드 번호로
	// 0을 가지는 디렉터리 엔트리를 검색
	pstEntry = (DIRECTORYENTRY*)gs_vbTempBuffer;
	for (i = 0; i < FILESYSTEM_MAXDIRECTORYENTRYCOUNT; i++)
	{
		if (pstEntry[i].iNumber == 0)
		{
			return i;
		}
	}
	return -1;
}

/**
 *  루트 디렉터리의 해당 인덱스에 디렉터리 엔트리를 설정
 */
static BOOL kSetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry)
{
	DIRECTORYENTRY* pstRootEntry;

	// 파일 시스템을 인식하지 못했거나 인덱스가 올바르지 않으면 실패
	if ((gs_stFileSystemManager.bMounted == FALSE) ||
		(iIndex < 0) || (iIndex >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
	{
		return FALSE;
	}

	// 루트 디렉터리를 읽음
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return FALSE;
	}

	// 루트 디렉터리에 있는 해당 데이터를 갱신
	pstRootEntry = (DIRECTORYENTRY*)gs_vbTempBuffer;
	kMemCpy(pstRootEntry + iIndex, pstEntry, sizeof(DIRECTORYENTRY));

	// 루트 디렉터리에 씀
	if (kWriteRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return FALSE;
	}
	return TRUE;
}

/**
 *  루트 디렉터리의 해당 인덱스에 위치하는 디렉터리 엔트리를 반환
 */
static BOOL kGetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry)
{
	DIRECTORYENTRY* pstRootEntry;

	// 파일 시스템을 인식하지 못했거나 인덱스가 올바르지 않으면 실패
	if ((gs_stFileSystemManager.bMounted == FALSE) ||
		(iIndex < 0) || (iIndex >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
	{
		return FALSE;
	}

	// 루트 디렉터리를 읽음
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return FALSE;
	}

	// 루트 디렉터리에 있는 해당 데이터를 갱신
	pstRootEntry = (DIRECTORYENTRY*)gs_vbTempBuffer;
	kMemCpy(pstEntry, pstRootEntry + iIndex, sizeof(DIRECTORYENTRY));
	return TRUE;
}

/**
 *  루트 디렉터리에서 파일 이름이 일치하는 엔트리를 찾아서 인덱스를 반환
 */
 // pstEntry에 해당 엔트리 정보를 저장하고 인덱스 반환
static int kFindDirectoryEntry(const char* pcFileName, DIRECTORYENTRY* pstEntry)
{
	DIRECTORYENTRY* pstRootEntry;
	int i;
	int iLength;

	// 파일 시스템을 인식하지 못했으면 실패
	if (gs_stFileSystemManager.bMounted == FALSE)
	{
		return -1;
	}

	// 루트 디렉터리를 읽음
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return -1;
	}

	iLength = kStrLen(pcFileName);
	// 루트 디렉터리 안에서 루프를 돌면서 파일 이름이 일치하는 엔트리를 반환
	pstRootEntry = (DIRECTORYENTRY*)gs_vbTempBuffer;
	for (i = 0; i < FILESYSTEM_MAXDIRECTORYENTRYCOUNT; i++)
	{
		if (kMemCmp(pstRootEntry[i].vcFileName, pcFileName, iLength) == 0)
		{
			kMemCpy(pstEntry, pstRootEntry + i, sizeof(DIRECTORYENTRY));
			return i;
		}
	}
	return -1;
}

/**
 *  파일 시스템의 정보를 반환
 */
void kGetFileSystemInformation(FILESYSTEMMANAGER* pstManager)
{
	kMemCpy(pstManager, &gs_stFileSystemManager, sizeof(gs_stFileSystemManager));
}

//==============================================================================
//  고수준 함수(High Level Function)
//==============================================================================
/**
 *  비어있는 핸들을 할당
 */
static void* kAllocateFileDirectoryHandle(void)
{
	int i;
	FILE* pstFile;

	// 핸들 풀(Handle Pool)을 모두 검색하여 비어있는 핸들을 반환
	pstFile = gs_stFileSystemManager.pstHandlePool;
	for (i = 0; i < FILESYSTEM_HANDLE_MAXCOUNT; i++)
	{
		// 비어있다면 반환
		if (pstFile->bType == FILESYSTEM_TYPE_FREE)
		{
			pstFile->bType = FILESYSTEM_TYPE_FILE;
			return pstFile;
		}

		// 다음으로 이동
		pstFile++;
	}

	return NULL;
}

/**
 *  사용한 핸들을 반환
 */
static void kFreeFileDirectoryHandle(FILE* pstFile)
{
	// 전체 영역을 초기화
	kMemSet(pstFile, 0, sizeof(FILE));

	// 비어있는 타입으로 설정
	pstFile->bType = FILESYSTEM_TYPE_FREE;
}

/**
 *  파일을 생성
 */
static BOOL kCreateFile(const char* pcFileName, DIRECTORYENTRY* pstEntry,
	int* piDirectoryEntryIndex)
{
	DWORD iNumber;
	INODEMAPBLOCK inodeMap;
	INODE newInode;
	SEGMENT* curSegment;

	// 빈 아이노드 검색
	iNumber = kFindFreeINode();
	if (iNumber == 0)
	{
		return FALSE;
	}

	// 빈 디렉터리 엔트리를 검색
	*piDirectoryEntryIndex = kFindFreeDirectoryEntry();
	if (*piDirectoryEntryIndex == -1)
	{
		return FALSE;
	}

	// 디렉터리 엔트리를 설정
	kMemCpy(pstEntry->vcFileName, pcFileName, kStrLen(pcFileName) + 1);
	pstEntry->iNumber = iNumber;
	pstEntry->dwFileSize = 0;

	// 디렉터리 엔트리를 등록
	if (kSetDirectoryEntryData(*piDirectoryEntryIndex, pstEntry) == FALSE)
	{
		return FALSE;
	}

	// 아이노드 할당
	kMemSet(&newInode, 0, sizeof(INODE));
	newInode.iNumber = iNumber;

	// 아이노드 맵 할당
	inodeMap.iNumber = iNumber;
	inodeMap.iNodeOffset = curSegment->segmentStartOffset +
		(curSegment->curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

	// 갱신 후 아이노드 정보를 세그먼트에 저장
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kMemCpy(gs_vbTempBuffer, &newInode, sizeof(INODE));
	kWriteSegment(curWorkingSegment, 1, &newInode, gs_vbTempBuffer);

	// 아이노드 맵 갱신 정보를 세그먼트에 저장
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kMemCpy(gs_vbTempBuffer, &inodeMap, sizeof(INODEMAPBLOCK));
	kWriteSegment(curWorkingSegment, 2, &inodeMap, gs_vbTempBuffer);

	return TRUE;
}

/**
 *  파일을 열거나 생성
 */
FILE* kOpenFile(const char* pcFileName, const char* pcMode)
{
	DIRECTORYENTRY stEntry;
	int iDirectoryEntryOffset;
	int iFileNameLength;
	INODE inode;
	CHECKPOINT checkPoint;
	INODEMAPBLOCK iNodeMap;
	DWORD dwSecondCluster;
	FILE* pstFile;

	// 파일 이름 검사
	iFileNameLength = kStrLen(pcFileName);
	if ((iFileNameLength > (sizeof(stEntry.vcFileName) - 1)) ||
		(iFileNameLength == 0))
	{
		return NULL;
	}

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// open을 수행하기 전 동기화를 위해 현재 세그먼트를 디스크에 write
	kWriteSegmentToDisk(curWorkingSegment);

	// 디스크로부터 체크포인트 읽기
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));

	//==========================================================================
	// 파일이 먼저 존재하는가 확인하고, 없다면 옵션을 보고 파일을 생성
	//==========================================================================
	iDirectoryEntryOffset = kFindDirectoryEntry(pcFileName, &stEntry);
	// 파일이 존재하지 않는 경우
	if (iDirectoryEntryOffset == -1)
	{
		// 파일이 없다면 읽기(r, r+) 옵션은 실패
		if (pcMode[0] == 'r')
		{
			// 동기화
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return NULL;
		}

		// 나머지 옵션들은 파일을 생성
		if (kCreateFile(pcFileName, &stEntry, &iDirectoryEntryOffset) == FALSE)
		{
			// 동기화
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return NULL;
		}

		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadINode(stEntry.iNumber, gs_vbTempBuffer);
		kMemCpy(&inode, gs_vbTempBuffer, sizeof(INODE));
	}
	//==========================================================================
	// 파일의 내용을 비워야 하는 옵션이면 파일에 연결된 클러스터를 모두 제거하고
	// 파일 크기를 0으로 설정
	//==========================================================================
	else if (pcMode[0] == 'w')
	{
		// LFS에서는 데이터를 앞으로만 쓴다.
		// 따라서 아이노드 데이터 블록 포인터를 모두 0으로 초기화 한
		// 아이노드 세그먼트에 갱신 후 쓰기
		kMemSet(&inode, 0, sizeof(INODE));
		inode.iNumber = stEntry.iNumber;

		// 아이노드 맵 정보 갱신
		iNodeMap.iNumber = inode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset
			+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// 세그먼트에 아이노드 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &inode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &inode, gs_vbTempBuffer);

		// 세그먼트에 아이노드 맵 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &inode.iNumber, gs_vbTempBuffer);

		// 파일의 내용이 모두 비워졌으므로, 크기를 0으로 설정
		stEntry.dwFileSize = 0;
		if (kSetDirectoryEntryData(iDirectoryEntryOffset, &stEntry) == FALSE)
		{
			// 동기화
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return NULL;
		}
	}
	else
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadINode(stEntry.iNumber, gs_vbTempBuffer);
		kMemCpy(&inode, gs_vbTempBuffer, sizeof(INODE));
	}
	//==========================================================================
	// 파일 핸들을 할당 받아 데이터를 설정한 후 반환
	//==========================================================================
	// 파일 핸들을 할당 받아 데이터 설정
	pstFile = kAllocateFileDirectoryHandle();
	if (pstFile == NULL)
	{
		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;
	}

	// 파일 핸들에 파일 정보를 설정
	pstFile->bType = FILESYSTEM_TYPE_FILE;
	pstFile->stFileHandle.iDirectoryEntryOffset = iDirectoryEntryOffset;
	pstFile->stFileHandle.iNumber = inode.iNumber;
	pstFile->stFileHandle.dwFileSize = inode.size;
	pstFile->stFileHandle.dwStartFileOffset = inode.dataBlockOffset[0];
	pstFile->stFileHandle.dwCurrentFileOffset = 0;
	pstFile->stFileHandle.dwCurrentOffset = 0;

	// 만약 추가 옵션(a)이 설정되어 있으면, 파일의 끝으로 이동
	if (pcMode[0] == 'a')
	{
		kSeekFile(pstFile, 0, FILESYSTEM_SEEK_END);
	}

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return pstFile;
}

/**
 *  파일을 읽어 버퍼로 복사
	모든 데이터는 블록 단위로 저장되어 있음.
 */
DWORD kReadFile(void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile)
{
	DWORD dwTotalCount;
	DWORD dwReadCount;
	DWORD dwCopySize;
	DWORD dwOffsetInCluster;
	DWORD iNumber;
	FILEHANDLE* pstFileHandle;
	DWORD currentDataBlockIdx;
	INODE iNode;
	DWORD i, j;
	DWORD actualDataSize = 0;

	// 핸들이 파일 타입이 아니면 실패
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return 0;
	}

	// 동기화를 위해 세그먼트 비우기
	kWriteSegmentToDisk(curWorkingSegment);
	pstFileHandle = &(pstFile->stFileHandle);

	iNumber = pstFileHandle->iNumber;
	kReadINode(iNumber, gs_vbTempBuffer);
	kMemCpy(&iNode, gs_vbTempBuffer, sizeof(INODE));

	// 파일이 비었거나 이미 파일의 끝이면 종료
	if ((pstFileHandle->dwFileSize == 0) || (
		(pstFileHandle->dwCurrentFileOffset == (iNode.curDataBlocks - 1))
		&& (pstFileHandle->dwCurrentOffset == iNode.dataBlockActualDataSize[pstFileHandle->dwCurrentFileOffset])
		))
	{
		return 0;
	}

	// 현재 I/O 수행 중인 파일의 데이터 블록
	currentDataBlockIdx = pstFileHandle->dwCurrentFileOffset;
	// 현재 데이터 블록에서의 오프셋
	dwOffsetInCluster = pstFileHandle->dwCurrentOffset;
	// 실제로 읽을 데이터 크기
	actualDataSize += (iNode.dataBlockActualDataSize[currentDataBlockIdx]
		- dwOffsetInCluster);

	for (i = currentDataBlockIdx + 1; i < 12; i++)
	{
		actualDataSize += iNode.dataBlockActualDataSize[i];
	}

	// 파일 끝과 비교해서 실제로 읽을 수 있는 값을 계산 (바이트 단위)
	dwTotalCount = MIN(dwSize * dwCount, actualDataSize);

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// 계산된 값만큼 다 읽을 때까지 반복
	dwReadCount = 0;

	while (dwReadCount != dwTotalCount)
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadCluster(iNode.dataBlockOffset[currentDataBlockIdx], gs_vbTempBuffer);

		// 처음 시작 시 데이터 블록의 중간부터 읽는 경우
		if (pstFileHandle->dwCurrentOffset != 0)
		{
			dwCopySize = MIN(iNode.dataBlockActualDataSize[currentDataBlockIdx]
				- pstFileHandle->dwCurrentOffset, dwTotalCount - dwReadCount);
		}
		// 데이터 블록의 처음부터 읽는 경우
		else
		{
			dwCopySize = MIN(iNode.dataBlockActualDataSize[currentDataBlockIdx], dwTotalCount - dwReadCount);
		}

		// 버퍼에 읽은 데이터 출력
		kMemCpy((char*)pvBuffer + dwReadCount, gs_vbTempBuffer, dwCopySize);
		dwReadCount += dwCopySize;
		currentDataBlockIdx++;

		// 데이터 블록의 끝까지 읽은 경우
		if (dwCopySize == iNode.dataBlockActualDataSize[currentDataBlockIdx])
		{
			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
		}
		// 데이터 블록의 중간까지만 읽은 경우
		// 읽기가 끝난 경우이다.
		else
		{
			pstFileHandle->dwCurrentOffset += dwCopySize;
		}
	}

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));

	// 읽은 레코드 수를 반환
	return (dwReadCount / dwSize);
}

/**
 *  루트 디렉터리에서 디렉터리 엔트리 값을 갱신
 */
static BOOL kUpdateDirectoryEntry(FILEHANDLE* pstFileHandle)
{
	DIRECTORYENTRY stEntry;

	// 디렉터리 엔트리 검색
	if ((pstFileHandle == NULL) ||
		(kGetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry)
			== FALSE))
	{
		return FALSE;
	}

	// 파일 크기와 시작 클러스터를 변경
	stEntry.dwFileSize = pstFileHandle->dwFileSize;
	stEntry.iNumber = pstFileHandle->iNumber;

	// 변경된 디렉터리 엔트리를 설정
	if (kSetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry)
		== FALSE)
	{
		return FALSE;
	}
	return TRUE;
}

/**
 *  버퍼의 데이터를 파일에 씀
	블록 단위로 세그먼트에 데이터를 씀
 */
DWORD kWriteFile(const void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile)
{
	DWORD dwWriteCount;
	DWORD dwTotalCount;
	DWORD dwOffsetInCluster;
	DWORD dwCopySize;
	DWORD dwAllocatedClusterIndex;
	DWORD dwNextClusterIndex;
	DWORD iNumber;
	INODE iNode;
	CHECKPOINT checkPoint;
	INODEMAPBLOCK iNodeMap;
	FILEHANDLE* pstFileHandle;
	DIRECTORYENTRY stEntry;
	INODE tempNode;
	BYTE tempFrontData[FILESYSTEM_SECTORSPERCLUSTER * 512];
	BYTE tempBackData[FILESYSTEM_SECTORSPERCLUSTER * 512];
	DWORD tempFrontSize;
	DWORD tempBackSize;
	DWORD startFileOffset;
	DWORD blocksToMove;
	DWORD i;

	// 핸들이 파일 타입이 아니면 실패
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return 0;
	}
	pstFileHandle = &(pstFile->stFileHandle);

	kWriteSegmentToDisk(curWorkingSegment);

	// 파일에 해당하는 디렉토리 엔트리 읽어오기
	kGetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry);

	kMemSet(tempFrontData, 0, sizeof(tempFrontData));
	kMemSet(tempBackData, 0, sizeof(tempBackData));

	// 파일 아이노드 읽어오기
	iNumber = pstFileHandle->iNumber;
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadINode(iNumber, gs_vbTempBuffer);
	kMemCpy(&iNode, gs_vbTempBuffer, sizeof(INODE));
	kMemCpy(&tempNode, &iNode, sizeof(INODE));

	// 파일에 쓸 총 바이트 수
	dwTotalCount = dwSize * dwCount;

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));
	// 다 쓸 때까지 반복
	dwWriteCount = 0;

	// 파일이 비었거나 파일의 끝(SEEK_END)이면 새로운 데이터 블록 추가
	if (iNode.size == 0 || ((pstFileHandle->dwCurrentFileOffset == (iNode.curDataBlocks - 1))
			&& (pstFileHandle->dwCurrentOffset == iNode.dataBlockActualDataSize[iNode.curDataBlocks - 1])))
	{
		while (dwWriteCount != dwTotalCount)
		{
			// 파일에 쓸 데이터 중 1블럭 크기 추출
			dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE, dwTotalCount - dwWriteCount);

			// 아이노드 갱신
			iNode.dataBlockOffset[iNode.curDataBlocks] = segments[curWorkingSegment].segmentStartOffset
				+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
			iNode.dataBlockActualDataSize[iNode.curDataBlocks] = dwCopySize;
			iNode.size += dwCopySize;
			iNode.curDataBlocks++;

			// 세그먼트에 새로운 데이터 블록 쓰기
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, (char*)pvBuffer + dwWriteCount, dwCopySize);
			kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

			// 아이노드 맵 정보 갱신
			iNodeMap.iNumber = iNode.iNumber;
			iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
				(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

			// 세그먼트에 아이노드 쓰기
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
			kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

			// 세그먼트에 아이노드 맵 쓰기
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
			kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

			// 파일 핸들 갱신
			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
			pstFileHandle->dwFileSize += dwCopySize;

			dwWriteCount += dwCopySize;
		}
	}
	// 기존 파일의 중간에서부터 데이터를 쓰는 경우
	// 현재 블록을 '처음부터 중간' / '중간부터 끝까지'의 블록으로 나눈 후,
	// '처음부터 중간' 블록의 남은 공간부터 입력된 데이터를 쓰기 시작
	// '중간부터 끝까지' 데이터는 별도의 데이터 블록으로 추가한다.
	// 체크포인트의 아이노드 맵, 세그먼트 요약 블록, 아이노드를 갱신해야 한다.
	else
	{
		startFileOffset = pstFileHandle->dwCurrentFileOffset;
		// 현재 블록 읽기
		kReadCluster(iNode.dataBlockOffset[startFileOffset], gs_vbTempBuffer);

		// 현재 블록을 '처음부터 중간' / '중간부터 끝까지'으로 나누기
		tempFrontSize = pstFileHandle->dwCurrentOffset;
		tempBackSize = iNode.dataBlockActualDataSize[startFileOffset] - tempFrontSize;

		kMemCpy(tempFrontData, gs_vbTempBuffer, tempFrontSize);
		kMemCpy(tempBackData, (char*)gs_vbTempBuffer + tempFrontSize, tempBackSize);

		// '처음부터 중간' 블록에 추가로 쓸 블록
		dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE - tempFrontSize, dwTotalCount - dwWriteCount);

		// 아이노드 갱신
		iNode.dataBlockOffset[startFileOffset] = segments[curWorkingSegment].segmentStartOffset
			+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
		iNode.dataBlockActualDataSize[startFileOffset] += dwCopySize;
		iNode.size += dwCopySize;

		// '처음부터 중간' 블록에 파일에 쓸 데이터를 추가한 블록 세그먼트에 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, tempFrontData, tempFrontSize);
		kMemCpy((char*)gs_vbTempBuffer + tempFrontSize, pvBuffer, dwCopySize);
		kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

		// 아이노드 맵 정보 갱신
		iNodeMap.iNumber = iNode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
			(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// 세그먼트에 아이노드 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

		// 세그먼트에 아이노드 맵 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

		// 파일 핸들 갱신
		if (startFileOffset == 0)
		{
			pstFileHandle->dwStartFileOffset = iNode.dataBlockOffset[startFileOffset];
		}
		pstFileHandle->dwCurrentFileOffset++;
		pstFileHandle->dwCurrentOffset = 0;
		pstFileHandle->dwFileSize += dwCopySize;

		dwWriteCount += dwCopySize;

		blocksToMove = 0;
		// 파일에 쓸 부분 중 남은 부분은 새로운 데이터 블록을 할당받음
		while (dwWriteCount != dwTotalCount)
		{
			// 옮겨야 할 데이터 블록 포인터 수
			blocksToMove++;

			// 파일에 쓸 데이터 중 1블럭 크기 추출
			dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE, dwTotalCount - dwWriteCount);

			// 아이노드 갱신
			iNode.dataBlockOffset[pstFileHandle->dwCurrentFileOffset] = segments[curWorkingSegment].segmentStartOffset
				+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
			iNode.dataBlockActualDataSize[iNode.curDataBlocks] += dwCopySize;
			iNode.size += dwCopySize;
			iNode.curDataBlocks++;

			// 세그먼트에 데이터 블록 쓰기
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, (char*)pvBuffer + dwWriteCount, dwCopySize);
			kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

			// 아이노드 맵 정보 갱신
			iNodeMap.iNumber = iNode.iNumber;
			iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
				(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

			// 세그먼트에 아이노드 쓰기
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
			kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

			// 세그먼트에 아이노드 맵 쓰기
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
			kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

			// 파일 핸들 갱신
			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
			pstFileHandle->dwFileSize += dwCopySize;

			dwWriteCount += dwCopySize;
		}

		// '중간부터 끝까지' 블록 데이터 세그먼트에 쓰기
		// 아이노드 갱신
		iNode.dataBlockOffset[pstFileHandle->dwCurrentFileOffset] = segments[curWorkingSegment].segmentStartOffset
			+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
		iNode.dataBlockActualDataSize[pstFileHandle->dwCurrentFileOffset] = tempBackSize;

		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, tempBackData, tempBackSize);
		kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

		// 아이노드 맵 정보 갱신
		iNodeMap.iNumber = iNode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
			(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// 세그먼트에 아이노드 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

		// 세그먼트에 아이노드 맵 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

		// 파일 핸들 갱신
		pstFileHandle->dwCurrentFileOffset++;
		pstFileHandle->dwCurrentOffset = 0;

		// 추가된 데이터 블록으로 인해 뒤로 밀린 데이터 블록들 복구
		for (i = 0; i < blocksToMove; i++)
		{
			iNode.dataBlockOffset[startFileOffset + 1 + blocksToMove + i] =
				tempNode.dataBlockOffset[startFileOffset + 1 + i];
			iNode.dataBlockActualDataSize[startFileOffset + 1 + blocksToMove + i] =
				tempNode.dataBlockActualDataSize[startFileOffset + 1 + i];

			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
		}

		// 아이노드 맵 정보 갱신
		iNodeMap.iNumber = iNode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
			(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// 세그먼트에 아이노드 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

		// 세그먼트에 아이노드 맵 쓰기
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);
	}
	//==========================================================================
   // 파일 크기가 변했다면 루트 디렉터리에 있는 디렉터리 엔트리 정보를 갱신
  //==========================================================================

	pstFileHandle->dwCurrentFileOffset--;
	pstFileHandle->dwCurrentOffset = iNode.dataBlockActualDataSize[pstFileHandle->dwCurrentFileOffset];

	if (pstFileHandle->dwFileSize > tempNode.size)
	{
		kUpdateDirectoryEntry(pstFileHandle);
	}

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));

	// 쓴 레코드 수를 반환
	return (dwWriteCount / dwSize);
}

/**
 *  파일 포인터의 위치를 이동
 */
int kSeekFile(FILE* pstFile, int iOffset, int iOrigin)
{
	DWORD dwRealOffset;
	DWORD i;
	DWORD tempOffset;
	DWORD curEntireFileOffset = 0;
	FILEHANDLE* pstFileHandle;
	INODE iNode;

	// 핸들이 파일 타입이 아니면 나감
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return 0;
	}
	pstFileHandle = &(pstFile->stFileHandle);

	kWriteSegmentToDisk(curWorkingSegment);
	// 파일 아이노드 가져오기
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadINode(pstFileHandle->iNumber, gs_vbTempBuffer);
	kMemCpy(&iNode, gs_vbTempBuffer, sizeof(INODE));

	for (i = 0; i <= pstFileHandle->dwCurrentFileOffset; i++)
	{
		if (i == pstFileHandle->dwCurrentFileOffset)
		{
			curEntireFileOffset += pstFileHandle->dwCurrentOffset;
		}
		else
		{
			curEntireFileOffset += iNode.dataBlockActualDataSize[i];
		}
	}
	//==========================================================================
	// Origin과 Offset을 조합하여 파일 시작을 기준으로 파일 포인터를 옮겨야 할 위치를
	// 계산
	//==========================================================================
	// 옵션에 따라서 실제 위치를 계산
	// 음수이면 파일의 시작 방향으로 이동하며 양수이면 파일의 끝 방향으로 이동
	switch (iOrigin)
	{
		// 파일의 시작을 기준으로 이동
	case FILESYSTEM_SEEK_SET:
		// 파일의 처음이므로 이동할 오프셋이 음수이면 0으로 설정
		if (iOffset <= 0)
		{
			dwRealOffset = 0;
		}
		else
		{
			dwRealOffset = iOffset;
		}
		break;

		// 현재 위치를 기준으로 이동
	case FILESYSTEM_SEEK_CUR:
		// 이동할 오프셋이 음수이고 현재 파일 포인터의 값보다 크다면
		// 더 이상 갈 수 없으므로 파일의 처음으로 이동
		if ((iOffset < 0) &&
			(curEntireFileOffset <= (DWORD)-iOffset))
		{
			dwRealOffset = 0;
		}
		else
		{
			dwRealOffset = curEntireFileOffset + iOffset;
		}
		break;

		// 파일의 끝부분을 기준으로 이동
	case FILESYSTEM_SEEK_END:
		// 이동할 오프셋이 음수이고 현재 파일 포인터의 값보다 크다면
		// 더 이상 갈 수 없으므로 파일의 처음으로 이동
		if ((iOffset < 0) &&
			(curEntireFileOffset <= (DWORD)-iOffset))
		{
			dwRealOffset = 0;
		}
		else
		{
			dwRealOffset = pstFileHandle->dwFileSize + iOffset;
		}
		break;
	}

	pstFileHandle->dwCurrentFileOffset = 0;
	pstFileHandle->dwCurrentOffset = 0;

	for (i = 0; i < iNode.curDataBlocks; i++)
	{
		if (dwRealOffset < iNode.dataBlockActualDataSize[i])
		{
			pstFileHandle->dwCurrentOffset = dwRealOffset;
			dwRealOffset = 0;
			break;
		}
		dwRealOffset -= iNode.dataBlockActualDataSize[i];
		pstFileHandle->dwCurrentFileOffset++;
		pstFileHandle->dwCurrentOffset = 0;
	}

	if (dwRealOffset > 0)
	{
		return -1;
	}

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));

	return 0;
}

/**
 *  파일을 닫음
 */
int kCloseFile(FILE* pstFile)
{
	// 핸들 타입이 파일이 아니면 실패
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return -1;
	}

	// 핸들을 반환
	kFreeFileDirectoryHandle(pstFile);
	return 0;
}

/**
 *  핸들 풀을 검사하여 파일이 열려있는지를 확인
 */
BOOL kIsFileOpened(const DIRECTORYENTRY* pstEntry)
{
	int i;
	FILE* pstFile;
	INODE* iNode;

	// 핸들 풀의 시작 어드레스부터 끝까지 열린 파일만 검색
	pstFile = gs_stFileSystemManager.pstHandlePool;
	for (i = 0; i < FILESYSTEM_HANDLE_MAXCOUNT; i++)
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadINode(pstEntry->iNumber, gs_vbTempBuffer);
		iNode = (INODE*)gs_vbTempBuffer;

		// 파일 타입 중에서 시작 클러스터가 일치하면 반환
		if ((pstFile[i].bType == FILESYSTEM_TYPE_FILE) &&
			(pstFile[i].stFileHandle.dwStartFileOffset == iNode->dataBlockOffset[0]))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 *  파일을 삭제
 */
int kRemoveFile(const char* pcFileName)
{
	DIRECTORYENTRY stEntry;
	int iDirectoryEntryOffset;
	int iFileNameLength;
	CHECKPOINT checkPoint;

	// 파일 이름 검사
	iFileNameLength = kStrLen(pcFileName);
	if ((iFileNameLength > (sizeof(stEntry.vcFileName) - 1)) ||
		(iFileNameLength == 0))
	{
		return NULL;
	}

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// 파일이 존재하는가 확인
	iDirectoryEntryOffset = kFindDirectoryEntry(pcFileName, &stEntry);
	if (iDirectoryEntryOffset == -1)
	{
		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return -1;
	}

	// 다른 태스크에서 해당 파일을 열고 있는지 핸들 풀을 검색하여 확인
	// 파일이 열려 있으면 삭제할 수 없음
	if (kIsFileOpened(&stEntry) == TRUE)
	{
		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return -1;
	}

	// 파일을 가리키는 아이노드에 대한 연결을 끊는다.
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));
	checkPoint.iMap[stEntry.iNumber].iNumber = 0;
	checkPoint.iMap[stEntry.iNumber].iNodeOffset = 0;

	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kMemCpy(gs_vbTempBuffer, &checkPoint, sizeof(CHECKPOINT));
	kWriteHeadCheckPoint(gs_vbTempBuffer);

	// 디렉터리 엔트리를 빈 것으로 설정
	kMemSet(&stEntry, 0, sizeof(stEntry));
	if (kSetDirectoryEntryData(iDirectoryEntryOffset, &stEntry) == FALSE)
	{
		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return -1;
	}

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return 0;
}

/**
 *  디렉터리를 엶
 */
DIR* kOpenDirectory(const char* pcDirectoryName)
{
	DIR* pstDirectory;
	DIRECTORYENTRY* pstDirectoryBuffer;

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// 루트 디렉터리 밖에 없으므로 디렉터리 이름은 무시하고 핸들만 할당받아서 반환
	pstDirectory = kAllocateFileDirectoryHandle();
	if (pstDirectory == NULL)
	{
		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;
	}

	// 루트 디렉터리를 저장할 버퍼를 할당
	pstDirectoryBuffer = (DIRECTORYENTRY*)kAllocateMemory(FILESYSTEM_CLUSTERSIZE);
	if (pstDirectory == NULL)
	{
		// 실패하면 핸들을 반환해야 함
		kFreeFileDirectoryHandle(pstDirectory);
		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;
	}

	// 루트 디렉터리를 읽음
	if (kReadRootDirectory((BYTE*)pstDirectoryBuffer) == FALSE)
	{
		// 실패하면 핸들과 메모리를 모두 반환해야 함
		kFreeFileDirectoryHandle(pstDirectory);
		kFreeMemory(pstDirectoryBuffer);

		// 동기화
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;

	}

	// 디렉터리 타입으로 설정하고 현재 디렉터리 엔트리의 오프셋을 초기화
	pstDirectory->bType = FILESYSTEM_TYPE_DIRECTORY;
	pstDirectory->stDirectoryHandle.iCurrentOffset = 0;
	pstDirectory->stDirectoryHandle.pstDirectoryBuffer = pstDirectoryBuffer;

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return pstDirectory;
}

/**
 *  디렉터리 엔트리를 반환하고 다음으로 이동
 */
struct kDirectoryEntryStruct* kReadDirectory(DIR* pstDirectory)
{
	DIRECTORYHANDLE* pstDirectoryHandle;
	DIRECTORYENTRY* pstEntry;

	// 핸들 타입이 디렉터리가 아니면 실패
	if ((pstDirectory == NULL) ||
		(pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
	{
		return NULL;
	}
	pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

	// 오프셋의 범위가 클러스터에 존재하는 최댓값을 넘어서면 실패
	if ((pstDirectoryHandle->iCurrentOffset < 0) ||
		(pstDirectoryHandle->iCurrentOffset >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
	{
		return NULL;
	}

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// 루트 디렉터리에 있는 최대 디렉터리 엔트리의 개수만큼 검색
	pstEntry = pstDirectoryHandle->pstDirectoryBuffer;
	while (pstDirectoryHandle->iCurrentOffset < FILESYSTEM_MAXDIRECTORYENTRYCOUNT)
	{
		// 파일이 존재하면 해당 디렉터리 엔트리를 반환
		if (pstEntry[pstDirectoryHandle->iCurrentOffset].iNumber != 0)
		{
			// 동기화
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return &(pstEntry[pstDirectoryHandle->iCurrentOffset++]);
		}

		pstDirectoryHandle->iCurrentOffset++;
	}

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return NULL;
}

/**
 *  디렉터리 포인터를 디렉터리의 처음으로 이동
 */
void kRewindDirectory(DIR* pstDirectory)
{
	DIRECTORYHANDLE* pstDirectoryHandle;

	// 핸들 타입이 디렉터리가 아니면 실패
	if ((pstDirectory == NULL) ||
		(pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
	{
		return;
	}
	pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// 디렉터리 엔트리의 포인터만 0으로 바꿔줌
	pstDirectoryHandle->iCurrentOffset = 0;

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));
}


/**
 *  열린 디렉터리를 닫음
 */
int kCloseDirectory(DIR* pstDirectory)
{
	DIRECTORYHANDLE* pstDirectoryHandle;

	// 핸들 타입이 디렉터리가 아니면 실패
	if ((pstDirectory == NULL) ||
		(pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
	{
		return -1;
	}
	pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

	// 동기화
	kLock(&(gs_stFileSystemManager.stMutex));

	// 루트 디렉터리의 버퍼를 해제하고 핸들을 반환
	kFreeMemory(pstDirectoryHandle->pstDirectoryBuffer);
	kFreeFileDirectoryHandle(pstDirectory);

	// 동기화
	kUnlock(&(gs_stFileSystemManager.stMutex));

	return 0;
}
