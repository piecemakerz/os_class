#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "Types.h"
#include "Synchronization.h"
#include "HardDisk.h"

////////////////////////////////////////////////////////////////////////////////
//
// 매크로와 함수 포인터
//
////////////////////////////////////////////////////////////////////////////////
// MINT 파일 시스템 시그너처(Signature)
#define FILESYSTEM_SIGNATURE                0x7E38CF10
// 클러스터의 크기(섹터 수), 4Kbyte
#define FILESYSTEM_SECTORSPERCLUSTER        8
// 파일 클러스터의 마지막 표시
#define FILESYSTEM_LASTCLUSTER              0xFFFFFFFF
// 빈 클러스터 표시
#define FILESYSTEM_FREECLUSTER              0x00
// 루트 디렉터리에 있는 최대 디렉터리 엔트리의 수
#define FILESYSTEM_MAXDIRECTORYENTRYCOUNT   128
// 클러스터의 크기(바이트 수)
#define FILESYSTEM_CLUSTERSIZE              ( FILESYSTEM_SECTORSPERCLUSTER * 512 )

// 핸들의 최대 개수, 최대 태스크 수의 3배로 생성
#define FILESYSTEM_HANDLE_MAXCOUNT          ( TASK_MAXCOUNT * 3 )
#define FILESYSTEM_MAXSEGMENTCLUSTERCOUNT	64
#define FILESYSTEM_MAXSEGMENTSIZE			( FILESYSTEM_CLUSTERSIZE * FILESYSTEM_MAXSEGMENTCLUSTERCOUNT)
// 파일 이름의 최대 길이
#define FILESYSTEM_MAXFILENAMELENGTH        24
#define FILESYSTEM_SEGMENTNUM				2
// 핸들의 타입을 정의
#define FILESYSTEM_TYPE_FREE                0
#define FILESYSTEM_TYPE_FILE                1
#define FILESYSTEM_TYPE_DIRECTORY           2

// SEEK 옵션 정의
#define FILESYSTEM_SEEK_SET                 0
#define FILESYSTEM_SEEK_CUR                 1
#define FILESYSTEM_SEEK_END                 2

// 하드 디스크 제어에 관련된 함수 포인터 타입 정의
typedef BOOL(*fReadHDDInformation) (BOOL bPrimary, BOOL bMaster,
	HDDINFORMATION* pstHDDInformation);
typedef int (*fReadHDDSector) (BOOL bPrimary, BOOL bMaster, DWORD dwLBA,
	int iSectorCount, char* pcBuffer);
typedef int (*fWriteHDDSector) (BOOL bPrimary, BOOL bMaster, DWORD dwLBA,
	int iSectorCount, char* pcBuffer);

// MINT 파일 시스템 함수를 표준 입출력 함수 이름으로 재정의
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

// MINT 파일 시스템 매크로를 표준 입출력의 매크로를 재정의
#define SEEK_SET    FILESYSTEM_SEEK_SET
#define SEEK_CUR    FILESYSTEM_SEEK_CUR
#define SEEK_END    FILESYSTEM_SEEK_END

// MINT 파일 시스템 타입과 필드를 표준 입출력의 타입으로 재정의
#define size_t      DWORD
#define dirent      kDirectoryEntryStruct
#define d_name      vcFileName
////////////////////////////////////////////////////////////////////////////////
//
// 구조체
//
////////////////////////////////////////////////////////////////////////////////
// 1바이트로 정렬
#pragma pack( push, 1 )

// 파티션 자료구조
typedef struct kPartitionStruct
{
	// 부팅 가능 플래그. 0x80이면 부팅 가능을 나타내며 0x00은 부팅 불가
	BYTE bBootableFlag;
	// 파티션의 시작 어드레스. 현재는 거의 사용하지 않으며 아래의 LBA 어드레스를 대신 사용
	BYTE vbStartingCHSAddress[3];
	// 파티션 타입
	BYTE bPartitionType;
	// 파티션의 마지막 어드레스. 현재는 거의 사용 안 함
	BYTE vbEndingCHSAddress[3];
	// 파티션의 시작 어드레스. LBA 어드레스로 나타낸 값
	DWORD dwStartingLBAAddress;
	// 파티션에 포함된 섹터 수
	DWORD dwSizeInSector;
} PARTITION;


