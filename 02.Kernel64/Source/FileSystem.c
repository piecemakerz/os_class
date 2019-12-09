#include "FileSystem.h"
#include "HardDisk.h"
#include "DynamicMemory.h"
#include "Task.h"
#include "Utility.h"
#include "Console.h"

// ���� �ý��� �ڷᱸ��
static FILESYSTEMMANAGER   gs_stFileSystemManager;
// ���� �ý��� �ӽ� ����. 1 �� ũ��
static BYTE gs_vbTempBuffer[FILESYSTEM_SECTORSPERCLUSTER * 512];
// ���׸�Ʈ�� ��ũ�� ���̴� ��� ����
static DWORD SEGMENTBLOCKSIZE = (sizeof(SEGMENT) + FILESYSTEM_CLUSTERSIZE - 1) / FILESYSTEM_CLUSTERSIZE;
// �޸𸮿����� �� 2���� ���׸�Ʈ�� �����Ѵ�.
// �ϳ��� ���׸�Ʈ�� �������� �ٸ� ���׸�Ʈ�� ����ϸ�,
// ���� �� ���׸�Ʈ�� ��ũ�� Write�ϴ� �½�ũ ȣ���� ��û�Ѵ�.
static SEGMENT segments[2];
// ���� �۾����� ���׸�Ʈ ��ȣ. 0���� 1������ ������.
static int curWorkingSegment;
// �ϵ� ��ũ ��� ���õ� �Լ� ������ ����
fReadHDDInformation gs_pfReadHDDInformation = NULL;
fReadHDDSector gs_pfReadHDDSector = NULL;
fWriteHDDSector gs_pfWriteHDDSector = NULL;

/**
 *  �ϵ� ��ũ�� ���� �ý����� ����
 */
