#include "Utility.h"
#include "AssemblyUtility.h"
#include "RTC.h"
#include "PIT.h"
#include <stdarg.h>

//PIT ��Ʈ�ѷ��� �߻��� Ƚ���� ������ ī����
volatile QWORD g_qwTickCount=0;
static unsigned long next = 1;
static int daysOfMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/**
 *  �޸𸮸� Ư�� ������ ä��
 */
void kMemSet( void* pvDestination, BYTE bData, int iSize )
{
    int i;
    QWORD qwData;
    int iRemainByteStartOffset;

    // 8����Ʈ �����͸� ä��
    qwData = 0;
    for( i = 0 ; i < 8 ; i++ )
    {
        qwData = ( qwData << 8 ) | bData;
    }
    
    // 8����Ʈ�� ���� ä��
    for( i = 0 ; i < ( iSize / 8 ) ; i++ )
    {
        ( ( QWORD* ) pvDestination )[ i ] = qwData;
    }
    
    // 8����Ʈ�� ä��� ���� �κ��� ������
    iRemainByteStartOffset = i * 8;
    for( i = 0 ; i < ( iSize % 8 ) ; i++ )
    {
        ( (char* ) pvDestination )[ iRemainByteStartOffset++ ] = bData;
    }
}

/**
 *  �޸� ����
 */
int kMemCpy( void* pvDestination, const void* pvSource, int iSize )
{
    int i;
    int iRemainByteStartOffset;

    // 8����Ʈ�� ���� ����
    for( i = 0 ; i < ( iSize / 8 ) ; i++ )
    {
        ( ( QWORD* ) pvDestination )[ i ] = ( ( QWORD* ) pvSource )[ i ];
    }

    // 8����Ʈ�� ä��� ���� �κ��� ������
    iRemainByteStartOffset = i * 8;
    for( i = 0 ; i < ( iSize % 8 ) ; i++ )
    {
        ( ( char* ) pvDestination )[ iRemainByteStartOffset ] =
            ( ( char* ) pvSource )[ iRemainByteStartOffset ];
        iRemainByteStartOffset++;
    }
}

/**
 *  �޸� ��
 */
int kMemCmp( const void* pvDestination, const void* pvSource, int iSize )
{
    int i, j;
    int iRemainByteStartOffset;
    QWORD qwValue;
    char cValue;
    
    // 8����Ʈ�� ���� ��
    for( i = 0 ; i < ( iSize / 8 ) ; i++ )
    {
        qwValue = ( ( QWORD* ) pvDestination )[ i ] - ( ( QWORD* ) pvSource )[ i ];

        if( qwValue != 0 )
        {
            // Ʋ�� ��ġ�� ��Ȯ�ϰ� ã�Ƽ� �� ���� ��ȯ
            for( i = 0; i < 8 ; i++ )
            {
                if( ( ( qwValue >> ( i * 8 ) ) & 0xFF ) != 0 )
                {
                    return ( qwValue >> ( i * 8 ) ) & 0xFF;
                }
            }
        }
    }

    // 8����Ʈ�� ä��� ���� �κ��� ������
    iRemainByteStartOffset = i * 8;
    for( i = 0 ; i < ( iSize % 8 ) ; i++ )
    {
        cValue = ( ( char* ) pvDestination )[ iRemainByteStartOffset ] -
            ( ( char* ) pvSource )[ iRemainByteStartOffset ];
        if( cValue != 0 )
        {
            return cValue;
        }
        iRemainByteStartOffset++;
    }
    return 0;
}

/**
 *  RFLAGS ���������� ���ͷ�Ʈ �÷��׸� �����ϰ� ���� ���ͷ�Ʈ �÷����� ���¸� ��ȯ
 */
BOOL kSetInterruptFlag( BOOL bEnableInterrupt )
{
    QWORD qwRFLAGS;

    // ������ RFLAGS �������� ���� ���� �ڿ� ���ͷ�Ʈ ����/�Ұ� ó��
    qwRFLAGS = kReadRFLAGS();
    if( bEnableInterrupt == TRUE )
    {
        kEnableInterrupt();
    }
    else
    {
        kDisableInterrupt();
    }

    // ���� RFLAGS ���������� IF ��Ʈ(��Ʈ 9)�� Ȯ���Ͽ� ������ ���ͷ�Ʈ ���¸� ��ȯ
    if( qwRFLAGS & 0x0200 )
    {
        return TRUE;
    }
    return FALSE;
}