// MBR 자료구조
typedef struct kMBRStruct
{
	// 부트 로더 코드가 위치하는 영역
	BYTE vbBootCode[430];

	// 파일 시스템 시그너처, 0x7E38CF10
	DWORD dwSignature;
	// 예약된 영역의 섹터 수
	DWORD dwReservedSectorCount;
	// 클러스터의 전체 개수
	DWORD dwTotalClusterCount;

	// 파티션 테이블
	PARTITION vstPartition[4];

	// 부트 로더 시그너처, 0x55, 0xAA
	BYTE vbBootLoaderSignature[2];
} MBR;

// 디렉터리 엔트리 자료구조
// 루트 디렉터리 엔트리는 최대 128개의 엔트리를 가질 수 있다.
// 파일 아이노드 번호가 0이면 빈 디렉터리 엔트리이다.
typedef struct kDirectoryEntryStruct
{
	// 파일 이름
	char vcFileName[FILESYSTEM_MAXFILENAMELENGTH];
	// 파일의 아이노드 번호
	DWORD iNumber;
	// 파일의 실제 크기 (바이트 단위)
	DWORD dwFileSize;

} DIRECTORYENTRY;

#pragma pack( pop )

// 파일을 관리하는 파일 핸들 자료구조
typedef struct kFileHandleStruct
{
	// 파일이 존재하는 디렉터리 엔트리의 오프셋
	int iDirectoryEntryOffset;
	// 파일 아이노드 번호
	DWORD iNumber;
	// 파일 크기 (바이트 단위)
	DWORD dwFileSize;
	// 파일의 시작 주소 오프셋(섹터 오프셋)
	// 0이면 데이터가 없는 것을 의미
	DWORD dwStartFileOffset;
	// 현재 I/O가 수행중인 파일의 오프셋 (데이터 블록 번호)
	DWORD dwCurrentFileOffset;
	// 현재 파일 데이터 블록에서의 바이트 단위 오프셋
	DWORD dwCurrentOffset;
} FILEHANDLE;

// 디렉터리를 관리하는 디렉터리 핸들 자료구조
typedef struct kDirectoryHandleStruct
{
	// 루트 디렉터리를 저장해둔 버퍼
	DIRECTORYENTRY* pstDirectoryBuffer;

	// 디렉터리 포인터의 현재 위치
	int iCurrentOffset;
} DIRECTORYHANDLE;

// 파일과 디렉터리에 대한 정보가 들어있는 자료구조
typedef struct kFileDirectoryHandleStruct
{
	// 자료구조의 타입 설정. 파일 핸들이나 디렉터리 핸들, 또는 빈 핸들 타입 지정 가능
	BYTE bType;

	// bType의 값에 따라 파일 또는 디렉터리로 사용
	union
	{
		// 파일 핸들
		FILEHANDLE stFileHandle;
		// 디렉터리 핸들
		DIRECTORYHANDLE stDirectoryHandle;
	};
} FILE, DIR;

// 파일 시스템을 관리하는 구조체
typedef struct kFileSystemManagerStruct
{
	// 파일 시스템이 정상적으로 인식되었는지 여부
	BOOL bMounted;

	// 각 영역의 섹터 수와 시작 LBA 어드레스
	DWORD dwReservedSectorCount;
	// 디스크 상의 헤드 체크포인트 시작 주소 (섹터 단위 오프셋)
	DWORD dwHeadCheckPointStartAddress;
	// 디스크 상의 루트 디렉터리 시작 주소 (섹터 단위 오프셋)
	DWORD dwRootDirStartAddress;
	// 디스크 상의 데이터 영역 시작 주소 (섹터 단위 오프셋)
	// 루트 디렉터리 영역 제외
	DWORD dwDataAreaStartAddress;
	// 데이터 영역의 클러스터의 총 개수
	// 루트 디렉터리 영역 제외
	DWORD dwTotalClusterCount;

	// 다음에 디스크에 쓰기를 시작할 디스크 주소 (섹터 단위 오프셋)
	DWORD dwNextAllocateClusterOffset;

	// 파일 시스템 동기화 객체
	MUTEX stMutex;

	// 핸들 풀(Handle Pool)의 어드레스
	FILE* pstHandlePool;
} FILESYSTEMMANAGER;

