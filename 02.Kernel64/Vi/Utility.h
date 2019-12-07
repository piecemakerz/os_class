#ifndef FAKE_U
#define FAKE_U
#include "FileSystem.h"
#define MIN( x, y )	( ( ( x ) < ( y ) ) ? ( x ) : ( y ) )
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
int kMemCpy( void* pvDestination, const void* pvSource, int iSize )
{
    int i;
    
    for( i = 0 ; i < iSize ; i++ )
    {
        ( ( char* ) pvDestination )[ i ] = ( ( char* ) pvSource )[ i ];
    }
    
    return iSize;
}
void kPrintf( const char* pcFormatString, ... ){
    kPrintf(pcFormatString);
}
void kClearScreen(){
    system("clear");
}
BYTE kGetCh( void )
{
    return getch();
}
#endif