/**
 *  ���ڿ��� ���̸� ��ȯ
 */
int kStrLen( const char* pcBuffer )
{
    int i;
    
    for( i = 0 ; ; i++ )
    {
        if( pcBuffer[ i ] == '\0' )
        {
            break;
        }
    }
    return i;
}

// ���� �� ũ��(Mbyte ����)
static QWORD gs_qwTotalRAMMBSize = 0;

/**
 *  64Mbyte �̻��� ��ġ���� �� ũ�⸦ üũ
 *      ���� ���� �������� �ѹ��� ȣ���ؾ� ��
 */
void kCheckTotalRAMSize( void )
{
    DWORD* pdwCurrentAddress;
    DWORD dwPreviousValue;
    
    // 64Mbyte(0x4000000)���� 4Mbyte������ �˻� ����
    pdwCurrentAddress = ( DWORD* ) 0x4000000;
    while( 1 )
    {
        // ������ �޸𸮿� �ִ� ���� ����
        dwPreviousValue = *pdwCurrentAddress;
        // 0x12345678�� �Ἥ �о��� �� ������ ���� �������� ��ȿ�� �޸� 
        // �������� ����
        *pdwCurrentAddress = 0x12345678;
        if( *pdwCurrentAddress != 0x12345678 )
        {
            break;
        }
        // ���� �޸� ������ ����
        *pdwCurrentAddress = dwPreviousValue;
        // ���� 4Mbyte ��ġ�� �̵�
        pdwCurrentAddress += ( 0x400000 / 4 );
    }
    // üũ�� ������ ��巹���� 1Mbyte�� ������ Mbyte ������ ���
    gs_qwTotalRAMMBSize = ( QWORD ) pdwCurrentAddress / 0x100000;
}

/**
 *  RAM ũ�⸦ ��ȯ
 */
QWORD kGetTotalRAMSize( void )
{
    return gs_qwTotalRAMMBSize;
}

/**
 *  atoi() �Լ��� ���� ����
 */
long kAToI( const char* pcBuffer, int iRadix )
{
    long lReturn;
    
    switch( iRadix )
    {
        // 16����
    case 16:
        lReturn = kHexStringToQword( pcBuffer );
        break;
        
        // 10���� �Ǵ� ��Ÿ
    case 10:
    default:
        lReturn = kDecimalStringToLong( pcBuffer );
        break;
    }
    return lReturn;
}

/**
 *  16���� ���ڿ��� QWORD�� ��ȯ 
 */
QWORD kHexStringToQword( const char* pcBuffer )
{
    QWORD qwValue = 0;
    int i;
    
    // ���ڿ��� ���鼭 ���ʷ� ��ȯ
    for( i = 0 ; pcBuffer[ i ] != '\0' ; i++ )
    {
        qwValue *= 16;
        if( ( 'A' <= pcBuffer[ i ] )  && ( pcBuffer[ i ] <= 'Z' ) )
        {
            qwValue += ( pcBuffer[ i ] - 'A' ) + 10;
        }
        else if( ( 'a' <= pcBuffer[ i ] )  && ( pcBuffer[ i ] <= 'z' ) )
        {
            qwValue += ( pcBuffer[ i ] - 'a' ) + 10;
        }
        else 
        {
            qwValue += pcBuffer[ i ] - '0';
        }
    }
    return qwValue;
}

/**
 *  10���� ���ڿ��� long���� ��ȯ
 */
long kDecimalStringToLong( const char* pcBuffer )
{
    long lValue = 0;
    int i;
    
    // �����̸� -�� �����ϰ� �������� ���� long���� ��ȯ
    if( pcBuffer[ 0 ] == '-' )
    {
        i = 1;
    }
    else
    {
        i = 0;
    }
    
    // ���ڿ��� ���鼭 ���ʷ� ��ȯ
    for( ; pcBuffer[ i ] != '\0' ; i++ )
    {
        lValue *= 10;
        lValue += pcBuffer[ i ] - '0';
    }
    
    // �����̸� - �߰�
    if( pcBuffer[ 0 ] == '-' )
    {
        lValue = -lValue;
    }
    return lValue;
}

/**
 *  itoa() �Լ��� ���� ����
 */