// 블록의 최신 여부를 판단하기 위한 세그먼트 요약 블록
// 각 세그먼트의 헤드에 위치한다.
typedef struct kSegmentSummaryBlock
{
	// 세그먼트 내의 각 데이터 블록에 대해
	// 해당 데이터 블록이 속한 파일의 아이넘버와 파일 내 오프셋(파일의 몇 번째 데이터 블록인가)을 기록
	// 해당 블록이 메타데이터 블록이면 inumbers의 값은 0이다.
	DWORD inumbers[FILESYSTEM_MAXSEGMENTCLUSTERCOUNT];
	DWORD offsets[FILESYSTEM_MAXSEGMENTCLUSTERCOUNT];
} SEGMENTSUMMARYBLOCK;

// 각 세그먼트는 메모리에 할당됨
typedef struct kSegmentStruct
{
	// 각 세그먼트의 헤드에는 세그먼트 요약 블록이 있음
	SEGMENTSUMMARYBLOCK SS;
	// 세그먼트가 디스크 상에 위치될 시작 오프셋 (섹터 단위)
	DWORD segmentStartOffset;
	// 다음 세그먼트가 위치할 오프셋 (섹터 단위)
	DWORD nextSegmentOffset;
	// 현재까지 세그먼트에 저장된 블록의 수
	DWORD curBlockCnt;
	// 각 데이터 블럭의 데이터 타입
	// 데이터(0), 아이노드(1), 아이노드 맵 조각(2)
	DWORD dataBlockType[FILESYSTEM_MAXSEGMENTCLUSTERCOUNT];
	// 데이터가 저장될 영역
	BYTE dataRegion[FILESYSTEM_MAXSEGMENTSIZE];
} SEGMENT;

// 파일에 대한 아이노드 자료구조
typedef struct kInode
{
	// 파일의 아이노드 번호
	DWORD iNumber;
	// 파일의 크기
	DWORD size;
	// 현재 파일의 데이터 블록 개수
	DWORD curDataBlocks;
	// 데이터 블록 포인터들
	// 간접 데이터 블록 포인터들은 고려하지 않으므로 최대 12개 블럭 크기의 파일.
	// 파일의 각 데이터 블록이 위치하는 디스크 섹터 오프셋
	DWORD dataBlockOffset[12];
	// 각 데이터 블럭에 실제로 저장되어 있는 데이터 바이트
	DWORD dataBlockActualDataSize[12];
} INODE;

// 아이노드의 위치를 가리키는 아이노드 맵 조각 하나
// 세그먼트 당 하나의 아이노드 맵이 할당된다.
// 아이노드 맵은 아이넘버를 할당받아서 아이노드의 디스크 주소를 반환한다.
// 디스크를 읽을 때에는 모든 세그먼트의 아이노드 맵이 메모리에 캐싱한 후
// 가장 최신의 아이노드 정보를 찾는다.
typedef struct kInodeMapBlock
{
	DWORD iNumber;
	DWORD iNodeOffset;
} INODEMAPBLOCK;

// 아이노드 맵 위치를 가리키는 체크포인트 영역
// 디스크 상의 맨 앞(MBR 이후)과 맨 뒤에 두 개가 존재한다.

typedef struct kCheckpointRegion
{
	// 아이노드 맵 블록의 디스크 상 위치 (섹터 단위)
	// iMap[아이노드 번호]
	INODEMAPBLOCK iMap[FILESYSTEM_MAXDIRECTORYENTRYCOUNT + 1];
	// 가장 오래된 세그먼트 오프셋(섹터 단위)
	DWORD headSegmentOffset;
	// 가장 최근의 세그먼트 오프셋(섹터 단위)
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
