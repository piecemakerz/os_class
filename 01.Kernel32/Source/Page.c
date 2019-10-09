#include "Page.h"

/**
 *	IA-32e ��� Ŀ���� ���� ������ ���̺� ����
 */
void kInitializePageTables( void )
{
	PML4TENTRY* pstPML4TEntry;
	PDPTENTRY* pstPDPTEntry;
	PDENTRY* pstPDEntry;
	PTENTRY* pstPTEntry;
	DWORD dwMappingAddress;
	int i;

	// PML4 ���̺� ����
	// ù ��° ��Ʈ�� �ܿ� �������� ��� 0���� �ʱ�ȭ
	pstPML4TEntry = ( PML4TENTRY* ) 0x100000;
	kSetPageEntryData( &( pstPML4TEntry[ 0 ] ), 0x00, 0x101000, PAGE_FLAGS_DEFAULT,
			0 );
	for( i = 1 ; i < PAGE_MAXENTRYCOUNT ; i++ )
	{
		kSetPageEntryData( &( pstPML4TEntry[ i ] ), 0, 0, 0, 0 );
	}
	
	// ������ ���͸� ������ ���̺� ����
	// �ϳ��� PDPT�� 512GByte���� ���� �����ϹǷ� �ϳ��� �����
	// 64���� ��Ʈ���� �����Ͽ� 64GByte���� ������
	pstPDPTEntry = ( PDPTENTRY* ) 0x101000;
	for( i = 0 ; i < 64 ; i++ )
	{
		kSetPageEntryData( &( pstPDPTEntry[ i ] ), 0, 0x102000 + ( i * PAGE_TABLESIZE ), 
				PAGE_FLAGS_DEFAULT, 0 );
	}
	for( i = 64 ; i < PAGE_MAXENTRYCOUNT ; i++ )
	{
		kSetPageEntryData( &( pstPDPTEntry[ i ] ), 0, 0, 0, 0 );
	}
	
	// ������ ���͸� ���̺� ����
	// �ϳ��� ������ ���͸��� 1GByte���� ���� ���� 
	// �����ְ� 64���� ������ ���͸��� �����Ͽ� �� 64GB���� ����
	pstPDEntry = ( PDENTRY* ) 0x102000;
	// ������ ���̺� ��ġ �ʱ�ȭ
	pstPTEntry = ( PTENTRY* ) 0x142000;
	dwMappingAddress = 0;
	for( i = 0 ; i < PAGE_MAXENTRYCOUNT * 64 ; i++ )
	{
		// ���� ���� ����
		// 0xAB8000�� 0xB800�� �����ϱ� ����  0xA00000 ~ 0xC00000 �޸� ������
		// 0x000000 ~ 0x200000 �������� ���ν�Ų��.
		DWORD dwUpperAddress = (( i * ( PAGE_DEFAULTSIZE >> 20 ) ) >> 12);
		if(dwUpperAddress == 0 && dwMappingAddress == 0xA00000)
		{
			kSetPageEntryData(&(pstPDEntry[i]), dwUpperAddress, 0x0000, PAGE_FLAGS_DEFAULT | PAGE_FLAGS_PS, 0);
		}

		// 0~2MB �޸� ������ 2MB ������ ��� 4KB ������ ũ��� �����Ѵ�.
		// �̸� ����, 0~2MB �޸� ������ 4KB �������� ����Ű�� ������ ���̺� 1���� �����Ѵ�.
		else if(dwUpperAddress == 0 && dwMappingAddress == 0)
		{
			kSetPageEntryData(&(pstPDEntry[i]), dwUpperAddress, pstPTEntry, PAGE_FLAGS_DEFAULT, 0);
		}
		else
		{
			// 32��Ʈ�δ� ���� ��巹���� ǥ���� �� �����Ƿ�, Mbyte ������ ����� ����
			// ���� ����� �ٽ� 4Kbyte�� ������ 32��Ʈ �̻��� ��巹���� �����
			kSetPageEntryData( &( pstPDEntry[ i ] ), dwUpperAddress, dwMappingAddress,
					PAGE_FLAGS_DEFAULT | PAGE_FLAGS_PS, 0 );
		}
		dwMappingAddress += PAGE_DEFAULTSIZE;
	}

	// 0 ~ 2MB �޸� ������ 4KB �������� ����Ű�� ������ ���̺� 1���� �����Ѵ�.
	// �ϳ��� ������ ���̺��� 512���� 8����Ʈ ��Ʈ���� ������.
	for(i = 0; i < 512; i++)
	{
		DWORD dwLowerAddress = (i * PAGE_5STEPSIZE);
		// �ֻ��� �޸𸮰����� 0x1ff000 ������ read only �Ӽ��� �ο��Ѵ�.
		if(i == 511)
		{
			kSetPageEntryData( &( pstPTEntry[ i ] ), 0, dwLowerAddress,	PAGE_FLAGS_P, 0 );
		}
		else
		{
			kSetPageEntryData( &( pstPTEntry[ i ] ), 0, dwLowerAddress,	PAGE_FLAGS_DEFAULT, 0 );
		}
	}

}

/**
 *	������ ��Ʈ���� ���� �ּҿ� �Ӽ� �÷��׸� ����
 */
void kSetPageEntryData( PTENTRY* pstEntry, DWORD dwUpperBaseAddress,
		DWORD dwLowerBaseAddress, DWORD dwLowerFlags, DWORD dwUpperFlags )
{
	pstEntry->dwAttributeAndLowerBaseAddress = dwLowerBaseAddress | dwLowerFlags;
	pstEntry->dwUpperBaseAddressAndEXB = ( dwUpperBaseAddress & 0xFF ) | 
		dwUpperFlags;
}
