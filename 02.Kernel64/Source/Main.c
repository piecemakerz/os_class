#include "Types.h"
#include "Keyboard.h"
#include "Descriptor.h"
#include "PIC.h"
#include "Console.h"
#include "ConsoleShell.h"

/**
 *  아래 함수는 C 언어 커널의 시작 부분임
 */
void Main( void )
{
    int iCursorX, iCursorY;

    // 콘솔을 먼저 초기화한 후, 다음 작업을 수행
    kInitializeConsole( 0, 10 );    
    kPrintf( "Switch To IA-32e Mode Success~!!\n" );
    kPrintf( "IA-32e C Language Kernel Start..............[Pass]\n" );
    kPrintStringViaRelocated( 0, 14, "This message is printed through the video memory relocated to 0xAB8000");
    kPrintf( "Initialize Console..........................[Pass]\n" );

    // 부팅 상황을 화면에 출력
    kGetCursor( &iCursorX, &iCursorY );
    kPrintf( "GDT Initialize And Switch For IA-32e Mode...[    ]" );
    kInitializeGDTTableAndTSS();
    kLoadGDTR( GDTR_STARTADDRESS );
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass\n" );
    
    kPrintf( "TSS Segment Load............................[    ]" );
    kLoadTR( GDT_TSSSEGMENT );
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass\n" );
    
    kPrintf( "IDT Initialize..............................[    ]" );
    kInitializeIDTTables();    
    kLoadIDTR( IDTR_STARTADDRESS );
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass\n" );

	//CheckMemoryReadWrite();

    kPrintf( "Total RAM Size Check........................[    ]" );
    kCheckTotalRAMSize();
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass], Size = %d MB\n", kGetTotalRAMSize() );

    kPrintf( "Keyboard Activate And Queue Initialize......[    ]" );
    // 키보드를 활성화
    if( kInitializeKeyboard() == TRUE )
    {
        kSetCursor( 45, iCursorY++ );
        kPrintf( "Pass\n" );
        kChangeKeyboardLED( FALSE, FALSE, FALSE );
    }
    else
    {
        kSetCursor( 45, iCursorY++ );
        kPrintf( "Fail\n" );
        while( 1 ) ;
    }
    
    kPrintf( "PIC Controller And Interrupt Initialize.....[    ]" );
    // PIC 컨트롤러 초기화 및 모든 인터럽트 활성화
    kInitializePIC();
    kMaskPICInterrupt( 0 );
    kEnableInterrupt();
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass\n" );

    // 셸을 시작
    kStartConsoleShell();
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
void kPrintAddress( int iX, int iY, int iAddress )
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

/**
 *  bit을 X, Y 위치에 출력
 */
void kPrint32Bit( int iX, int iY, DWORD* dwBits )
{
    CHARACTER* pstScreen = ( CHARACTER* ) 0xB8000;
    char string[32] = { 0, };
    DWORD mask;
    int i;
    int j;

    pstScreen += ( iY * 80 ) + iX;

    for(i = 0; i < 32; i++){
        mask = 1 << i;
        if((*bits)&mask){
            string[i] = '1';
        }
        else{
            string[i] = '0';
        }
    }
    for(j = 0; j < 32; j++){
        pstScreen[j].bCharactor = string[i-1];
        i--;
    }
}
void kPrint64Bit( int iX, int iY, QWORD* qwBits )
{
    CHARACTER* pstScreen = ( CHARACTER* ) 0xB8000;
    char string[64] = { 0, };
    QWORD mask;
    int i;
    int j;

    pstScreen += ( iY * 80 ) + iX;

    for(i = 0; i < 64; i++){
        mask = 1 << i;
        if((*bits)&mask){
            string[i] = '1';
        }
        else{
            string[i] = '0';
        }
    }
    for(j = 0; j < 64; j++){
        pstScreen[j].bCharactor = string[i-1];
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
