#include "Utility.h"
#include "AssemblyUtility.h"
#include "RTC.h"
#include "PIT.h"
#include <stdarg.h>

//PIT 컨트롤러가 발생한 횟수를 저장할 카운터
volatile QWORD g_qwTickCount=0;
static unsigned long next = 1;
static int daysOfMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/**
 *  메모리를 특정 값으로 채움
 */
void kMemSet( void* pvDestination, BYTE bData, int iSize )
{
    int i;
    QWORD qwData;
    int iRemainByteStartOffset;

    // 8바이트 데이터를 채움
    qwData = 0;
    for( i = 0 ; i < 8 ; i++ )
    {
        qwData = ( qwData << 8 ) | bData;
    }
    
    // 8바이트씩 먼저 채움
    for( i = 0 ; i < ( iSize / 8 ) ; i++ )
    {
        ( ( QWORD* ) pvDestination )[ i ] = qwData;
    }
    
    // 8바이트씩 채우고 남은 부분을 마무리
    iRemainByteStartOffset = i * 8;
    for( i = 0 ; i < ( iSize % 8 ) ; i++ )
    {
        ( (char* ) pvDestination )[ iRemainByteStartOffset++ ] = bData;
    }
}

/**
 *  메모리 복사
 */
int kMemCpy( void* pvDestination, const void* pvSource, int iSize )
{
    int i;
    int iRemainByteStartOffset;

    // 8바이트씩 먼저 복사
    for( i = 0 ; i < ( iSize / 8 ) ; i++ )
    {
        ( ( QWORD* ) pvDestination )[ i ] = ( ( QWORD* ) pvSource )[ i ];
    }

    // 8바이트씩 채우고 남은 부분을 마무리
    iRemainByteStartOffset = i * 8;
    for( i = 0 ; i < ( iSize % 8 ) ; i++ )
    {
        ( ( char* ) pvDestination )[ iRemainByteStartOffset ] =
            ( ( char* ) pvSource )[ iRemainByteStartOffset ];
        iRemainByteStartOffset++;
    }
}

/**
 *  메모리 비교
 */
