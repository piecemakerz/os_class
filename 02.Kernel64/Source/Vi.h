#ifndef VI__
#define VI__

typedef struct Line Line;
struct Line{
    char characters[80];
    Line* prevLine;
    Line* nextLine;
    short curCharPos;
    BOOL isOverflow;
};

typedef struct _Text_{
    Line* visualTop;
    Line* realTop;
    int visualLine;
    int realLine;
    int characterCount;
    BOOL drawAgain;
}Text;

void kVi( const char* pcParameterBuffer );

typedef struct __kViCommand{
    // 커맨드 문자열
    char* pcCommand;
    // 커맨드를 수행하는 함수의 포인터
    CommandFunction pfFunction;
} ViCommand;

static void LoadFile(char* filename);
static void DrawDisplay();
static void InsertMode(char key);
static void CommandMode(char key);
static void NewLine();
static void NextLine();
static void PrevLine();
static void changeToInsertMode_I(const char* pcParameterBuffer);
static void W_SaveFile(const char* pcParameterBuffer);

#endif