int kIToA( long lValue, char* pcBuffer, int iRadix )
{
    int iReturn;
    
    switch( iRadix )
    {
        // 16����
    case 16:
        iReturn = kHexToString( lValue, pcBuffer );
        break;
        
        // 10���� �Ǵ� ��Ÿ
    case 10:
    default:
        iReturn = kDecimalToString( lValue, pcBuffer );
        break;
    }
    
    return iReturn;
}

/**
 *  16���� ���� ���ڿ��� ��ȯ
 */
int kHexToString( QWORD qwValue, char* pcBuffer )
{
    QWORD i;
    QWORD qwCurrentValue;

    // 0�� ������ �ٷ� ó��
    if( qwValue == 0 )
    {
        pcBuffer[ 0 ] = '0';
        pcBuffer[ 1 ] = '\0';
        return 1;
    }
    
    // ���ۿ� 1�� �ڸ����� 16, 256, ...�� �ڸ� ������ ���� ����
    for( i = 0 ; qwValue > 0 ; i++ )
    {
        qwCurrentValue = qwValue % 16;
        if( qwCurrentValue >= 10 )
        {
            pcBuffer[ i ] = 'A' + ( qwCurrentValue - 10 );
        }
        else
        {
            pcBuffer[ i ] = '0' + qwCurrentValue;
        }
        
        qwValue = qwValue / 16;
    }
    pcBuffer[ i ] = '\0';
    
    // ���ۿ� ����ִ� ���ڿ��� ����� ... 256, 16, 1�� �ڸ� ������ ����
    kReverseString( pcBuffer );
    return i;
}

/**
 *  10���� ���� ���ڿ��� ��ȯ
 */
int kDecimalToString( long lValue, char* pcBuffer )
{
    long i;

    // 0�� ������ �ٷ� ó��
    if( lValue == 0 )
    {
        pcBuffer[ 0 ] = '0';
        pcBuffer[ 1 ] = '\0';
        return 1;
    }
    
    // ���� �����̸� ��� ���ۿ� '-'�� �߰��ϰ� ����� ��ȯ
    if( lValue < 0 )
    {
        i = 1;
        pcBuffer[ 0 ] = '-';
        lValue = -lValue;
    }
    else
    {
        i = 0;
    }

    // ���ۿ� 1�� �ڸ����� 10, 100, 1000 ...�� �ڸ� ������ ���� ����
    for( ; lValue > 0 ; i++ )
    {
        pcBuffer[ i ] = '0' + lValue % 10;        
        lValue = lValue / 10;
    }
    pcBuffer[ i ] = '\0';
    
    // ���ۿ� ����ִ� ���ڿ��� ����� ... 1000, 100, 10, 1�� �ڸ� ������ ����
    if( pcBuffer[ 0 ] == '-' )
    {
        // ������ ���� ��ȣ�� �����ϰ� ���ڿ��� ������
        kReverseString( &( pcBuffer[ 1 ] ) );
    }
    else
    {
        kReverseString( pcBuffer );
    }
    
    return i;
}

/**
 *  ���ڿ��� ������ ������
 */
void kReverseString( char* pcBuffer )
{
   int iLength;
   int i;
   char cTemp;
   
   
   // ���ڿ��� ����� �߽����� ��/�츦 �ٲ㼭 ������ ������
   iLength = kStrLen( pcBuffer );
   for( i = 0 ; i < iLength / 2 ; i++ )
   {
       cTemp = pcBuffer[ i ];
       pcBuffer[ i ] = pcBuffer[ iLength - 1 - i ];
       pcBuffer[ iLength - 1 - i ] = cTemp;
   }
}

/**
 *  sprintf() �Լ��� ���� ����
 */
int kSPrintf( char* pcBuffer, const char* pcFormatString, ... )
{
    va_list ap;
    int iReturn;
    
    // ���� ���ڸ� ������ vsprintf() �Լ��� �Ѱ���
    va_start( ap, pcFormatString );
    iReturn = kVSPrintf( pcBuffer, pcFormatString, ap );
    va_end( ap );
    
    return iReturn;
}

/**
 *  vsprintf() �Լ��� ���� ����
 *      ���ۿ� ���� ���ڿ��� ���� �����͸� ����
 */