BOOL kFormat(void)
{
	HDDINFORMATION* pstHDD;
	MBR* pstMBR;
	DWORD dwTotalSectorCount, dwRemainSectorCount;
	DWORD dwMaxClusterCount, dwClsuterCount;
	DWORD i;

	// ����ȭ ó��
	kLock(&(gs_stFileSystemManager.stMutex));

	//==========================================================================
	//  �ϵ� ��ũ ������ �о ��Ÿ ������ ũ��� Ŭ�������� ������ ���
	//==========================================================================
	// �ϵ� ��ũ�� ������ �� �ϵ� ��ũ�� �� ���� ���� ����
	pstHDD = (HDDINFORMATION*)gs_vbTempBuffer;
	if (gs_pfReadHDDInformation(TRUE, TRUE, pstHDD) == FALSE)
	{
		// ����ȭ ó��
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}
	dwTotalSectorCount = pstHDD->dwTotalSectors;

	// ��� ������ ��ũ ���� ���� ��� ũ��� ������ ���� ����� ������ ����
	// MBR ���� & ��� üũ ����Ʈ ���� ����
	dwRemainSectorCount = dwTotalSectorCount - FILESYSTEM_SECTORSPERCLUSTER - 1;
	dwClsuterCount = dwRemainSectorCount / FILESYSTEM_SECTORSPERCLUSTER;
	//==========================================================================
	// ���� ������ MBR�� ���� ����, ��Ʈ ���͸� �������� ��� 0���� �ʱ�ȭ�Ͽ�
	// ���� �ý����� ����
	//==========================================================================
	// MBR ���� �б�
	if (gs_pfReadHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
	{
		// ����ȭ ó��
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// ��Ƽ�� ������ ���� �ý��� ���� ����
	pstMBR = (MBR*)gs_vbTempBuffer;
	kMemSet(pstMBR->vstPartition, 0, sizeof(pstMBR->vstPartition));
	pstMBR->dwSignature = FILESYSTEM_SIGNATURE;
	pstMBR->dwReservedSectorCount = 0;
	pstMBR->dwTotalClusterCount = dwClsuterCount;

	// MBR ������ 1 ���͸� ��
	if (gs_pfWriteHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
	{
		// ����ȭ ó��
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// MBR ���ĺ��� ��Ʈ ���͸����� ��� 0���� �ʱ�ȭ
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);

	// �� �� üũ����Ʈ�� ��Ʈ ���͸� ��� 0���� �ʱ�ȭ
	for (i = 0; i < (FILESYSTEM_SECTORSPERCLUSTER * 2); i++)
	{
		if (gs_pfWriteHDDSector(TRUE, TRUE, i + 1, 1, gs_vbTempBuffer) == FALSE)
		{
			// ����ȭ ó��
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return FALSE;
		}
	}
	// ����ȭ ó��
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return TRUE;
}

/**
 *  ���� �ý����� �ʱ�ȭ
 */
BOOL kInitializeFileSystem(void)
{
	// �ڷᱸ�� �ʱ�ȭ�� ����ȭ ��ü �ʱ�ȭ
	kMemSet(&gs_stFileSystemManager, 0, sizeof(gs_stFileSystemManager));
	kInitializeMutex(&(gs_stFileSystemManager.stMutex));

	// �ϵ� ��ũ�� �ʱ�ȭ
	if (kInitializeHDD() == TRUE)
	{
		// �ʱ�ȭ�� �����ϸ� �Լ� �����͸� �ϵ� ��ũ�� �Լ��� ����
		gs_pfReadHDDInformation = kReadHDDInformation;
		gs_pfReadHDDSector = kReadHDDSector;
		gs_pfWriteHDDSector = kWriteHDDSector;
	}
	else
	{
		return FALSE;
	}

	// ���� �ý��� ����
	if (kMount() == FALSE)
	{
		return FALSE;
	}

	// �ڵ��� ���� ������ �Ҵ�
	gs_stFileSystemManager.pstHandlePool = (FILE*)kAllocateMemory(
		FILESYSTEM_HANDLE_MAXCOUNT * sizeof(FILE));

	// �޸� �Ҵ��� �����ϸ� �ϵ� ��ũ�� �νĵ��� ���� ������ ����
	if (gs_stFileSystemManager.pstHandlePool == NULL)
	{
		gs_stFileSystemManager.bMounted = FALSE;
		return FALSE;
	}

	// �ڵ� Ǯ�� ��� 0���� �����Ͽ� �ʱ�ȭ
	kMemSet(gs_stFileSystemManager.pstHandlePool, 0,
		FILESYSTEM_HANDLE_MAXCOUNT * sizeof(FILE));

	return TRUE;
}

//==============================================================================
//  ������ �Լ�(Low Level Function)
//==============================================================================
/**
 *  �ϵ� ��ũ�� MBR�� �о MINT ���� �ý������� Ȯ��
 *      MINT ���� �ý����̶�� ���� �ý��ۿ� ���õ� ���� ������ �о
 *      �ڷᱸ���� ����
 */
BOOL kMount(void)
{
	MBR* pstMBR;

	// ����ȭ ó��
	kLock(&(gs_stFileSystemManager.stMutex));

	// MBR�� ����
	if (gs_pfReadHDDSector(TRUE, TRUE, 0, 1, gs_vbTempBuffer) == FALSE)
	{
		// ����ȭ ó��
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// �ñ׳�ó�� Ȯ���Ͽ� ���ٸ� �ڷᱸ���� �� ������ ���� ���� ����
	pstMBR = (MBR*)gs_vbTempBuffer;
	if (pstMBR->dwSignature != FILESYSTEM_SIGNATURE)
	{
		// ����ȭ ó��
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return FALSE;
	}

	// ���� �ý��� �ν� ����
	gs_stFileSystemManager.bMounted = TRUE;

	// �� ������ ���� LBA ��巹���� ���� ���� ���
	gs_stFileSystemManager.dwReservedSectorCount = pstMBR->dwReservedSectorCount;

	// ����� ���� �ڿ� ��� üũ �����Ͱ� �����Ѵ�.
	gs_stFileSystemManager.dwHeadCheckPointStartAddress = pstMBR->dwReservedSectorCount + 1;

	// ��Ʈ ���丮�� ��ũ ���� ������ ��ġ�� �����Ƿ�, ������ ��Ÿ������ó�� �����Ѵ�.
	gs_stFileSystemManager.dwRootDirStartAddress = pstMBR->dwReservedSectorCount + 1 + FILESYSTEM_SECTORSPERCLUSTER;
	gs_stFileSystemManager.dwDataAreaStartAddress = pstMBR->dwReservedSectorCount + 1 + (FILESYSTEM_SECTORSPERCLUSTER * 2);

	// ��Ʈ ���͸� ������ ������ ������� �������� ����.
	gs_stFileSystemManager.dwTotalClusterCount = pstMBR->dwTotalClusterCount - 1;

	// ������ ��ũ�� ���⸦ ������ ��ũ �ּ�(���� ���� ������)
	gs_stFileSystemManager.dwNextAllocateClusterOffset = gs_stFileSystemManager.dwDataAreaStartAddress;

	// �޸𸮿��� ����ϴ� ������ �ʱ�ȭ
	kMemSet(segments, 0, sizeof(SEGMENT));
	curWorkingSegment = 0;
	segments[0].segmentStartOffset = gs_stFileSystemManager.dwDataAreaStartAddress;
	segments[1].segmentStartOffset = segments[0].segmentStartOffset +
		(SEGMENTBLOCKSIZE * FILESYSTEM_SECTORSPERCLUSTER);

	// ����ȭ ó��
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return TRUE;
}

/**
 *  ���� �ý��ۿ� ����� �ϵ� ��ũ�� ������ ��ȯ
 */
BOOL kGetHDDInformation(HDDINFORMATION* pstInformation)
{
	BOOL bResult;

	// ����ȭ ó��
	kLock(&(gs_stFileSystemManager.stMutex));

	bResult = gs_pfReadHDDInformation(TRUE, TRUE, pstInformation);

	// ����ȭ ó��
	kUnlock(&(gs_stFileSystemManager.stMutex));

	return bResult;
}

/**
 *  ������ ������ �����¿��� �� Ŭ�����͸� ����
	��ũ�� ���� ���� �������� �Է¹���
 */
static BOOL kReadCluster(DWORD dwOffset, BYTE* pbBuffer)
{
	// ������ ������ ���� ��巹���� ����
	return gs_pfReadHDDSector(TRUE, TRUE, dwOffset, FILESYSTEM_SECTORSPERCLUSTER, pbBuffer);
}

/**
 *  ������ ������ �����¿� �� Ŭ�����͸� ��
	��ũ�� ���� ���� �������� �Է¹���
 */
static BOOL kWriteCluster(DWORD dwOffset, BYTE* pbBuffer)
{
	// ������ ������ ���� ��巹���� ����
	return gs_pfWriteHDDSector(TRUE, TRUE, dwOffset, FILESYSTEM_SECTORSPERCLUSTER, pbBuffer);
}

// ��Ʈ ���͸��� ����
static BOOL kReadRootDirectory(BYTE* pbBuffer)
{
	return kReadCluster(gs_stFileSystemManager.dwRootDirStartAddress, pbBuffer);
}

// ��Ʈ ���͸��� ��
static BOOL kWriteRootDirectory(BYTE* pbBuffer)
{
	return kWriteCluster(gs_stFileSystemManager.dwRootDirStartAddress, pbBuffer);
}

// ��� üũ����Ʈ�� ����
static BOOL kReadHeadCheckPoint(BYTE* pbBuffer)
{
	return kReadCluster(gs_stFileSystemManager.dwHeadCheckPointStartAddress, pbBuffer);
}

// ��� üũ����Ʈ�� ��
static BOOL kWriteHeadCheckPoint(BYTE* pbBuffer)
{
	return kWriteCluster(gs_stFileSystemManager.dwHeadCheckPointStartAddress, pbBuffer);
}

// ��ũ���� ���̳�带 ����
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

// segmentNum ���׸�Ʈ�� ��� 1�� ����
// ���̳�� �� ����� ��� ��ũ �� üũ����Ʈ�� �����Ѵ�.
// ���� ���׸�Ʈ�� ���� ���� ��ũ�� ���� ���׸�Ʈ�� ��ȯ�Ѵ�.
// ���ڷ� ���� INODE�� �ֽ� �����ͷ� ���ŵǾ��ٰ� �����Ѵ�.
// type - ������(0), ���̳��(1), ���̳�� �� ����(2)
static BOOL kWriteSegment(DWORD segmentNum, DWORD type, INODE* inode, BYTE* pbBuffer)
{
	SEGMENT* curSegment;
	BYTE* curDataRegion;
	SEGMENTSUMMARYBLOCK* SS;
	DWORD* curTypeArr;
	// ����Ʈ ���� ������ ���� �ε���
	DWORD curSegmentIdx;
	// ���� ���׸�Ʈ ������ ��� ��
	DWORD curBlockCnt;
	CHECKPOINT checkPoint;

	curSegment = &segments[segmentNum];
	SS = &(curSegment->SS);
	curTypeArr = curSegment->dataBlockType;
	curBlockCnt = curSegment->curBlockCnt;
	curSegmentIdx = curBlockCnt * FILESYSTEM_CLUSTERSIZE;
	curDataRegion = curSegment->dataRegion;

	// ���׸�Ʈ�� ��� ������ ������ ����
	kMemCpy((char*)curDataRegion + curSegmentIdx, pbBuffer, FILESYSTEM_CLUSTERSIZE);
	// �� ������ ����� Ÿ�� �Ҵ�
	curTypeArr[curBlockCnt] = type;

	// ������ ����� ��� ���׸�Ʈ�� SS ����� �������ش�.
	if (type == 0)
	{
		SS->inumbers[curBlockCnt] = inode->iNumber;
		SS->offsets[curBlockCnt] = inode->curDataBlocks - 1;
	}

	// ���̳�� �� ����� ��� ��ũ �� üũ����Ʈ�� �����Ѵ�.
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

	// ���׸�Ʈ�� ����� ��� �� ����
	curSegment->curBlockCnt++;

	// ���� ���׸�Ʈ�� ���� á�ٸ� ���׸�Ʈ�� ��ũ�� ���� ����.
	// ���� �۾� ���׸�Ʈ�� �ٸ� ���׸�Ʈ�� ��ü�Ѵ�.
	if (curSegment->curBlockCnt >= FILESYSTEM_MAXSEGMENTCLUSTERCOUNT)
	{
		// �ٸ� ���׸�Ʈ �ʱ�ȭ
		kMemSet(&segments[!curWorkingSegment], 0, sizeof(SEGMENT));
		// �ٸ� ���׸�Ʈ ���� �ּ� �Ҵ�
		segments[!curWorkingSegment].segmentStartOffset = segments[curWorkingSegment].nextSegmentOffset;
		// �ٸ� ���׸�Ʈ�� ���� ���׸�Ʈ �ּ� �Ҵ�
		segments[!curWorkingSegment].nextSegmentOffset = segments[!curWorkingSegment].segmentStartOffset +
			(SEGMENTBLOCKSIZE * FILESYSTEM_SECTORSPERCLUSTER);
		// ���׸�Ʈ�� ��ũ�� ��°�� Write
		kWriteSegmentToDisk(curWorkingSegment);
		// ���׸�Ʈ ��ü
		curWorkingSegment = !curWorkingSegment;
	}

	return TRUE;
}

// ���׸�Ʈ�� ��ũ�� ����ϱ�
// üũ����Ʈ�� �������ش�.
static BOOL kWriteSegmentToDisk(DWORD segmentNum)
{
	SEGMENT* segment = &(segments[segmentNum]);
	CHECKPOINT* checkPoint;
	// ������� ��ũ�� ���� ���׸�Ʈ ����ü�� ��� ��
	DWORD curWriteSegmentBlocks = 0;

	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	checkPoint = (CHECKPOINT*)gs_vbTempBuffer;
	// ���� �ֱ� ���׸�Ʈ ���� ��ġ ����
	checkPoint->tailSegmentOffset = segment->segmentStartOffset;
	kWriteHeadCheckPoint(gs_vbTempBuffer);

	// ���׸�Ʈ �� ��Ͼ� ��ũ�� ���
	// ���׸�Ʈ�� ������ �Ҵ�� ������ ��� ������ ��� ���� ������ �ִ� ���׸�Ʈ ũ�⸸ŭ ���
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

		// �� ������ ��ũ�� Write
		kWriteCluster(gs_stFileSystemManager.dwNextAllocateClusterOffset, gs_vbTempBuffer);
		gs_stFileSystemManager.dwNextAllocateClusterOffset += FILESYSTEM_SECTORSPERCLUSTER;
		curWriteSegmentBlocks += 1;
	}

	return TRUE;
}

// �� ���̳�� �˻�
// �� ���̳�忡 �ش��ϴ� ���̳�� ��ȣ ��ȯ
// �� ���̳�带 ã�� ���ߴٸ� 0 ��ȯ
static DWORD kFindFreeINode(void)
{
	DWORD i;
	CHECKPOINT checkPoint;
	INODEMAPBLOCK* inodeMap;
	INODE* curInode;

	// ���� �ý����� �ν����� �������� ����
	if (gs_stFileSystemManager.bMounted == FALSE)
	{
		return -1;
	}

	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));
	inodeMap = checkPoint.iMap;

	// ������ ���鼭 �� ���̳�� �˻�
	for (i = 1; i <= 128; i++)
	{
		// ���̳ѹ��� 0���̸� �� ���̳���̴�.
		if (inodeMap[i].iNumber == 0)
		{
			return i;
		}
	}

	return 0;
}

/**
 *  ��Ʈ ���͸����� �� ���͸� ��Ʈ���� ��ȯ
	���͸� ��Ʈ���� ���̳ѹ��� 0�̸� �� ��Ʈ���̴�.
 */
static int kFindFreeDirectoryEntry(void)
{
	DIRECTORYENTRY* pstEntry;
	int i;

	// ���� �ý����� �ν����� �������� ����
	if (gs_stFileSystemManager.bMounted == FALSE)
	{
		return -1;
	}

	// ��Ʈ ���͸��� ����
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return -1;
	}

	// ��Ʈ ���͸� �ȿ��� ������ ���鼭 �� ��Ʈ��, �� ���̳�� ��ȣ��
	// 0�� ������ ���͸� ��Ʈ���� �˻�
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
 *  ��Ʈ ���͸��� �ش� �ε����� ���͸� ��Ʈ���� ����
 */
static BOOL kSetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry)
{
	DIRECTORYENTRY* pstRootEntry;

	// ���� �ý����� �ν����� ���߰ų� �ε����� �ùٸ��� ������ ����
	if ((gs_stFileSystemManager.bMounted == FALSE) ||
		(iIndex < 0) || (iIndex >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
	{
		return FALSE;
	}

	// ��Ʈ ���͸��� ����
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return FALSE;
	}

	// ��Ʈ ���͸��� �ִ� �ش� �����͸� ����
	pstRootEntry = (DIRECTORYENTRY*)gs_vbTempBuffer;
	kMemCpy(pstRootEntry + iIndex, pstEntry, sizeof(DIRECTORYENTRY));

	// ��Ʈ ���͸��� ��
	if (kWriteRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return FALSE;
	}
	return TRUE;
}

/**
 *  ��Ʈ ���͸��� �ش� �ε����� ��ġ�ϴ� ���͸� ��Ʈ���� ��ȯ
 */
static BOOL kGetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry)
{
	DIRECTORYENTRY* pstRootEntry;

	// ���� �ý����� �ν����� ���߰ų� �ε����� �ùٸ��� ������ ����
	if ((gs_stFileSystemManager.bMounted == FALSE) ||
		(iIndex < 0) || (iIndex >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
	{
		return FALSE;
	}

	// ��Ʈ ���͸��� ����
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return FALSE;
	}

	// ��Ʈ ���͸��� �ִ� �ش� �����͸� ����
	pstRootEntry = (DIRECTORYENTRY*)gs_vbTempBuffer;
	kMemCpy(pstEntry, pstRootEntry + iIndex, sizeof(DIRECTORYENTRY));
	return TRUE;
}

/**
 *  ��Ʈ ���͸����� ���� �̸��� ��ġ�ϴ� ��Ʈ���� ã�Ƽ� �ε����� ��ȯ
 */
 // pstEntry�� �ش� ��Ʈ�� ������ �����ϰ� �ε��� ��ȯ
static int kFindDirectoryEntry(const char* pcFileName, DIRECTORYENTRY* pstEntry)
{
	DIRECTORYENTRY* pstRootEntry;
	int i;
	int iLength;

	// ���� �ý����� �ν����� �������� ����
	if (gs_stFileSystemManager.bMounted == FALSE)
	{
		return -1;
	}

	// ��Ʈ ���͸��� ����
	if (kReadRootDirectory(gs_vbTempBuffer) == FALSE)
	{
		return -1;
	}

	iLength = kStrLen(pcFileName);
	// ��Ʈ ���͸� �ȿ��� ������ ���鼭 ���� �̸��� ��ġ�ϴ� ��Ʈ���� ��ȯ
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
 *  ���� �ý����� ������ ��ȯ
 */
void kGetFileSystemInformation(FILESYSTEMMANAGER* pstManager)
{
	kMemCpy(pstManager, &gs_stFileSystemManager, sizeof(gs_stFileSystemManager));
}

//==============================================================================
//  ����� �Լ�(High Level Function)
//==============================================================================
/**
 *  ����ִ� �ڵ��� �Ҵ�
 */
static void* kAllocateFileDirectoryHandle(void)
{
	int i;
	FILE* pstFile;

	// �ڵ� Ǯ(Handle Pool)�� ��� �˻��Ͽ� ����ִ� �ڵ��� ��ȯ
	pstFile = gs_stFileSystemManager.pstHandlePool;
	for (i = 0; i < FILESYSTEM_HANDLE_MAXCOUNT; i++)
	{
		// ����ִٸ� ��ȯ
		if (pstFile->bType == FILESYSTEM_TYPE_FREE)
		{
			pstFile->bType = FILESYSTEM_TYPE_FILE;
			return pstFile;
		}

		// �������� �̵�
		pstFile++;
	}

	return NULL;
}

/**
 *  ����� �ڵ��� ��ȯ
 */
static void kFreeFileDirectoryHandle(FILE* pstFile)
{
	// ��ü ������ �ʱ�ȭ
	kMemSet(pstFile, 0, sizeof(FILE));

	// ����ִ� Ÿ������ ����
	pstFile->bType = FILESYSTEM_TYPE_FREE;
}

/**
 *  ������ ����
 */
static BOOL kCreateFile(const char* pcFileName, DIRECTORYENTRY* pstEntry,
	int* piDirectoryEntryIndex)
{
	DWORD iNumber;
	INODEMAPBLOCK inodeMap;
	INODE newInode;
	SEGMENT* curSegment;

	// �� ���̳�� �˻�
	iNumber = kFindFreeINode();
	if (iNumber == 0)
	{
		return FALSE;
	}

	// �� ���͸� ��Ʈ���� �˻�
	*piDirectoryEntryIndex = kFindFreeDirectoryEntry();
	if (*piDirectoryEntryIndex == -1)
	{
		return FALSE;
	}

	// ���͸� ��Ʈ���� ����
	kMemCpy(pstEntry->vcFileName, pcFileName, kStrLen(pcFileName) + 1);
	pstEntry->iNumber = iNumber;
	pstEntry->dwFileSize = 0;

	// ���͸� ��Ʈ���� ���
	if (kSetDirectoryEntryData(*piDirectoryEntryIndex, pstEntry) == FALSE)
	{
		return FALSE;
	}

	// ���̳�� �Ҵ�
	kMemSet(&newInode, 0, sizeof(INODE));
	newInode.iNumber = iNumber;

	// ���̳�� �� �Ҵ�
	inodeMap.iNumber = iNumber;
	inodeMap.iNodeOffset = curSegment->segmentStartOffset +
		(curSegment->curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

	// ���� �� ���̳�� ������ ���׸�Ʈ�� ����
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kMemCpy(gs_vbTempBuffer, &newInode, sizeof(INODE));
	kWriteSegment(curWorkingSegment, 1, &newInode, gs_vbTempBuffer);

	// ���̳�� �� ���� ������ ���׸�Ʈ�� ����
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kMemCpy(gs_vbTempBuffer, &inodeMap, sizeof(INODEMAPBLOCK));
	kWriteSegment(curWorkingSegment, 2, &inodeMap, gs_vbTempBuffer);

	return TRUE;
}

/**
 *  ������ ���ų� ����
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

	// ���� �̸� �˻�
	iFileNameLength = kStrLen(pcFileName);
	if ((iFileNameLength > (sizeof(stEntry.vcFileName) - 1)) ||
		(iFileNameLength == 0))
	{
		return NULL;
	}

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// open�� �����ϱ� �� ����ȭ�� ���� ���� ���׸�Ʈ�� ��ũ�� write
	kWriteSegmentToDisk(curWorkingSegment);

	// ��ũ�κ��� üũ����Ʈ �б�
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));

	//==========================================================================
	// ������ ���� �����ϴ°� Ȯ���ϰ�, ���ٸ� �ɼ��� ���� ������ ����
	//==========================================================================
	iDirectoryEntryOffset = kFindDirectoryEntry(pcFileName, &stEntry);
	// ������ �������� �ʴ� ���
	if (iDirectoryEntryOffset == -1)
	{
		// ������ ���ٸ� �б�(r, r+) �ɼ��� ����
		if (pcMode[0] == 'r')
		{
			// ����ȭ
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return NULL;
		}

		// ������ �ɼǵ��� ������ ����
		if (kCreateFile(pcFileName, &stEntry, &iDirectoryEntryOffset) == FALSE)
		{
			// ����ȭ
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return NULL;
		}

		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadINode(stEntry.iNumber, gs_vbTempBuffer);
		kMemCpy(&inode, gs_vbTempBuffer, sizeof(INODE));
	}
	//==========================================================================
	// ������ ������ ����� �ϴ� �ɼ��̸� ���Ͽ� ����� Ŭ�����͸� ��� �����ϰ�
	// ���� ũ�⸦ 0���� ����
	//==========================================================================
	else if (pcMode[0] == 'w')
	{
		// LFS������ �����͸� �����θ� ����.
		// ���� ���̳�� ������ ��� �����͸� ��� 0���� �ʱ�ȭ ��
		// ���̳�� ���׸�Ʈ�� ���� �� ����
		kMemSet(&inode, 0, sizeof(INODE));
		inode.iNumber = stEntry.iNumber;

		// ���̳�� �� ���� ����
		iNodeMap.iNumber = inode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset
			+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// ���׸�Ʈ�� ���̳�� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &inode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &inode, gs_vbTempBuffer);

		// ���׸�Ʈ�� ���̳�� �� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &inode.iNumber, gs_vbTempBuffer);

		// ������ ������ ��� ��������Ƿ�, ũ�⸦ 0���� ����
		stEntry.dwFileSize = 0;
		if (kSetDirectoryEntryData(iDirectoryEntryOffset, &stEntry) == FALSE)
		{
			// ����ȭ
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
	// ���� �ڵ��� �Ҵ� �޾� �����͸� ������ �� ��ȯ
	//==========================================================================
	// ���� �ڵ��� �Ҵ� �޾� ������ ����
	pstFile = kAllocateFileDirectoryHandle();
	if (pstFile == NULL)
	{
		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;
	}

	// ���� �ڵ鿡 ���� ������ ����
	pstFile->bType = FILESYSTEM_TYPE_FILE;
	pstFile->stFileHandle.iDirectoryEntryOffset = iDirectoryEntryOffset;
	pstFile->stFileHandle.iNumber = inode.iNumber;
	pstFile->stFileHandle.dwFileSize = inode.size;
	pstFile->stFileHandle.dwStartFileOffset = inode.dataBlockOffset[0];
	pstFile->stFileHandle.dwCurrentFileOffset = 0;
	pstFile->stFileHandle.dwCurrentOffset = 0;

	// ���� �߰� �ɼ�(a)�� �����Ǿ� ������, ������ ������ �̵�
	if (pcMode[0] == 'a')
	{
		kSeekFile(pstFile, 0, FILESYSTEM_SEEK_END);
	}

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return pstFile;
}

/**
 *  ������ �о� ���۷� ����
	��� �����ʹ� ��� ������ ����Ǿ� ����.
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

	// �ڵ��� ���� Ÿ���� �ƴϸ� ����
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return 0;
	}

	// ����ȭ�� ���� ���׸�Ʈ ����
	kWriteSegmentToDisk(curWorkingSegment);
	pstFileHandle = &(pstFile->stFileHandle);

	iNumber = pstFileHandle->iNumber;
	kReadINode(iNumber, gs_vbTempBuffer);
	kMemCpy(&iNode, gs_vbTempBuffer, sizeof(INODE));

	// ������ ����ų� �̹� ������ ���̸� ����
	if ((pstFileHandle->dwFileSize == 0) || (
		(pstFileHandle->dwCurrentFileOffset == (iNode.curDataBlocks - 1))
		&& (pstFileHandle->dwCurrentOffset == iNode.dataBlockActualDataSize[pstFileHandle->dwCurrentFileOffset])
		))
	{
		return 0;
	}

	// ���� I/O ���� ���� ������ ������ ���
	currentDataBlockIdx = pstFileHandle->dwCurrentFileOffset;
	// ���� ������ ��Ͽ����� ������
	dwOffsetInCluster = pstFileHandle->dwCurrentOffset;
	// ������ ���� ������ ũ��
	actualDataSize += (iNode.dataBlockActualDataSize[currentDataBlockIdx]
		- dwOffsetInCluster);

	for (i = currentDataBlockIdx + 1; i < 12; i++)
	{
		actualDataSize += iNode.dataBlockActualDataSize[i];
	}

	// ���� ���� ���ؼ� ������ ���� �� �ִ� ���� ��� (����Ʈ ����)
	dwTotalCount = MIN(dwSize * dwCount, actualDataSize);

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// ���� ����ŭ �� ���� ������ �ݺ�
	dwReadCount = 0;

	while (dwReadCount != dwTotalCount)
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadCluster(iNode.dataBlockOffset[currentDataBlockIdx], gs_vbTempBuffer);

		// ó�� ���� �� ������ ����� �߰����� �д� ���
		if (pstFileHandle->dwCurrentOffset != 0)
		{
			dwCopySize = MIN(iNode.dataBlockActualDataSize[currentDataBlockIdx]
				- pstFileHandle->dwCurrentOffset, dwTotalCount - dwReadCount);
		}
		// ������ ����� ó������ �д� ���
		else
		{
			dwCopySize = MIN(iNode.dataBlockActualDataSize[currentDataBlockIdx], dwTotalCount - dwReadCount);
		}

		// ���ۿ� ���� ������ ���
		kMemCpy((char*)pvBuffer + dwReadCount, gs_vbTempBuffer, dwCopySize);
		dwReadCount += dwCopySize;
		currentDataBlockIdx++;

		// ������ ����� ������ ���� ���
		if (dwCopySize == iNode.dataBlockActualDataSize[currentDataBlockIdx])
		{
			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
		}
		// ������ ����� �߰������� ���� ���
		// �бⰡ ���� ����̴�.
		else
		{
			pstFileHandle->dwCurrentOffset += dwCopySize;
		}
	}

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));

	// ���� ���ڵ� ���� ��ȯ
	return (dwReadCount / dwSize);
}

/**
 *  ��Ʈ ���͸����� ���͸� ��Ʈ�� ���� ����
 */
static BOOL kUpdateDirectoryEntry(FILEHANDLE* pstFileHandle)
{
	DIRECTORYENTRY stEntry;

	// ���͸� ��Ʈ�� �˻�
	if ((pstFileHandle == NULL) ||
		(kGetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry)
			== FALSE))
	{
		return FALSE;
	}

	// ���� ũ��� ���� Ŭ�����͸� ����
	stEntry.dwFileSize = pstFileHandle->dwFileSize;
	stEntry.iNumber = pstFileHandle->iNumber;

	// ����� ���͸� ��Ʈ���� ����
	if (kSetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry)
		== FALSE)
	{
		return FALSE;
	}
	return TRUE;
}

/**
 *  ������ �����͸� ���Ͽ� ��
	��� ������ ���׸�Ʈ�� �����͸� ��
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

	// �ڵ��� ���� Ÿ���� �ƴϸ� ����
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return 0;
	}
	pstFileHandle = &(pstFile->stFileHandle);

	kWriteSegmentToDisk(curWorkingSegment);

	// ���Ͽ� �ش��ϴ� ���丮 ��Ʈ�� �о����
	kGetDirectoryEntryData(pstFileHandle->iDirectoryEntryOffset, &stEntry);

	kMemSet(tempFrontData, 0, sizeof(tempFrontData));
	kMemSet(tempBackData, 0, sizeof(tempBackData));

	// ���� ���̳�� �о����
	iNumber = pstFileHandle->iNumber;
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadINode(iNumber, gs_vbTempBuffer);
	kMemCpy(&iNode, gs_vbTempBuffer, sizeof(INODE));
	kMemCpy(&tempNode, &iNode, sizeof(INODE));

	// ���Ͽ� �� �� ����Ʈ ��
	dwTotalCount = dwSize * dwCount;

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));
	// �� �� ������ �ݺ�
	dwWriteCount = 0;

	// ������ ����ų� ������ ��(SEEK_END)�̸� ���ο� ������ ��� �߰�
	if (iNode.size == 0 || ((pstFileHandle->dwCurrentFileOffset == (iNode.curDataBlocks - 1))
			&& (pstFileHandle->dwCurrentOffset == iNode.dataBlockActualDataSize[iNode.curDataBlocks - 1])))
	{
		while (dwWriteCount != dwTotalCount)
		{
			// ���Ͽ� �� ������ �� 1�� ũ�� ����
			dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE, dwTotalCount - dwWriteCount);

			// ���̳�� ����
			iNode.dataBlockOffset[iNode.curDataBlocks] = segments[curWorkingSegment].segmentStartOffset
				+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
			iNode.dataBlockActualDataSize[iNode.curDataBlocks] = dwCopySize;
			iNode.size += dwCopySize;
			iNode.curDataBlocks++;

			// ���׸�Ʈ�� ���ο� ������ ��� ����
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, (char*)pvBuffer + dwWriteCount, dwCopySize);
			kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

			// ���̳�� �� ���� ����
			iNodeMap.iNumber = iNode.iNumber;
			iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
				(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

			// ���׸�Ʈ�� ���̳�� ����
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
			kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

			// ���׸�Ʈ�� ���̳�� �� ����
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
			kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

			// ���� �ڵ� ����
			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
			pstFileHandle->dwFileSize += dwCopySize;

			dwWriteCount += dwCopySize;
		}
	}
	// ���� ������ �߰��������� �����͸� ���� ���
	// ���� ����� 'ó������ �߰�' / '�߰����� ������'�� ������� ���� ��,
	// 'ó������ �߰�' ����� ���� �������� �Էµ� �����͸� ���� ����
	// '�߰����� ������' �����ʹ� ������ ������ ������� �߰��Ѵ�.
	// üũ����Ʈ�� ���̳�� ��, ���׸�Ʈ ��� ���, ���̳�带 �����ؾ� �Ѵ�.
	else
	{
		startFileOffset = pstFileHandle->dwCurrentFileOffset;
		// ���� ��� �б�
		kReadCluster(iNode.dataBlockOffset[startFileOffset], gs_vbTempBuffer);

		// ���� ����� 'ó������ �߰�' / '�߰����� ������'���� ������
		tempFrontSize = pstFileHandle->dwCurrentOffset;
		tempBackSize = iNode.dataBlockActualDataSize[startFileOffset] - tempFrontSize;

		kMemCpy(tempFrontData, gs_vbTempBuffer, tempFrontSize);
		kMemCpy(tempBackData, (char*)gs_vbTempBuffer + tempFrontSize, tempBackSize);

		// 'ó������ �߰�' ��Ͽ� �߰��� �� ���
		dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE - tempFrontSize, dwTotalCount - dwWriteCount);

		// ���̳�� ����
		iNode.dataBlockOffset[startFileOffset] = segments[curWorkingSegment].segmentStartOffset
			+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
		iNode.dataBlockActualDataSize[startFileOffset] += dwCopySize;
		iNode.size += dwCopySize;

		// 'ó������ �߰�' ��Ͽ� ���Ͽ� �� �����͸� �߰��� ��� ���׸�Ʈ�� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, tempFrontData, tempFrontSize);
		kMemCpy((char*)gs_vbTempBuffer + tempFrontSize, pvBuffer, dwCopySize);
		kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

		// ���̳�� �� ���� ����
		iNodeMap.iNumber = iNode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
			(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// ���׸�Ʈ�� ���̳�� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

		// ���׸�Ʈ�� ���̳�� �� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

		// ���� �ڵ� ����
		if (startFileOffset == 0)
		{
			pstFileHandle->dwStartFileOffset = iNode.dataBlockOffset[startFileOffset];
		}
		pstFileHandle->dwCurrentFileOffset++;
		pstFileHandle->dwCurrentOffset = 0;
		pstFileHandle->dwFileSize += dwCopySize;

		dwWriteCount += dwCopySize;

		blocksToMove = 0;
		// ���Ͽ� �� �κ� �� ���� �κ��� ���ο� ������ ����� �Ҵ����
		while (dwWriteCount != dwTotalCount)
		{
			// �Űܾ� �� ������ ��� ������ ��
			blocksToMove++;

			// ���Ͽ� �� ������ �� 1�� ũ�� ����
			dwCopySize = MIN(FILESYSTEM_CLUSTERSIZE, dwTotalCount - dwWriteCount);

			// ���̳�� ����
			iNode.dataBlockOffset[pstFileHandle->dwCurrentFileOffset] = segments[curWorkingSegment].segmentStartOffset
				+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
			iNode.dataBlockActualDataSize[iNode.curDataBlocks] += dwCopySize;
			iNode.size += dwCopySize;
			iNode.curDataBlocks++;

			// ���׸�Ʈ�� ������ ��� ����
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, (char*)pvBuffer + dwWriteCount, dwCopySize);
			kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

			// ���̳�� �� ���� ����
			iNodeMap.iNumber = iNode.iNumber;
			iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
				(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

			// ���׸�Ʈ�� ���̳�� ����
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
			kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

			// ���׸�Ʈ�� ���̳�� �� ����
			kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
			kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
			kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

			// ���� �ڵ� ����
			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
			pstFileHandle->dwFileSize += dwCopySize;

			dwWriteCount += dwCopySize;
		}

		// '�߰����� ������' ��� ������ ���׸�Ʈ�� ����
		// ���̳�� ����
		iNode.dataBlockOffset[pstFileHandle->dwCurrentFileOffset] = segments[curWorkingSegment].segmentStartOffset
			+ (segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);
		iNode.dataBlockActualDataSize[pstFileHandle->dwCurrentFileOffset] = tempBackSize;

		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, tempBackData, tempBackSize);
		kWriteSegment(curWorkingSegment, 0, &iNode, gs_vbTempBuffer);

		// ���̳�� �� ���� ����
		iNodeMap.iNumber = iNode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
			(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// ���׸�Ʈ�� ���̳�� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

		// ���׸�Ʈ�� ���̳�� �� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);

		// ���� �ڵ� ����
		pstFileHandle->dwCurrentFileOffset++;
		pstFileHandle->dwCurrentOffset = 0;

		// �߰��� ������ ������� ���� �ڷ� �и� ������ ��ϵ� ����
		for (i = 0; i < blocksToMove; i++)
		{
			iNode.dataBlockOffset[startFileOffset + 1 + blocksToMove + i] =
				tempNode.dataBlockOffset[startFileOffset + 1 + i];
			iNode.dataBlockActualDataSize[startFileOffset + 1 + blocksToMove + i] =
				tempNode.dataBlockActualDataSize[startFileOffset + 1 + i];

			pstFileHandle->dwCurrentFileOffset++;
			pstFileHandle->dwCurrentOffset = 0;
		}

		// ���̳�� �� ���� ����
		iNodeMap.iNumber = iNode.iNumber;
		iNodeMap.iNodeOffset = segments[curWorkingSegment].segmentStartOffset +
			(segments[curWorkingSegment].curBlockCnt * FILESYSTEM_SECTORSPERCLUSTER);

		// ���׸�Ʈ�� ���̳�� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNode, sizeof(INODE));
		kWriteSegment(curWorkingSegment, 1, &iNode, gs_vbTempBuffer);

		// ���׸�Ʈ�� ���̳�� �� ����
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kMemCpy(gs_vbTempBuffer, &iNodeMap, sizeof(INODEMAPBLOCK));
		kWriteSegment(curWorkingSegment, 2, &iNode, gs_vbTempBuffer);
	}
	//==========================================================================
   // ���� ũ�Ⱑ ���ߴٸ� ��Ʈ ���͸��� �ִ� ���͸� ��Ʈ�� ������ ����
  //==========================================================================

	pstFileHandle->dwCurrentFileOffset--;
	pstFileHandle->dwCurrentOffset = iNode.dataBlockActualDataSize[pstFileHandle->dwCurrentFileOffset];

	if (pstFileHandle->dwFileSize > tempNode.size)
	{
		kUpdateDirectoryEntry(pstFileHandle);
	}

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));

	// �� ���ڵ� ���� ��ȯ
	return (dwWriteCount / dwSize);
}

/**
 *  ���� �������� ��ġ�� �̵�
 */
int kSeekFile(FILE* pstFile, int iOffset, int iOrigin)
{
	DWORD dwRealOffset;
	DWORD i;
	DWORD tempOffset;
	DWORD curEntireFileOffset = 0;
	FILEHANDLE* pstFileHandle;
	INODE iNode;

	// �ڵ��� ���� Ÿ���� �ƴϸ� ����
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return 0;
	}
	pstFileHandle = &(pstFile->stFileHandle);

	kWriteSegmentToDisk(curWorkingSegment);
	// ���� ���̳�� ��������
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
	// Origin�� Offset�� �����Ͽ� ���� ������ �������� ���� �����͸� �Űܾ� �� ��ġ��
	// ���
	//==========================================================================
	// �ɼǿ� ���� ���� ��ġ�� ���
	// �����̸� ������ ���� �������� �̵��ϸ� ����̸� ������ �� �������� �̵�
	switch (iOrigin)
	{
		// ������ ������ �������� �̵�
	case FILESYSTEM_SEEK_SET:
		// ������ ó���̹Ƿ� �̵��� �������� �����̸� 0���� ����
		if (iOffset <= 0)
		{
			dwRealOffset = 0;
		}
		else
		{
			dwRealOffset = iOffset;
		}
		break;

		// ���� ��ġ�� �������� �̵�
	case FILESYSTEM_SEEK_CUR:
		// �̵��� �������� �����̰� ���� ���� �������� ������ ũ�ٸ�
		// �� �̻� �� �� �����Ƿ� ������ ó������ �̵�
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

		// ������ ���κ��� �������� �̵�
	case FILESYSTEM_SEEK_END:
		// �̵��� �������� �����̰� ���� ���� �������� ������ ũ�ٸ�
		// �� �̻� �� �� �����Ƿ� ������ ó������ �̵�
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

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));

	return 0;
}

