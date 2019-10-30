#include "Types.h"
#include "Keyboard.h"
#include "Descriptor.h"
#include "PIC.h"
#include "Console.h"
#include "ConsoleShell.h"

/**
 *  �Ʒ� �Լ��� C ��� Ŀ���� ���� �κ���
 */
void Main( void )
{
    int iCursorX, iCursorY;

    // �ܼ��� ���� �ʱ�ȭ�� ��, ���� �۾��� ����
    kInitializeConsole( 0, 10 );    
    kPrintf( "Switch To IA-32e Mode Success~!!\n" );
    kPrintf( "IA-32e C Language Kernel Start..............[Pass]\n" );
    kPrintStringViaRelocated( 0, 14, "This message is printed through the video memory relocated to 0xAB8000");
    kPrintf( "Initialize Console..........................[Pass]\n" );

    // ���� ��Ȳ�� ȭ�鿡 ���
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
    // Ű���带 Ȱ��ȭ
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
    // PIC ��Ʈ�ѷ� �ʱ�ȭ �� ��� ���ͷ�Ʈ Ȱ��ȭ
    kInitializePIC();
    kMaskPICInterrupt( 0 );
    kEnableInterrupt();
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass\n" );

    // ���� ����
    kStartConsoleShell();
}

/**
 *  ���ڿ��� X, Y ��ġ�� ���
 */
void kPrintString( int iX, int iY, const char* pcString )
{
    CHARACTER* pstScreen = ( CHARACTER* ) 0xB8000;
    int i;
    
    // X, Y ��ǥ�� �̿��ؼ� ���ڿ��� ����� ��巹���� ���
    pstScreen += ( iY * 80 ) + iX;

    // NULL�� ���� ������ ���ڿ� ���
    for( i = 0 ; pcString[ i ] != 0 ; i++ )
    {
        pstScreen[ i ].bCharactor = pcString[ i ];
    }
}

/**
 *  Address�� X, Y ��ġ�� ���
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
 *  bit�� X, Y ��ġ�� ���
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

// ���ڿ��� X, Y ��ġ�� ����ϵ�, ���ε� ���� �޸� 0xAB8000�� �������� ����Ѵ�.
void kPrintStringViaRelocated(int iX, int iY, const char* pcString )
{
	CHARACTER* pstScreen = ( CHARACTER* ) 0xAB8000;
	int i;

	// X, Y ��ǥ�� �̿��ؼ� ���ڿ��� ����� ��巹���� ���
	pstScreen += ( iY * 80 ) + iX;

	// NULL�� ���� ������ ���ڿ� ���
	for( i = 0 ; pcString[ i ] != 0 ; i++ )
	{
		pstScreen[ i ].bCharactor = pcString[ i ];
	}
}

// �ּ� 0x1fe000�� 0x1ff000�� ���� �а� �� �� �ִ��� Ȯ���Ѵ�.
// 1MB~2MB �޸� ������ ����¡�� ���� ���̺���� �����ϸ� ��� 0���� �ʱ�ȭ�Ǿ��ִ�.
void CheckMemoryReadWrite()
{
	DWORD* checkPos;
	DWORD readData;

	// 0x1fe000 Ȯ��
	checkPos = (DWORD*) 0x1fe000;
	readData = (DWORD) 0xffffffff;

	// 0x1fe000 �޸𸮿��� ������ �б�
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

	// 0x1fe000 �޸𸮿��� ������ ����
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

	// 0x1ff000 Ȯ��
	checkPos = 0x1ff000;
	readData = 0xffffffff;

	// 0x1ff000 �޸𸮿��� ������ �б�
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

	// 0x1ff000 �޸𸮿��� ������ ����
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
