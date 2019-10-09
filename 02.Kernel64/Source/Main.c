#include "Types.h"

// �Լ� ����
void kPrintString( int iX, int iY, const char* pcString );
void kPrintStringViaRelocated(int iX, int iY, const char* pcString );
void CheckMemoryReadWrite(void);
/**
 *  �Ʒ� �Լ��� C ��� Ŀ���� ���� �κ���
 */
void Main( void )
{
    kPrintString( 0, 12, "Switch To IA-32e Mode Success~!!" );
    kPrintString( 0, 13, "IA-32e C Language Kernel Start..............[Pass]" );
    kPrintStringViaRelocated( 0, 14, "This message is printed through the video memory relocated to 0xAB8000");
    CheckMemoryReadWrite();
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

void CheckMemoryReadWrite()
{
	DWORD* checkPos;
	DWORD readData;

	checkPos = (DWORD*) 0x1fe000;
	readData = (DWORD) 0xffffffff;

	kPrintString(0, 15, "Read from 0x1fe000 [    ]");
	readData = *checkPos;
	if(readData == 0xffffffff)
	{
		kPrintString(20, 15, "Fail");
	}
	else
	{
		kPrintString(20, 15, "Pass");
	}

	kPrintString(0, 16, "Write to 0x1fe000 [    ]");
	*checkPos = 0xffffffff;
	if(*checkPos != 0xffffffff)
	{
		kPrintString(19, 16, "Fail");
	}
	else
	{
		kPrintString(19, 16, "Pass");
	}
	*checkPos = 0x00000000;

	checkPos = 0x1ff000;
	readData = 0xffffffff;

	kPrintString(0, 17, "Read from 0x1ff000 [    ]");
	readData = *checkPos;
	if(readData == 0xffffffff)
	{
		kPrintString(20, 17, "Fail");
	}
	else
	{
		kPrintString(20, 17, "Pass");
	}

	kPrintString(0, 18, "Write to 0x1ff000 [    ]");
	*checkPos = 0xffffffff;
	if(*checkPos != 0xffffffff)
	{
		kPrintString(19, 18, "Fail");
	}
	else
	{
		kPrintString(19, 18, "Pass");
	}
	*checkPos = 0x00000000;
}