int kVSPrintf( char* pcBuffer, const char* pcFormatString, va_list ap )
{
    QWORD i, j;
    int iBufferIndex = 0;
    int iFormatLength, iCopyLength;
    char* pcCopyString;
    QWORD qwValue;
    int iValue;

    BOOL useField = FALSE;
    BOOL printX = FALSE;
    BOOL alignToLeft = FALSE;
    char fieldBuffer[5];
    int fieldBufferIndex = 0;
    char numberBuffer[21];
    int fieldLength;
    int numberLength;
    int paddingLength;
    
    // ���� ���ڿ��� ���̸� �о ���ڿ��� ���̸�ŭ �����͸� ��� ���ۿ� ���
    iFormatLength = kStrLen( pcFormatString );
    for( i = 0 ; i < iFormatLength ; i++ ) 
    {
        // %�� �����ϸ� ������ Ÿ�� ���ڷ� ó��
        if( pcFormatString[ i ] == '%' ) 
        {
            // % ������ ���ڷ� �̵�
            i++;
            if (pcFormatString[ i ] == '#'){
                i++;
                printX = TRUE;
            }
            while( pcFormatString[ i ] >= '0' && pcFormatString[ i ] <= '9'){
                if(! useField ){
                    useField = TRUE;
                    fieldBufferIndex = 0;
                }
                fieldBuffer[fieldBufferIndex++] = pcFormatString[ i++ ];
            }
            switch( pcFormatString[ i ] ) 
            {
                // ���ڿ� ���  
            case 's':
                // ���� ���ڿ� ����ִ� �Ķ���͸� ���ڿ� Ÿ������ ��ȯ
                pcCopyString = ( char* ) ( va_arg(ap, char* ));
                iCopyLength = kStrLen( pcCopyString );
                // ���ڿ��� ���̸�ŭ�� ��� ���۷� �����ϰ� ����� ���̸�ŭ 
                // ������ �ε����� �̵�
                kMemCpy( pcBuffer + iBufferIndex, pcCopyString, iCopyLength );
                iBufferIndex += iCopyLength;
                break;
                
                // ���� ���
            case 'c':
                // ���� ���ڿ� ����ִ� �Ķ���͸� ���� Ÿ������ ��ȯ�Ͽ� 
                // ��� ���ۿ� �����ϰ� ������ �ε����� 1��ŭ �̵�
                pcBuffer[ iBufferIndex ] = ( char ) ( va_arg( ap, int ) );
                iBufferIndex++;
                break;

                // ���� ���
            case 'd':
            case 'i':
                // ���� ���ڿ� ����ִ� �Ķ���͸� ���� Ÿ������ ��ȯ�Ͽ�
                // ��� ���ۿ� �����ϰ� ����� ���̸�ŭ ������ �ε����� �̵�
                iValue = ( int ) ( va_arg( ap, int ) );

                if( useField ){
                    useField = FALSE;

                    fieldBuffer[fieldBufferIndex] = '\0';
                    fieldLength = kAToI(fieldBuffer,10);

                    numberLength = kIToA(iValue, numberBuffer, 10);
                    paddingLength = fieldLength > numberLength ? fieldLength - numberLength : 0;

                    for (int k=0; k<paddingLength; k++)
                        pcBuffer[iBufferIndex + k] = (fieldBuffer[0] == '0' && iValue > 0) ? '0' : ' ';
                    
                    for (int k=0; k<numberLength; k++)
                        pcBuffer[iBufferIndex + paddingLength + k] = numberBuffer[k];
                    iBufferIndex += paddingLength + numberLength;
                }else{
                    iBufferIndex += kIToA( iValue, pcBuffer + iBufferIndex, 10 );
                }
                break;
                
                // 4����Ʈ Hex ���
            case 'x':
            case 'X':
                // ���� ���ڿ� ����ִ� �Ķ���͸� DWORD Ÿ������ ��ȯ�Ͽ�
                // ��� ���ۿ� �����ϰ� ����� ���̸�ŭ ������ �ε����� �̵�
                qwValue = ( DWORD ) ( va_arg( ap, DWORD ) ) & 0xFFFFFFFF;
                if( useField ){
                    useField = FALSE;

                    fieldBuffer[fieldBufferIndex] = '\0';
                    fieldLength = kAToI(fieldBuffer, 10);

                    numberLength = kIToA(qwValue, numberBuffer, 16);
                    paddingLength = fieldLength > numberLength ? fieldLength - numberLength : 0;
                    // if(printX == TRUE){
                    //     numberLength+=2;
                    //     paddingLength-=2;
                    // }
                    for (int k=0; k<paddingLength; k++)
                        pcBuffer[iBufferIndex + k] = (fieldBuffer[0] == '0' && iValue>0) ? '0':' ';
                    if(printX){
                        paddingLength = paddingLength<0 ? 0:paddingLength;
                        pcBuffer[iBufferIndex + paddingLength++] = '0';
                        pcBuffer[iBufferIndex + paddingLength++] = 'x';
                    }
                    for (int k=0; k<numberLength; k++)
                        pcBuffer[iBufferIndex + paddingLength + k] = numberBuffer[k];
                    iBufferIndex += paddingLength + numberLength;
                }else{
                    paddingLength = 0;
                    if(printX){
                        pcBuffer[iBufferIndex + paddingLength++] = '0';
                        pcBuffer[iBufferIndex + paddingLength++] = 'x';
                    }
                    iBufferIndex += kIToA( qwValue, pcBuffer + paddingLength + iBufferIndex, 16 );
                }
                break;

                // 8����Ʈ Hex ���
            case 'q':
            case 'Q':
            case 'p':
                // ���� ���ڿ� ����ִ� �Ķ���͸� QWORD Ÿ������ ��ȯ�Ͽ�
                // ��� ���ۿ� �����ϰ� ����� ���̸�ŭ ������ �ε����� �̵�
                qwValue = ( QWORD ) ( va_arg( ap, QWORD ) );
                iBufferIndex += kIToA( qwValue, pcBuffer + iBufferIndex, 16 );
                break;
            
                // ���� �ش����� ������ ���ڸ� �״�� ����ϰ� ������ �ε�����
                // 1��ŭ �̵�
            default:
                pcBuffer[ iBufferIndex ] = pcFormatString[ i ];
                iBufferIndex++;
                break;
            }
        } 
        // �Ϲ� ���ڿ� ó��
        else
        {
            // ���ڸ� �״�� ����ϰ� ������ �ε����� 1��ŭ �̵�
            pcBuffer[ iBufferIndex ] = pcFormatString[ i ];
            iBufferIndex++;
        }
        useField = FALSE;
        printX = FALSE;
        alignToLeft = FALSE;
    }
    
    // NULL�� �߰��Ͽ� ������ ���ڿ��� ����� ����� ������ ���̸� ��ȯ
    pcBuffer[ iBufferIndex ] = '\0';
    return iBufferIndex;
}

