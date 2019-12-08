#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "Types.h"
#include "Synchronization.h"
#include "HardDisk.h"

////////////////////////////////////////////////////////////////////////////////
//
// ��ũ�ο� �Լ� ������
//
////////////////////////////////////////////////////////////////////////////////
// MINT ���� �ý��� �ñ׳�ó(Signature)
#define FILESYSTEM_SIGNATURE                0x7E38CF10
// Ŭ�������� ũ��(���� ��), 4Kbyte
#define FILESYSTEM_SECTORSPERCLUSTER        8
// ���� Ŭ�������� ������ ǥ��
#define FILESYSTEM_LASTCLUSTER              0xFFFFFFFF
// �� Ŭ������ ǥ��
#define FILESYSTEM_FREECLUSTER              0x00
// ��Ʈ ���͸��� �ִ� �ִ� ���͸� ��Ʈ���� ��
#define FILESYSTEM_MAXDIRECTORYENTRYCOUNT   128
// Ŭ�������� ũ��(����Ʈ ��)
#define FILESYSTEM_CLUSTERSIZE              ( FILESYSTEM_SECTORSPERCLUSTER * 512 )

// �ڵ��� �ִ� ����, �ִ� �½�ũ ���� 3��� ����
#define FILESYSTEM_HANDLE_MAXCOUNT          ( TASK_MAXCOUNT * 3 )
#define FILESYSTEM_MAXSEGMENTCLUSTERCOUNT	64
#define FILESYSTEM_MAXSEGMENTSIZE			( FILESYSTEM_CLUSTERSIZE * FILESYSTEM_MAXSEGMENTCLUSTERCOUNT)
// ���� �̸��� �ִ� ����
#define FILESYSTEM_MAXFILENAMELENGTH        24
#define FILESYSTEM_SEGMENTNUM				2
// �ڵ��� Ÿ���� ����
#define FILESYSTEM_TYPE_FREE                0
#define FILESYSTEM_TYPE_FILE                1
#define FILESYSTEM_TYPE_DIRECTORY           2

// SEEK �ɼ� ����
#define FILESYSTEM_SEEK_SET                 0
#define FILESYSTEM_SEEK_CUR                 1
#define FILESYSTEM_SEEK_END                 2

// �ϵ� ��ũ ��� ���õ� �Լ� ������ Ÿ�� ����
typedef BOOL(*fReadHDDInformation) (BOOL bPrimary, BOOL bMaster,
	HDDINFORMATION* pstHDDInformation);
typedef int (*fReadHDDSector) (BOOL bPrimary, BOOL bMaster, DWORD dwLBA,
	int iSectorCount, char* pcBuffer);
typedef int (*fWriteHDDSector) (BOOL bPrimary, BOOL bMaster, DWORD dwLBA,
	int iSectorCount, char* pcBuffer);

// MINT ���� �ý��� �Լ��� ǥ�� ����� �Լ� �̸����� ������
#define fopen       kOpenFile
#define fread       kReadFile
#define fwrite      kWriteFile
#define fseek       kSeekFile
#define fclose      kCloseFile
#define remove      kRemoveFile
#define opendir     kOpenDirectory
#define readdir     kReadDirectory
#define rewinddir   kRewindDirectory
#define closedir    kCloseDirectory

// MINT ���� �ý��� ��ũ�θ� ǥ�� ������� ��ũ�θ� ������
#define SEEK_SET    FILESYSTEM_SEEK_SET
#define SEEK_CUR    FILESYSTEM_SEEK_CUR
#define SEEK_END    FILESYSTEM_SEEK_END

// MINT ���� �ý��� Ÿ�԰� �ʵ带 ǥ�� ������� Ÿ������ ������
#define size_t      DWORD
#define dirent      kDirectoryEntryStruct
#define d_name      vcFileName
////////////////////////////////////////////////////////////////////////////////
//
// ����ü
//
////////////////////////////////////////////////////////////////////////////////
// 1����Ʈ�� ����
#pragma pack( push, 1 )

