#include "Types.h"
#include "Keyboard.h"
#include "Descriptor.h"
#include "PIC.h"

// 함수 선언
void kPrintString( int iX, int iY, const char* pcString );

/**
 *  아래 함수는 C 언어 커널의 시작 부분임
 */
void Main( void )
{
	// for keyboard
    char vcTemp[ 2 ] = { 0, };
    BYTE bTemp;
    int i = 0;
    KEYDATA stData;

    kPrintString( 0, 12, "Switch To IA-32e Mode Success~!!" );
    kPrintString( 0, 13, "IA-32e C Language Kernel Start..............[Pass]" );
    kPrintStringViaRelocated( 0, 14, "This message is printed through the video memory relocated to 0xAB8000");

    kPrintString( 0, 15, "GDT Initialize And Switch For IA-32e Mode...[    ]" );
    kInitializeGDTTableAndTSS();
    kLoadGDTR( GDTR_STARTADDRESS );
    kPrintString( 45, 15, "Pass" );
    
    kPrintString( 0, 16, "TSS Segment Load............................[    ]" );
    kLoadTR( GDT_TSSSEGMENT );
    kPrintString( 45, 16, "Pass" );
    
    kPrintString( 0, 17, "IDT Initialize..............................[    ]" );
    kInitializeIDTTables();    
    kLoadIDTR( IDTR_STARTADDRESS );
    kPrintString( 45, 17, "Pass" );

	CheckMemoryReadWrite();

    kPrintString( 0, 22, "Keyboard Activate And Queue Initialize......[    ]" );
    // 키보드를 활성화
    if( kInitializeKeyboard() == TRUE )
    {
        kPrintString( 45, 22, "Pass" );
        kChangeKeyboardLED( FALSE, FALSE, FALSE );
    }
    else
    {
        kPrintString( 45, 22, "Fail" );
        while( 1 ) ;
    }
    
    kPrintString( 0, 23, "PIC Controller And Interrupt Initialize.....[    ]" );
    // PIC 컨트롤러 초기화 및 모든 인터럽트 활성화
    kInitializePIC();
    kMaskPICInterrupt( 0 );
    kEnableInterrupt();
    kPrintString( 45, 23, "Pass" );
    
    while( 1 )
    {
        // 키 큐에 데이터가 있으면 키를 처리함
        if( kGetKeyFromKeyQueue( &stData ) == TRUE )
        {
            // 키가 눌러졌으면 키의 ASCII 코드 값을 화면에 출력
            if( stData.bFlags & KEY_FLAGS_DOWN )
            {
                // 키 데이터의 ACII 코드 값을 저장
                vcTemp[ 0 ] = stData.bASCIICode;
                kPrintString( i++, 24, vcTemp );

                // 0이 입력되면 변수를 0으로 나누어 Divide Error 예외(벡터 0번)을
                // 발생시킴
                if( vcTemp[ 0 ] == '0' )
                {
                    // 아래 코드를 수행하면 Divide Error 예외가 발생하여
                    // 커널의 임시 핸들러가 수행됨
                    bTemp = bTemp / 0;
                }
            }
        }
    }
}

/**
 *  문자열을 X, Y 위치에 출력
 */
void kPrintString( int iX, int iY, const char* pcString )
{
    CHARACTER* pstScreen = ( CHARACTER* ) 0xB8000;
    int i;
    
    // X, Y 좌표를 이용해서 문자열을 출력할 어드레스를 계산
    pstScreen += ( iY * 80 ) + iX;

    // NULL이 나올 때까지 문자열 출력
    for( i = 0 ; pcString[ i ] != 0 ; i++ )
    {
        pstScreen[ i ].bCharactor = pcString[ i ];
    }
}

/**
 *  Address을 X, Y 위치에 출력
 */
void kPrintAddress( int iX, int iY, int address )
{
    CHARACTER* pstScreen = ( CHARACTER* ) 0xB8000;
    int adr = address;
    char string[8] = { 0, };
    int i;
    int j;
    int size;
    int mod;

    pstScreen += ( iY * 80 ) + iX;

    for(i = 0; i < 8; i++){
        mod = adr % 16;
        if(mod < 10){
            string[i] = '0' + mod;
        }
        else{
            string[i] = 'a' + (mod-10);
        }
        adr = adr / 16;
        if(adr == 0){
            break;
        }
    }

    pstScreen[0].bCharactor = '0';
    pstScreen[1].bCharactor = 'x';
    size = i + 1;
    for(j = 0; j < size; j++){
        pstScreen[j+2].bCharactor = string[i];
        i--;
    }
}

// 문자열을 X, Y 위치에 출력하되, 맵핑된 비디오 메모리 0xAB8000을 기준으로 출력한다.
void kPrintStringViaRelocated(int iX, int iY, const char* pcString )
{
	CHARACTER* pstScreen = ( CHARACTER* ) 0xAB8000;
	int i;

	// X, Y 좌표를 이용해서 문자열을 출력할 어드레스를 계산
	pstScreen += ( iY * 80 ) + iX;

	// NULL이 나올 때까지 문자열 출력
	for( i = 0 ; pcString[ i ] != 0 ; i++ )
	{
		pstScreen[ i ].bCharactor = pcString[ i ];
	}
}

// 주소 0x1fe000와 0x1ff000에 값을 읽고 쓸 수 있는지 확인한다.
// 1MB~2MB 메모리 공간은 페이징을 위한 테이블들을 제외하면 모두 0으로 초기화되어있다.
void CheckMemoryReadWrite()
{
	DWORD* checkPos;
	DWORD readData;

	// 0x1fe000 확인
	checkPos = (DWORD*) 0x1fe000;
	readData = (DWORD) 0xffffffff;

	// 0x1fe000 메모리에서 데이터 읽기
	kPrintString(0, 18, "Read from 0x1fe000 [    ]");
	readData = *checkPos;
	if(readData == 0xffffffff)
	{
		kPrintString(20, 18, "Fail");
	}
	else
	{
		kPrintString(20, 18, "Pass");
	}

	// 0x1fe000 메모리에서 데이터 쓰기
	kPrintString(0, 19, "Write to 0x1fe000 [    ]");
	*checkPos = 0xffffffff;
	if(*checkPos != 0xffffffff)
	{
		kPrintString(19, 19, "Fail");
	}
	else
	{
		kPrintString(19, 19, "Pass");
	}
	*checkPos = 0x00000000;

	// 0x1ff000 확인
	checkPos = 0x1ff000;
	readData = 0xffffffff;

	// 0x1ff000 메모리에서 데이터 읽기
	kPrintString(0, 20, "Read from 0x1ff000 [    ]");
	readData = *checkPos;
	if(readData == 0xffffffff)
	{
		kPrintString(20, 20, "Fail");
	}
	else
	{
		kPrintString(20, 20, "Pass");
	}

	// 0x1ff000 메모리에서 데이터 쓰기
	kPrintString(0, 21, "Write to 0x1ff000 [    ]");
	*checkPos = 0xffffffff;
	if(*checkPos != 0xffffffff)
	{
		kPrintString(19, 21, "Fail");
	}
	else
	{
		kPrintString(19, 21, "Pass");
	}
	*checkPos = 0x00000000;
}