/**
 *  Tick Count�� ��ȯ
 */
QWORD kGetTickCount( void )
{
    return g_qwTickCount;
}

// ���� �յ� ������ �ڵ�
// RTC�� ������� �� time()�� �� ������ �ð��� �����ϱ� ������,
// �и��� ������ �����ٸ����� �������� �ʾҴ�.
// ���� ī���� ���������� ���� ������� rand()�� �����Ͽ���.
DWORD rand(WORD max_rand)
{
	next = next * 1103515245 + 12345;
	return (DWORD)(next/65536) % max_rand;
}

void srand(WORD seed)
{
	next = seed;
}

WORD time(void)
{
	return kReadCounter0();
}
/*
// ���� 0�ú��� ������� �帥 �� ���� �����Ѵ�.
// ���� �⵵�� ������ ���� ������� �ʴ´�.
unsigned long time(void)
{
	WORD i;
	BYTE j;
	unsigned long totalSeconds;
	BYTE hour, minute, second;
	WORD year;
	BYTE month, dayOfMonth, dayOfWeek;
	unsigned long secondsOfDay = 86400;
	totalSeconds = 0;
	kReadRTCTime( &hour, &minute, &second );
	kReadRTCDate( &year, &month, &dayOfMonth, &dayOfWeek);
	for(i=1970; i<year; i++)
	{
		if((year%4 == 0) && (year%100 != 0) || (year%400 == 0))
		{
			totalSeconds += secondsOfDay * 366;
		}
		else
		{
			totalSeconds += secondsOfDay * 365;
		}
	}
	for(j=1; j<month; j++)
	{
		totalSeconds += secondsOfDay * daysOfMonth[j];
	}
	totalSeconds += secondsOfDay * (dayOfMonth - 1);
	totalSeconds += hour * 3600;
	totalSeconds += minute * 60;
	totalSeconds += second;
	return totalSeconds;
}
*/

/**
 *  �и�������(milisecond) ���� ���
 */
void kSleep( QWORD qwMillisecond )
{
    QWORD qwLastTickCount;

    qwLastTickCount = g_qwTickCount;

    while( ( g_qwTickCount - qwLastTickCount ) <= qwMillisecond )
    {
        kSchedule();
    }
}