// ��Ƽ�� �ڷᱸ��
typedef struct kPartitionStruct
{
	// ���� ���� �÷���. 0x80�̸� ���� ������ ��Ÿ���� 0x00�� ���� �Ұ�
	BYTE bBootableFlag;
	// ��Ƽ���� ���� ��巹��. ����� ���� ������� ������ �Ʒ��� LBA ��巹���� ��� ���
	BYTE vbStartingCHSAddress[3];
	// ��Ƽ�� Ÿ��
	BYTE bPartitionType;
	// ��Ƽ���� ������ ��巹��. ����� ���� ��� �� ��
	BYTE vbEndingCHSAddress[3];
	// ��Ƽ���� ���� ��巹��. LBA ��巹���� ��Ÿ�� ��
	DWORD dwStartingLBAAddress;
	// ��Ƽ�ǿ� ���Ե� ���� ��
	DWORD dwSizeInSector;
} PARTITION;


// MBR �ڷᱸ��
typedef struct kMBRStruct
{
	// ��Ʈ �δ� �ڵ尡 ��ġ�ϴ� ����
	BYTE vbBootCode[430];

	// ���� �ý��� �ñ׳�ó, 0x7E38CF10
	DWORD dwSignature;
	// ����� ������ ���� ��
	DWORD dwReservedSectorCount;
	// Ŭ�������� ��ü ����
	DWORD dwTotalClusterCount;

	// ��Ƽ�� ���̺�
	PARTITION vstPartition[4];

	// ��Ʈ �δ� �ñ׳�ó, 0x55, 0xAA
	BYTE vbBootLoaderSignature[2];
} MBR;

// ���͸� ��Ʈ�� �ڷᱸ��
// ��Ʈ ���͸� ��Ʈ���� �ִ� 128���� ��Ʈ���� ���� �� �ִ�.
// ���� ���̳�� ��ȣ�� 0�̸� �� ���͸� ��Ʈ���̴�.
typedef struct kDirectoryEntryStruct
{
	// ���� �̸�
	char vcFileName[FILESYSTEM_MAXFILENAMELENGTH];
	// ������ ���̳�� ��ȣ
	DWORD iNumber;
	// ������ ���� ũ�� (����Ʈ ����)
	DWORD dwFileSize;

} DIRECTORYENTRY;

#pragma pack( pop )

// ������ �����ϴ� ���� �ڵ� �ڷᱸ��
typedef struct kFileHandleStruct
{
	// ������ �����ϴ� ���͸� ��Ʈ���� ������
	int iDirectoryEntryOffset;
	// ���� ���̳�� ��ȣ
	DWORD iNumber;
	// ���� ũ�� (����Ʈ ����)
	DWORD dwFileSize;
	// ������ ���� �ּ� ������(���� ������)
	// 0�̸� �����Ͱ� ���� ���� �ǹ�
	DWORD dwStartFileOffset;
	// ���� I/O�� �������� ������ ������ (������ ��� ��ȣ)
	DWORD dwCurrentFileOffset;
	// ���� ���� ������ ��Ͽ����� ����Ʈ ���� ������
	DWORD dwCurrentOffset;
} FILEHANDLE;

// ���͸��� �����ϴ� ���͸� �ڵ� �ڷᱸ��
typedef struct kDirectoryHandleStruct
{
	// ��Ʈ ���͸��� �����ص� ����
	DIRECTORYENTRY* pstDirectoryBuffer;

	// ���͸� �������� ���� ��ġ
	int iCurrentOffset;
} DIRECTORYHANDLE;

// ���ϰ� ���͸��� ���� ������ ����ִ� �ڷᱸ��
typedef struct kFileDirectoryHandleStruct
{
	// �ڷᱸ���� Ÿ�� ����. ���� �ڵ��̳� ���͸� �ڵ�, �Ǵ� �� �ڵ� Ÿ�� ���� ����
	BYTE bType;

	// bType�� ���� ���� ���� �Ǵ� ���͸��� ���
	union
	{
		// ���� �ڵ�
		FILEHANDLE stFileHandle;
		// ���͸� �ڵ�
		DIRECTORYHANDLE stDirectoryHandle;
	};
} FILE, DIR;

