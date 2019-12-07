#include "DynamicMemory.h"
#include "Keyboard.h"
#include "Utility.h"
#include "FileSystem.h"
#include "ConsoleShell.h"
#include "Vi.h"

static FILE* fp;
static char vcFileName[ 50 ];
static Line* commandLine;
static Line* editLine;
static Text text;
static Line* currentLine;
static BOOL InsertModeFlag = FALSE;
static BOOL ESC_FLAG = FALSE;

void kVi( const char* pcParameterBuffer ){
    PARAMETERLIST stList;
    int iLength;
    int iEnterCount;
    commandLine = (Line*)kAllocateMemory(sizeof(Line));

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
    kInitializeParameter( &stList, pcParameterBuffer );
    iLength = kGetNextParameter( &stList, vcFileName );
    vcFileName[ iLength ] = '\0';

    // 파일 생성 실패
    if ( iLength > ( FILESYSTEM_MAXFILENAMELENGTH - 1 )){
        kPrintf("File name is too long\n");
        return;
    }

    // 파일 초기화
    if( iLength == 0 ) LoadFile(NULL);
    else LoadFile(vcFileName);
    
    // 변수 초기화
    text.drawAgain = TRUE;
    text.realLine = 1;
    // 메인 이벤트 루프
    kClearScreen();
    while(1){
        char key = kGetCh();

        if(key== KEY_ESC){
            InsertModeFlag = FALSE;
            ESC_FLAG = TRUE;
            continue;
        }
        if (InsertModeFlag)
            InsertMode(key);
        else
            CommandMode(key);
        DrawDisplay();
    }
}

static void LoadFile(char* filename){
    BYTE bKey;
    editLine = (Line*)kAllocateMemory(sizeof(Line));
    editLine->prevLine = editLine;
    editLine->nextLine = editLine;
    text.visualTop = editLine;
    // 새로운 파일
    if (filename == NULL)
        return;
    
    fp = fopen(filename, "r");
        // 이미 존재하는 파일이 아닌 경우, 파일을 생성
        if (fp==NULL){
            fp = fopen(vcFileName, "w");
        // 아닌 경우, 파일 불러옴
        } else{
            while(fread( &bKey, 1, 1, fp ) == 1){
                if (bKey == '\n' || editLine->curCharPos==80){
                    NewLine();
                }
                editLine->characters[editLine->curCharPos++]=bKey;
            }
        }
}

static void DrawDisplay(){
    if(text.drawAgain){
        kClearScreen();
        Line* tmpline = text.visualTop;
        for(int i=0; i<23; i++){
            kPrintf("%s",tmpline);
            tmpline = tmpline->nextLine;
        }
    }
    else{
        ;
    }
}
// editline을 새로운라인으로 옮김
static void NewLine(){
    editLine->characters[editLine->curCharPos+1] = '\0';
    if(editLine->nextLine == editLine){
        // 마지막 라인 추가
        Line* tmpLine = (Line*)kAllocateMemory(sizeof(Line));
        tmpLine->nextLine=tmpLine;
        tmpLine->prevLine = editLine;
        editLine->nextLine=tmpLine;
        editLine = tmpLine;
    }else{
        // 중간 라인 추가
        Line* tmpLine = (Line*)kAllocateMemory(sizeof(Line));
        tmpLine->nextLine = editLine->nextLine;
        tmpLine->prevLine = editLine;
        editLine->nextLine=tmpLine;
        editLine = tmpLine;
        text.drawAgain=TRUE;
    }
    text.realLine++;
    text.visualLine++;
    if(text.visualLine == 23){
        text.visualTop = text.visualTop->nextLine;
        text.drawAgain = TRUE;
        text.visualLine = 23;
    }
}
// overflow처리
static void OverFlow(){
    Line* tmpLine = (Line*)kAllocateMemory(sizeof(Line));
    tmpLine->nextLine = editLine->nextLine;
    tmpLine->prevLine = editLine->prevLine;
    tmpLine->isOverflow = TRUE;
    editLine->nextLine=tmpLine;
    text.visualLine++;  // 현재 수정중인 라인 수정 
    if(text.visualLine == 23){
        text.visualTop = text.visualTop->nextLine;
        text.drawAgain = TRUE;
        text.visualLine = 23;
    }
}
// editline을 다음 라인으로 옮김
static void NextLine(){
    if(editLine->nextLine != editLine){
        editLine->nextLine->curCharPos = MIN(kStrLen(editLine->nextLine->characters), editLine->curCharPos);// 새로운 커서 위치
        editLine = editLine->nextLine; // 다음 라인으로 이동
        text.realLine++; // text 정보 변경
        text.visualLine++;  // 현재 수정중인 라인 수정 
        if(text.visualLine == 23){
            text.visualTop = text.visualTop->nextLine;
            text.drawAgain = TRUE;
            text.visualLine = 23;
        }
        
    }
}
// editline을 윗라인으로 옮김
static void PrevLine(){
    if(editLine->prevLine != editLine){
        editLine->prevLine->curCharPos = MIN(kStrLen(editLine->prevLine->characters), editLine->curCharPos);
        editLine = editLine->prevLine;
        text.realLine--;
        text.visualLine--;
        if(text.visualLine == 0){
            text.visualTop = text.visualTop->prevLine;
            text.drawAgain = TRUE;
            text.visualLine = 1;
        }
    }
}
static void InsertMode(char key){
    InsertModeFlag = TRUE;
    if (key == KEY_BACKSPACE){
    }else{
        editLine->characters[editLine->curCharPos++] = key;
        if(editLine->curCharPos == 80)
            OverFlow();
    }
    
}
static ViCommand CommandList[] = {
    {":w", changeToInsertMode_I}
};
static void CommandMode(char key){
    if(ESC_FLAG){
        switch (key)
        {
        case 'i':
            InsertModeFlag = TRUE;
            ESC_FLAG = FALSE;
            break;
        
        default:
            break;
        }
    }
    else if( key == KEY_ENTER ){
        if( commandLine->characters > 0 ){
            // 커맨드 버퍼에 있는 명령을 실행
            commandLine->characters[ commandLine->curCharPos++ ] = '\0';
            kExecuteCommand( commandLine->characters );
        }
        kMemSet(commandLine->characters, NULL, commandLine->curCharPos);
        commandLine->curCharPos = 0;
    }
    
}
static void changeToInsertMode_I(const char* pcParameterBuffer){
    InsertModeFlag = TRUE;
    ESC_FLAG = FALSE;
}

static void W_SaveFile(const char* pcParameterBuffer){
    PARAMETERLIST stList;
    int iLength;
    int iEnterCount;
    commandLine = (Line*)kAllocateMemory(sizeof(Line));

    // 파라미터 리스트를 초기화하여 파일 이름을 추출
    kInitializeParameter( &stList, pcParameterBuffer );
    iLength = kGetNextParameter( &stList, vcFileName );
    vcFileName[ iLength ] = '\0';

    // 파일 생성 실패
    if ( iLength > ( FILESYSTEM_MAXFILENAMELENGTH - 1 )){
        kPrintf("File name is too long\n");
        return;
    }
    Line* top = editLine;
    while(top->prevLine != top)
        top = top->prevLine;
    fp = fopen( vcFileName, "w" );
    
    do{
        BYTE* buffer = top->characters;
        int dwRemainCount = kStrLen(top->characters);
        int dwWriteCount = MIN( dwRemainCount , FILESYSTEM_CLUSTERSIZE );
        if( fwrite( buffer, 1, dwWriteCount, fp ) != dwWriteCount );
        top = top->nextLine;
    }while(top);
}