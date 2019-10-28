#include "Types.h"
#include "Keyboard.h"
#include "Descriptor.h"
#include "PIC.h"

// �Լ� ����
void kPrintString( int iX, int iY, const char* pcString );

/**
 *  �Ʒ� �Լ��� C ��� Ŀ���� ���� �κ���
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
    // Ű���带 Ȱ��ȭ
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
    // PIC ��Ʈ�ѷ� �ʱ�ȭ �� ��� ���ͷ�Ʈ Ȱ��ȭ
    kInitializePIC();
    kMaskPICInterrupt( 0 );
    kEnableInterrupt();
    kPrintString( 45, 23, "Pass" );
    
    while( 1 )
    {
        // Ű ť�� �����Ͱ� ������ Ű�� ó����
        if( kGetKeyFromKeyQueue( &stData ) == TRUE )
        {
            // Ű�� ���������� Ű�� ASCII �ڵ� ���� ȭ�鿡 ���
            if( stData.bFlags & KEY_FLAGS_DOWN )
            {
                // Ű �������� ACII �ڵ� ���� ����
                vcTemp[ 0 ] = stData.bASCIICode;
                kPrintString( i++, 24, vcTemp );

                // 0�� �ԷµǸ� ������ 0���� ������ Divide Error ����(���� 0��)��
                // �߻���Ŵ
                if( vcTemp[ 0 ] == '0' )
                {
                    // �Ʒ� �ڵ带 �����ϸ� Divide Error ���ܰ� �߻��Ͽ�
                    // Ŀ���� �ӽ� �ڵ鷯�� �����
                    bTemp = bTemp / 0;
                }
            }
        }
    }
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
