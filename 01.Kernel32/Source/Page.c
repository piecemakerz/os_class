#include "Page.h"

/**
 *	IA-32e 모드 커널을 위한 페이지 테이블 생성
 */
void kInitializePageTables( void )
{
	PML4TENTRY* pstPML4TEntry;
	PDPTENTRY* pstPDPTEntry;
	PDENTRY* pstPDEntry;
	PTENTRY* pstPTEntry;
	DWORD dwMappingAddress;
	int i;

	// PML4 테이블 생성
	// 첫 번째 엔트리 외에 나머지는 모두 0으로 초기화
	pstPML4TEntry = ( PML4TENTRY* ) 0x100000;
	kSetPageEntryData( &( pstPML4TEntry[ 0 ] ), 0x00, 0x101000, PAGE_FLAGS_DEFAULT,
			0 );
	for( i = 1 ; i < PAGE_MAXENTRYCOUNT ; i++ )
	{
		kSetPageEntryData( &( pstPML4TEntry[ i ] ), 0, 0, 0, 0 );
	}
	
	// 페이지 디렉터리 포인터 테이블 생성
	// 하나의 PDPT로 512GByte까지 매핑 가능하므로 하나로 충분함
	// 64개의 엔트리를 설정하여 64GByte까지 매핑함
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
	
	// 페이지 디렉터리 테이블 생성
	// 하나의 페이지 디렉터리가 1GByte까지 매핑 가능 
	// 여유있게 64개의 페이지 디렉터리를 생성하여 총 64GB까지 지원
	pstPDEntry = ( PDENTRY* ) 0x102000;
	// 페이지 테이블 위치 초기화
	pstPTEntry = ( PTENTRY* ) 0x142000;
	dwMappingAddress = 0;
	for( i = 0 ; i < PAGE_MAXENTRYCOUNT * 64 ; i++ )
	{
		// 더블 맵핑 구현
		// 0xAB8000을 0xB800에 맵핑하기 위해  0xA00000 ~ 0xC00000 메모리 공간을
		// 0x000000 ~ 0x200000 페이지에 맵핑시킨다.
		DWORD dwUpperAddress = (( i * ( PAGE_DEFAULTSIZE >> 20 ) ) >> 12);
		if(dwUpperAddress == 0 && dwMappingAddress == 0xA00000)
		{
			kSetPageEntryData(&(pstPDEntry[i]), dwUpperAddress, 0x0000, PAGE_FLAGS_DEFAULT | PAGE_FLAGS_PS, 0);
		}

		// 0~2MB 메모리 공간은 2MB 페이지 대신 4KB 페이지 크기로 관리한다.
		// 이를 위해, 0~2MB 메모리 공간은 4KB 페이지를 가리키는 페이지 테이블 1개로 관리한다.
		else if(dwUpperAddress == 0 && dwMappingAddress == 0)
		{
			kSetPageEntryData(&(pstPDEntry[i]), dwUpperAddress, pstPTEntry, PAGE_FLAGS_DEFAULT, 0);
		}
		else
		{
			// 32비트로는 상위 어드레스를 표현할 수 없으므로, Mbyte 단위로 계산한 다음
			// 최종 결과를 다시 4Kbyte로 나누어 32비트 이상의 어드레스를 계산함
			kSetPageEntryData( &( pstPDEntry[ i ] ), dwUpperAddress, dwMappingAddress,
					PAGE_FLAGS_DEFAULT | PAGE_FLAGS_PS, 0 );
		}
		dwMappingAddress += PAGE_DEFAULTSIZE;
	}

	// 0 ~ 2MB 메모리 공간은 4KB 페이지를 가리키는 페이지 테이블 1개로 관리한다.
	// 하나의 페이지 테이블은 512개의 8바이트 엔트리를 가진다.
	for(i = 0; i < 512; i++)
	{
		DWORD dwLowerAddress = (i * PAGE_5STEPSIZE);
		// 최상위 메모리공간인 0x1ff000 공간은 read only 속성을 부여한다.
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
 *	페이지 엔트리에 기준 주소와 속성 플래그를 설정
 */
void kSetPageEntryData( PTENTRY* pstEntry, DWORD dwUpperBaseAddress,
		DWORD dwLowerBaseAddress, DWORD dwLowerFlags, DWORD dwUpperFlags )
{
	pstEntry->dwAttributeAndLowerBaseAddress = dwLowerBaseAddress | dwLowerFlags;
	pstEntry->dwUpperBaseAddressAndEXB = ( dwUpperBaseAddress & 0xFF ) | 
		dwUpperFlags;
}
