#ifndef __CONSOLESHELL_H__
#define __CONSOLESHELL_H__

#include "Types.h"

////////////////////////////////////////////////////////////////////////////////
//
// ��ũ��
//
////////////////////////////////////////////////////////////////////////////////
#define CONSOLESHELL_MAXCOMMANDBUFFERCOUNT  300
#define CONSOLESHELL_PROMPTMESSAGE          "MINT64>"

// ���ڿ� �����͸� �Ķ���ͷ� �޴� �Լ� ������ Ÿ�� ����
typedef void ( * CommandFunction ) ( const char* pcParameter );


////////////////////////////////////////////////////////////////////////////////
//
// ����ü
//
////////////////////////////////////////////////////////////////////////////////
// 1����Ʈ�� ����
#pragma pack( push, 1 )

// ���� Ŀ�ǵ带 �����ϴ� �ڷᱸ��
typedef struct kShellCommandEntryStruct
{
    // Ŀ�ǵ� ���ڿ�
    char* pcCommand;
    // Ŀ�ǵ��� ����
    char* pcHelp;
    // Ŀ�ǵ带 �����ϴ� �Լ��� ������
    CommandFunction pfFunction;
} SHELLCOMMANDENTRY;

// �Ķ���͸� ó���ϱ����� ������ �����ϴ� �ڷᱸ��
typedef struct kParameterListStruct
{
    // �Ķ���� ������ ��巹��
    const char* pcBuffer;
    // �Ķ������ ����
    int iLength;
    // ���� ó���� �Ķ���Ͱ� �����ϴ� ��ġ
    int iCurrentPosition;
} PARAMETERLIST;

typedef struct tTrieStruct
{
	BOOL finish;
	int count;
	struct tTrieStruct* next[26];
}Trie;

typedef struct free_block {
    int size;
    struct free_block* next;
} free_block;

static free_block free_block_list_head = { 0, 0 };
static const int overhead = sizeof(int);
static const int align_to = 16;
static int curMallocPos;

#pragma pack( pop )

////////////////////////////////////////////////////////////////////////////////
//
// �Լ�
//
////////////////////////////////////////////////////////////////////////////////
// ���� �� �ڵ�
void kStartConsoleShell( void );
void kExecuteCommand( const char* pcCommandBuffer );
void kInitializeParameter( PARAMETERLIST* pstList, const char* pcParameter );
int kGetNextParameter( PARAMETERLIST* pstList, char* pcParameter );

// Ŀ�ǵ带 ó���ϴ� �Լ�
void kHelp( const char* pcParameterBuffer );
void kCls( const char* pcParameterBuffer );
void kShowTotalRAMSize( const char* pcParameterBuffer );
void kStringToDecimalHexTest( const char* pcParameterBuffer );
void kShutdown( const char* pcParamegerBuffer );
void kRaiseFault( const char* pcParamegerBuffer );
void kHistoryPrint(char* vcCommandBuffer, char historyBuffer[][CONSOLESHELL_MAXCOMMANDBUFFERCOUNT],
		char* lineClear, int* iCommandBufferIndex, int historyIdx);
void kCDummy(const char* pcParamegerBuffer);
void kRDummy1(const char* pcParamegerBuffer);
void kRDummy2(const char* pcParamegerBuffer);

void* kMalloc(int size);
void kFree(void* ptr);
void kTrieInitialize(Trie* trie);
void kTrieInsert(Trie* trie, const char* key);
Trie* kTrieFind(Trie* trie, const char* key);
void kTrieFindMostSpecific(Trie* trie, char* key, int* strIndex);
void kPrintEveryCandidate(Trie* trie, const char* key, char* resultBuffer, char* tempStr, int* bufferIdx, int* tempStrIdx);

#endif /*__CONSOLESHELL_H__*/