int kMemCmp( const void* pvDestination, const void* pvSource, int iSize )
{
    int i, j;
    int iRemainByteStartOffset;
    QWORD qwValue;
    char cValue;
    
    // 8바이트씩 먼저 비교
    for( i = 0 ; i < ( iSize / 8 ) ; i++ )
    {
        qwValue = ( ( QWORD* ) pvDestination )[ i ] - ( ( QWORD* ) pvSource )[ i ];

        if( qwValue != 0 )
        {
            // 틀린 위치를 정확하게 찾아서 그 값을 반환
            for( i = 0; i < 8 ; i++ )
            {
                if( ( ( qwValue >> ( i * 8 ) ) & 0xFF ) != 0 )
                {
                    return ( qwValue >> ( i * 8 ) ) & 0xFF;
                }
            }
        }
    }

    // 8바이트씩 채우고 남은 부분을 마무리
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
 *  RFLAGS 레지스터의 인터럽트 플래그를 변경하고 이전 인터럽트 플래그의 상태를 반환
 */
BOOL kSetInterruptFlag( BOOL bEnableInterrupt )
{
    QWORD qwRFLAGS;

    // 이전의 RFLAGS 레지스터 값을 읽은 뒤에 인터럽트 가능/불가 처리
    qwRFLAGS = kReadRFLAGS();
    if( bEnableInterrupt == TRUE )
    {
        kEnableInterrupt();
    }
    else
    {
        kDisableInterrupt();
    }

    // 이전 RFLAGS 레지스터의 IF 비트(비트 9)를 확인하여 이전의 인터럽트 상태를 반환
    if( qwRFLAGS & 0x0200 )
    {
        return TRUE;
    }
    return FALSE;
}

/**
 *  문자열의 길이를 반환
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

// 램의 총 크기(Mbyte 단위)
static QWORD gs_qwTotalRAMMBSize = 0;

/**
 *  64Mbyte 이상의 위치부터 램 크기를 체크
 *      최초 부팅 과정에서 한번만 호출해야 함
 */
void kCheckTotalRAMSize( void )
{
    DWORD* pdwCurrentAddress;
    DWORD dwPreviousValue;
    
    // 64Mbyte(0x4000000)부터 4Mbyte단위로 검사 시작
    pdwCurrentAddress = ( DWORD* ) 0x4000000;
    while( 1 )
    {
        // 이전에 메모리에 있던 값을 저장
        dwPreviousValue = *pdwCurrentAddress;
        // 0x12345678을 써서 읽었을 때 문제가 없는 곳까지를 유효한 메모리 
        // 영역으로 인정
        *pdwCurrentAddress = 0x12345678;
        if( *pdwCurrentAddress != 0x12345678 )
        {
            break;
        }
        // 이전 메모리 값으로 복원
        *pdwCurrentAddress = dwPreviousValue;
        // 다음 4Mbyte 위치로 이동
        pdwCurrentAddress += ( 0x400000 / 4 );
    }
    // 체크가 성공한 어드레스를 1Mbyte로 나누어 Mbyte 단위로 계산
    gs_qwTotalRAMMBSize = ( QWORD ) pdwCurrentAddress / 0x100000;
}

/**
 *  RAM 크기를 반환
 */
QWORD kGetTotalRAMSize( void )
{
    return gs_qwTotalRAMMBSize;
}

/**
 *  atoi() 함수의 내부 구현
 */
long kAToI( const char* pcBuffer, int iRadix )
{
    long lReturn;
    
    switch( iRadix )
    {
        // 16진수
    case 16:
        lReturn = kHexStringToQword( pcBuffer );
        break;
        
        // 10진수 또는 기타
    case 10:
    default:
        lReturn = kDecimalStringToLong( pcBuffer );
        break;
    }
    return lReturn;
}

/**
 *  16진수 문자열을 QWORD로 변환 
 */
QWORD kHexStringToQword( const char* pcBuffer )
{
    QWORD qwValue = 0;
    int i;
    
    // 문자열을 돌면서 차례로 변환
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
 *  10진수 문자열을 long으로 변환
 */
long kDecimalStringToLong( const char* pcBuffer )
{
    long lValue = 0;
    int i;
    
    // 음수이면 -를 제외하고 나머지를 먼저 long으로 변환
    if( pcBuffer[ 0 ] == '-' )
    {
        i = 1;
    }
    else
    {
        i = 0;
    }
    
    // 문자열을 돌면서 차례로 변환
    for( ; pcBuffer[ i ] != '\0' ; i++ )
    {
        lValue *= 10;
        lValue += pcBuffer[ i ] - '0';
    }
    
    // 음수이면 - 추가
    if( pcBuffer[ 0 ] == '-' )
    {
        lValue = -lValue;
    }
    return lValue;
}

/**
 *  itoa() 함수의 내부 구현
 */
int kIToA( long lValue, char* pcBuffer, int iRadix )
{
    int iReturn;
    
    switch( iRadix )
    {
        // 16진수
    case 16:
        iReturn = kHexToString( lValue, pcBuffer );
        break;
        
        // 10진수 또는 기타
    case 10:
    default:
        iReturn = kDecimalToString( lValue, pcBuffer );
        break;
    }
    
    return iReturn;
}

/**
 *  16진수 값을 문자열로 변환
 */
int kHexToString( QWORD qwValue, char* pcBuffer )
{
    QWORD i;
    QWORD qwCurrentValue;

    // 0이 들어오면 바로 처리
    if( qwValue == 0 )
    {
        pcBuffer[ 0 ] = '0';
        pcBuffer[ 1 ] = '\0';
        return 1;
    }
    
    // 버퍼에 1의 자리부터 16, 256, ...의 자리 순서로 숫자 삽입
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
    
    // 버퍼에 들어있는 문자열을 뒤집어서 ... 256, 16, 1의 자리 순서로 변경
    kReverseString( pcBuffer );
    return i;
}

/**
 *  10진수 값을 문자열로 변환
 */
int kDecimalToString( long lValue, char* pcBuffer )
{
    long i;

    // 0이 들어오면 바로 처리
    if( lValue == 0 )
    {
        pcBuffer[ 0 ] = '0';
        pcBuffer[ 1 ] = '\0';
        return 1;
    }
    
    // 만약 음수이면 출력 버퍼에 '-'를 추가하고 양수로 변환
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

    // 버퍼에 1의 자리부터 10, 100, 1000 ...의 자리 순서로 숫자 삽입
    for( ; lValue > 0 ; i++ )
    {
        pcBuffer[ i ] = '0' + lValue % 10;        
        lValue = lValue / 10;
    }
    pcBuffer[ i ] = '\0';
    
    // 버퍼에 들어있는 문자열을 뒤집어서 ... 1000, 100, 10, 1의 자리 순서로 변경
    if( pcBuffer[ 0 ] == '-' )
    {
        // 음수인 경우는 부호를 제외하고 문자열을 뒤집음
        kReverseString( &( pcBuffer[ 1 ] ) );
    }
    else
    {
        kReverseString( pcBuffer );
    }
    
    return i;
}

/**
 *  문자열의 순서를 뒤집음
 */
void kReverseString( char* pcBuffer )
{
   int iLength;
   int i;
   char cTemp;
   
   
   // 문자열의 가운데를 중심으로 좌/우를 바꿔서 순서를 뒤집음
   iLength = kStrLen( pcBuffer );
   for( i = 0 ; i < iLength / 2 ; i++ )
   {
       cTemp = pcBuffer[ i ];
       pcBuffer[ i ] = pcBuffer[ iLength - 1 - i ];
       pcBuffer[ iLength - 1 - i ] = cTemp;
   }
}

/**
 *  sprintf() 함수의 내부 구현
 */
int kSPrintf( char* pcBuffer, const char* pcFormatString, ... )
{
    va_list ap;
    int iReturn;
    
    // 가변 인자를 꺼내서 vsprintf() 함수에 넘겨줌
    va_start( ap, pcFormatString );
    iReturn = kVSPrintf( pcBuffer, pcFormatString, ap );
    va_end( ap );
    
    return iReturn;
}

/**
 *  vsprintf() 함수의 내부 구현
 *      버퍼에 포맷 문자열에 따라 데이터를 복사
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
    
    // 포맷 문자열의 길이를 읽어서 문자열의 길이만큼 데이터를 출력 버퍼에 출력
    iFormatLength = kStrLen( pcFormatString );
    for( i = 0 ; i < iFormatLength ; i++ ) 
    {
        // %로 시작하면 데이터 타입 문자로 처리
        if( pcFormatString[ i ] == '%' ) 
        {
            // % 다음의 문자로 이동
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
                // 문자열 출력  
            case 's':
                // 가변 인자에 들어있는 파라미터를 문자열 타입으로 변환
                pcCopyString = ( char* ) ( va_arg(ap, char* ));
                iCopyLength = kStrLen( pcCopyString );
                // 문자열의 길이만큼을 출력 버퍼로 복사하고 출력한 길이만큼 
                // 버퍼의 인덱스를 이동
                kMemCpy( pcBuffer + iBufferIndex, pcCopyString, iCopyLength );
                iBufferIndex += iCopyLength;
                break;
                
                // 문자 출력
            case 'c':
                // 가변 인자에 들어있는 파라미터를 문자 타입으로 변환하여 
                // 출력 버퍼에 복사하고 버퍼의 인덱스를 1만큼 이동
                pcBuffer[ iBufferIndex ] = ( char ) ( va_arg( ap, int ) );
                iBufferIndex++;
                break;

                // 정수 출력
            case 'd':
            case 'i':
                // 가변 인자에 들어있는 파라미터를 정수 타입으로 변환하여
                // 출력 버퍼에 복사하고 출력한 길이만큼 버퍼의 인덱스를 이동
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
                
                // 4바이트 Hex 출력
            case 'x':
            case 'X':
                // 가변 인자에 들어있는 파라미터를 DWORD 타입으로 변환하여
                // 출력 버퍼에 복사하고 출력한 길이만큼 버퍼의 인덱스를 이동
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

                // 8바이트 Hex 출력
            case 'q':
            case 'Q':
            case 'p':
                // 가변 인자에 들어있는 파라미터를 QWORD 타입으로 변환하여
                // 출력 버퍼에 복사하고 출력한 길이만큼 버퍼의 인덱스를 이동
                qwValue = ( QWORD ) ( va_arg( ap, QWORD ) );
                iBufferIndex += kIToA( qwValue, pcBuffer + iBufferIndex, 16 );
                break;
            
                // 위에 해당하지 않으면 문자를 그대로 출력하고 버퍼의 인덱스를
                // 1만큼 이동
            default:
                pcBuffer[ iBufferIndex ] = pcFormatString[ i ];
                iBufferIndex++;
                break;
            }
        } 
        // 일반 문자열 처리
        else
        {
            // 문자를 그대로 출력하고 버퍼의 인덱스를 1만큼 이동
            pcBuffer[ iBufferIndex ] = pcFormatString[ i ];
            iBufferIndex++;
        }
        useField = FALSE;
        printX = FALSE;
        alignToLeft = FALSE;
    }
    
    // NULL을 추가하여 완전한 문자열로 만들고 출력한 문자의 길이를 반환
    pcBuffer[ iBufferIndex ] = '\0';
    return iBufferIndex;
}

/**
 *  Tick Count를 반환
 */
QWORD kGetTickCount( void )
{
    return g_qwTickCount;
}

// 선형 합동 생성기 코드
// RTC를 기반으로 한 time()이 초 단위로 시간을 측정하기 때문에,
// 밀리초 단위의 스케줄링에는 적합하지 않았다.
// 따라서 카운터 레지스터의 값을 기반으로 rand()를 구현하였다.
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
// 오전 0시부터 현재까지 흐른 초 수를 리턴한다.
// 현재 년도가 윤년인 경우는 고려하지 않는다.
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
 *  밀리세컨드(milisecond) 동안 대기
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