/**
 *  ������ ����
 */
int kCloseFile(FILE* pstFile)
{
	// �ڵ� Ÿ���� ������ �ƴϸ� ����
	if ((pstFile == NULL) ||
		(pstFile->bType != FILESYSTEM_TYPE_FILE))
	{
		return -1;
	}

	// �ڵ��� ��ȯ
	kFreeFileDirectoryHandle(pstFile);
	return 0;
}

/**
 *  �ڵ� Ǯ�� �˻��Ͽ� ������ �����ִ����� Ȯ��
 */
BOOL kIsFileOpened(const DIRECTORYENTRY* pstEntry)
{
	int i;
	FILE* pstFile;
	INODE* iNode;

	// �ڵ� Ǯ�� ���� ��巹������ ������ ���� ���ϸ� �˻�
	pstFile = gs_stFileSystemManager.pstHandlePool;
	for (i = 0; i < FILESYSTEM_HANDLE_MAXCOUNT; i++)
	{
		kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
		kReadINode(pstEntry->iNumber, gs_vbTempBuffer);
		iNode = (INODE*)gs_vbTempBuffer;

		// ���� Ÿ�� �߿��� ���� Ŭ�����Ͱ� ��ġ�ϸ� ��ȯ
		if ((pstFile[i].bType == FILESYSTEM_TYPE_FILE) &&
			(pstFile[i].stFileHandle.dwStartFileOffset == iNode->dataBlockOffset[0]))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 *  ������ ����
 */
int kRemoveFile(const char* pcFileName)
{
	DIRECTORYENTRY stEntry;
	int iDirectoryEntryOffset;
	int iFileNameLength;
	CHECKPOINT checkPoint;

	// ���� �̸� �˻�
	iFileNameLength = kStrLen(pcFileName);
	if ((iFileNameLength > (sizeof(stEntry.vcFileName) - 1)) ||
		(iFileNameLength == 0))
	{
		return NULL;
	}

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// ������ �����ϴ°� Ȯ��
	iDirectoryEntryOffset = kFindDirectoryEntry(pcFileName, &stEntry);
	if (iDirectoryEntryOffset == -1)
	{
		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return -1;
	}

	// �ٸ� �½�ũ���� �ش� ������ ���� �ִ��� �ڵ� Ǯ�� �˻��Ͽ� Ȯ��
	// ������ ���� ������ ������ �� ����
	if (kIsFileOpened(&stEntry) == TRUE)
	{
		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return -1;
	}

	// ������ ����Ű�� ���̳�忡 ���� ������ ���´�.
	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kReadHeadCheckPoint(gs_vbTempBuffer);
	kMemCpy(&checkPoint, gs_vbTempBuffer, sizeof(CHECKPOINT));
	checkPoint.iMap[stEntry.iNumber].iNumber = 0;
	checkPoint.iMap[stEntry.iNumber].iNodeOffset = 0;

	kMemSet(gs_vbTempBuffer, 0, FILESYSTEM_CLUSTERSIZE);
	kMemCpy(gs_vbTempBuffer, &checkPoint, sizeof(CHECKPOINT));
	kWriteHeadCheckPoint(gs_vbTempBuffer);

	// ���͸� ��Ʈ���� �� ������ ����
	kMemSet(&stEntry, 0, sizeof(stEntry));
	if (kSetDirectoryEntryData(iDirectoryEntryOffset, &stEntry) == FALSE)
	{
		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return -1;
	}

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return 0;
}

/**
 *  ���͸��� ��
 */
DIR* kOpenDirectory(const char* pcDirectoryName)
{
	DIR* pstDirectory;
	DIRECTORYENTRY* pstDirectoryBuffer;

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// ��Ʈ ���͸� �ۿ� �����Ƿ� ���͸� �̸��� �����ϰ� �ڵ鸸 �Ҵ�޾Ƽ� ��ȯ
	pstDirectory = kAllocateFileDirectoryHandle();
	if (pstDirectory == NULL)
	{
		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;
	}

	// ��Ʈ ���͸��� ������ ���۸� �Ҵ�
	pstDirectoryBuffer = (DIRECTORYENTRY*)kAllocateMemory(FILESYSTEM_CLUSTERSIZE);
	if (pstDirectory == NULL)
	{
		// �����ϸ� �ڵ��� ��ȯ�ؾ� ��
		kFreeFileDirectoryHandle(pstDirectory);
		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;
	}

	// ��Ʈ ���͸��� ����
	if (kReadRootDirectory((BYTE*)pstDirectoryBuffer) == FALSE)
	{
		// �����ϸ� �ڵ�� �޸𸮸� ��� ��ȯ�ؾ� ��
		kFreeFileDirectoryHandle(pstDirectory);
		kFreeMemory(pstDirectoryBuffer);

		// ����ȭ
		kUnlock(&(gs_stFileSystemManager.stMutex));
		return NULL;

	}

	// ���͸� Ÿ������ �����ϰ� ���� ���͸� ��Ʈ���� �������� �ʱ�ȭ
	pstDirectory->bType = FILESYSTEM_TYPE_DIRECTORY;
	pstDirectory->stDirectoryHandle.iCurrentOffset = 0;
	pstDirectory->stDirectoryHandle.pstDirectoryBuffer = pstDirectoryBuffer;

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return pstDirectory;
}

/**
 *  ���͸� ��Ʈ���� ��ȯ�ϰ� �������� �̵�
 */
struct kDirectoryEntryStruct* kReadDirectory(DIR* pstDirectory)
{
	DIRECTORYHANDLE* pstDirectoryHandle;
	DIRECTORYENTRY* pstEntry;

	// �ڵ� Ÿ���� ���͸��� �ƴϸ� ����
	if ((pstDirectory == NULL) ||
		(pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
	{
		return NULL;
	}
	pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

	// �������� ������ Ŭ�����Ϳ� �����ϴ� �ִ��� �Ѿ�� ����
	if ((pstDirectoryHandle->iCurrentOffset < 0) ||
		(pstDirectoryHandle->iCurrentOffset >= FILESYSTEM_MAXDIRECTORYENTRYCOUNT))
	{
		return NULL;
	}

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// ��Ʈ ���͸��� �ִ� �ִ� ���͸� ��Ʈ���� ������ŭ �˻�
	pstEntry = pstDirectoryHandle->pstDirectoryBuffer;
	while (pstDirectoryHandle->iCurrentOffset < FILESYSTEM_MAXDIRECTORYENTRYCOUNT)
	{
		// ������ �����ϸ� �ش� ���͸� ��Ʈ���� ��ȯ
		if (pstEntry[pstDirectoryHandle->iCurrentOffset].iNumber != 0)
		{
			// ����ȭ
			kUnlock(&(gs_stFileSystemManager.stMutex));
			return &(pstEntry[pstDirectoryHandle->iCurrentOffset++]);
		}

		pstDirectoryHandle->iCurrentOffset++;
	}

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));
	return NULL;
}

/**
 *  ���͸� �����͸� ���͸��� ó������ �̵�
 */
void kRewindDirectory(DIR* pstDirectory)
{
	DIRECTORYHANDLE* pstDirectoryHandle;

	// �ڵ� Ÿ���� ���͸��� �ƴϸ� ����
	if ((pstDirectory == NULL) ||
		(pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
	{
		return;
	}
	pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// ���͸� ��Ʈ���� �����͸� 0���� �ٲ���
	pstDirectoryHandle->iCurrentOffset = 0;

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));
}


/**
 *  ���� ���͸��� ����
 */
int kCloseDirectory(DIR* pstDirectory)
{
	DIRECTORYHANDLE* pstDirectoryHandle;

	// �ڵ� Ÿ���� ���͸��� �ƴϸ� ����
	if ((pstDirectory == NULL) ||
		(pstDirectory->bType != FILESYSTEM_TYPE_DIRECTORY))
	{
		return -1;
	}
	pstDirectoryHandle = &(pstDirectory->stDirectoryHandle);

	// ����ȭ
	kLock(&(gs_stFileSystemManager.stMutex));

	// ��Ʈ ���͸��� ���۸� �����ϰ� �ڵ��� ��ȯ
	kFreeMemory(pstDirectoryHandle->pstDirectoryBuffer);
	kFreeFileDirectoryHandle(pstDirectory);

	// ����ȭ
	kUnlock(&(gs_stFileSystemManager.stMutex));

	return 0;
}