// ���� �ý����� �����ϴ� ����ü
typedef struct kFileSystemManagerStruct
{
	// ���� �ý����� ���������� �νĵǾ����� ����
	BOOL bMounted;

	// �� ������ ���� ���� ���� LBA ��巹��
	DWORD dwReservedSectorCount;
	// ��ũ ���� ��� üũ����Ʈ ���� �ּ� (���� ���� ������)
	DWORD dwHeadCheckPointStartAddress;
	// ��ũ ���� ��Ʈ ���͸� ���� �ּ� (���� ���� ������)
	DWORD dwRootDirStartAddress;
	// ��ũ ���� ������ ���� ���� �ּ� (���� ���� ������)
	// ��Ʈ ���͸� ���� ����
	DWORD dwDataAreaStartAddress;
	// ������ ������ Ŭ�������� �� ����
	// ��Ʈ ���͸� ���� ����
	DWORD dwTotalClusterCount;

	// ������ ��ũ�� ���⸦ ������ ��ũ �ּ� (���� ���� ������)
	DWORD dwNextAllocateClusterOffset;

	// ���� �ý��� ����ȭ ��ü
	MUTEX stMutex;

	// �ڵ� Ǯ(Handle Pool)�� ��巹��
	FILE* pstHandlePool;
} FILESYSTEMMANAGER;

// ����� �ֽ� ���θ� �Ǵ��ϱ� ���� ���׸�Ʈ ��� ���
// �� ���׸�Ʈ�� ��忡 ��ġ�Ѵ�.
typedef struct kSegmentSummaryBlock
{
	// ���׸�Ʈ ���� �� ������ ��Ͽ� ����
	// �ش� ������ ����� ���� ������ ���̳ѹ��� ���� �� ������(������ �� ��° ������ ����ΰ�)�� ���
	// �ش� ����� ��Ÿ������ ����̸� inumbers�� ���� 0�̴�.
	DWORD inumbers[FILESYSTEM_MAXSEGMENTCLUSTERCOUNT];
	DWORD offsets[FILESYSTEM_MAXSEGMENTCLUSTERCOUNT];
} SEGMENTSUMMARYBLOCK;

// �� ���׸�Ʈ�� �޸𸮿� �Ҵ��
typedef struct kSegmentStruct
{
	// �� ���׸�Ʈ�� ��忡�� ���׸�Ʈ ��� ����� ����
	SEGMENTSUMMARYBLOCK SS;
	// ���׸�Ʈ�� ��ũ �� ��ġ�� ���� ������ (���� ����)
	DWORD segmentStartOffset;
	// ���� ���׸�Ʈ�� ��ġ�� ������ (���� ����)
	DWORD nextSegmentOffset;
	// ������� ���׸�Ʈ�� ����� ����� ��
	DWORD curBlockCnt;
	// �� ������ ���� ������ Ÿ��
	// ������(0), ���̳��(1), ���̳�� �� ����(2)
	DWORD dataBlockType[FILESYSTEM_MAXSEGMENTCLUSTERCOUNT];
	// �����Ͱ� ����� ����
	BYTE dataRegion[FILESYSTEM_MAXSEGMENTSIZE];
} SEGMENT;

// ���Ͽ� ���� ���̳�� �ڷᱸ��
typedef struct kInode
{
	// ������ ���̳�� ��ȣ
	DWORD iNumber;
	// ������ ũ��
	DWORD size;
	// ���� ������ ������ ��� ����
	DWORD curDataBlocks;
	// ������ ��� �����͵�
	// ���� ������ ��� �����͵��� ������� �����Ƿ� �ִ� 12�� �� ũ���� ����.
	// ������ �� ������ ����� ��ġ�ϴ� ��ũ ���� ������
	DWORD dataBlockOffset[12];
	// �� ������ ���� ������ ����Ǿ� �ִ� ������ ����Ʈ
	DWORD dataBlockActualDataSize[12];
} INODE;

// ���̳���� ��ġ�� ����Ű�� ���̳�� �� ���� �ϳ�
// ���׸�Ʈ �� �ϳ��� ���̳�� ���� �Ҵ�ȴ�.
// ���̳�� ���� ���̳ѹ��� �Ҵ�޾Ƽ� ���̳���� ��ũ �ּҸ� ��ȯ�Ѵ�.
// ��ũ�� ���� ������ ��� ���׸�Ʈ�� ���̳�� ���� �޸𸮿� ĳ���� ��
// ���� �ֽ��� ���̳�� ������ ã�´�.
typedef struct kInodeMapBlock
{
	DWORD iNumber;
	DWORD iNodeOffset;
} INODEMAPBLOCK;

// ���̳�� �� ��ġ�� ����Ű�� üũ����Ʈ ����
// ��ũ ���� �� ��(MBR ����)�� �� �ڿ� �� ���� �����Ѵ�.

typedef struct kCheckpointRegion
{
	// ���̳�� �� ����� ��ũ �� ��ġ (���� ����)
	// iMap[���̳�� ��ȣ]
	INODEMAPBLOCK iMap[FILESYSTEM_MAXDIRECTORYENTRYCOUNT + 1];
	// ���� ������ ���׸�Ʈ ������(���� ����)
	DWORD headSegmentOffset;
	// ���� �ֱ��� ���׸�Ʈ ������(���� ����)
	DWORD tailSegmentOffset;
} CHECKPOINT;

BOOL kFormat(void);
BOOL kInitializeFileSystem(void);
BOOL kMount(void);
BOOL kGetHDDInformation(HDDINFORMATION* pstInformation);
static BOOL kReadCluster(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kWriteCluster(DWORD dwOffset, BYTE* pbBuffer);
static BOOL kReadRootDirectory(BYTE* pbBuffer);
static BOOL kWriteRootDirectory(BYTE* pbBuffer);
static BOOL kReadHeadCheckPoint(BYTE* pbBuffer);
static BOOL kWriteHeadCheckPoint(BYTE* pbBuffer);
static BOOL kReadINode(DWORD inumber, BYTE* pbBuffer);
static BOOL kWriteSegment(DWORD segmentNum, DWORD type, INODE* inode, BYTE* pbBuffer);
static BOOL kWriteSegmentToDisk(DWORD segmentNum);
static DWORD kFindFreeINode(void);
static int kFindFreeDirectoryEntry(void);
static BOOL kSetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry);
static BOOL kGetDirectoryEntryData(int iIndex, DIRECTORYENTRY* pstEntry);
static int kFindDirectoryEntry(const char* pcFileName, DIRECTORYENTRY* pstEntry);
void kGetFileSystemInformation(FILESYSTEMMANAGER* pstManager);
static void* kAllocateFileDirectoryHandle(void);
static void kFreeFileDirectoryHandle(FILE* pstFile);
static BOOL kCreateFile(const char* pcFileName, DIRECTORYENTRY* pstEntry,
	int* piDirectoryEntryIndex);
FILE* kOpenFile(const char* pcFileName, const char* pcMode);
DWORD kReadFile(void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile);
static BOOL kUpdateDirectoryEntry(FILEHANDLE* pstFileHandle);
DWORD kWriteFile(const void* pvBuffer, DWORD dwSize, DWORD dwCount, FILE* pstFile);
int kSeekFile(FILE* pstFile, int iOffset, int iOrigin);
int kCloseFile(FILE* pstFile);
BOOL kIsFileOpened(const DIRECTORYENTRY* pstEntry);
int kRemoveFile(const char* pcFileName);
DIR* kOpenDirectory(const char* pcDirectoryName);
struct kDirectoryEntryStruct* kReadDirectory(DIR* pstDirectory);
void kRewindDirectory(DIR* pstDirectory);
int kCloseDirectory(DIR* pstDirectory);
#endif /*__FILESYSTEM_H__*/
