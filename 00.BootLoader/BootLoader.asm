[ORG 0x00]          ; 코드의 시작 어드레스를 0x00으로 설정
[BITS 16]           ; 이하의 코드는 16비트 코드로 설정

SECTION .text       ; text 섹션(세그먼트) 정의

jmp 0x07C0:START    ; CS는 코드 세그먼트의 시작주소를 가리키는 중요한 레지스터지만
                    ; BIOS가 사용하던 데이터가 들어있으므로 초기화가 필요하다
                    ; BootLoader가 메모리에 복사되는 위치인 0x7c00으로 초기화 한다

TOTALSECTORCOUNT	dw	0x02
                    ; 부트 로더를 제외한 MINT64 OS 이미지의 크기
                    ; 최대 1152 섹터까지 가능

; 레지스터 초기화
START:
	; 세그먼트 레지스터 설정
	mov ax, 0x07C0  ; 부트 로더의 시작 어드레스(0x7C00)를
	mov ds, ax      ; DS(데이터 영역 레지스터)에 설정
	mov ax, 0xB800	; 비디오 메모리의 시작 어드레스(0xB8000)를
	mov es, ax		; ES(여분 레지스터)에 설정

	;스택을 0x0000:0000~0x0000:FFFF 영역에 64KB(0x10000) 크기로 생성
	mov ax, 0x0000	; 스택 세그먼트의 시작 어드레스(0x0000)를
	mov ss, ax		; SS 세그먼트 레지스터(스택 기준주소 저장)에 설정
	mov sp, 0xFFFE	; SP 레지스터의 어드레스를 0xFFFE로 설정
	mov bp, 0xFFFE	; BP 레지스터의 어드레스를 0xFFFE로 설정

; 화면 지움
	mov si, 80 * 25 * 2	; SI 레지스터를 마지막 위치로 초기화
.SCREENCLEARLOOP:
	mov byte[es:si-2], 0	; 0으로 빈 글짜 출력
	mov byte[es:si-1], 0x0A	; 0x0A 으로 검은 바탕에 밝은 녹색 속성 설정
	sub si, 2        ; si-2 == 0 일때 zf 플래그가 1로 셋팅 됨
	                 ; 즉, 모든 위치를 지웠을 때, 플래그가 세팅됨
	jg .SCREENCLEARLOOP
                     ; jg는 zf 플래그가 0일때 작동

; 환영 메시지 출력
	mov si, MESSAGE1 ; PRINTMESSAGE()는 SI를 통해 인자를 받는다
	call PRINTMESSAGE

; 날짜 메시지 출력
	mov di, 160      ; 글짜 출력 위치를 두번째 줄로 바꾼다
                     ; di는 이 Bootloader에서 출력시에 글짜 좌표로 쓰인다 
	mov si, CURRENTDATEMESSAGE
	call PRINTMESSAGE

; 날짜 호출 및 출력
	; 날짜 호출 인터럽트
	mov ah, 04h      ; 날짜 인터럽트(1AH - 04H) 
	int 1Ah          ; 일=dl, 월=dh, 년=cl, 세기=ch를 받아온다

	; 일자 출력 및 unpack
	movzx bx, dl     ; PRINT_AND_SAVE_BCD()는 BX를 통해 인자를 받는다
	call PRINT_AND_SAVE_BCD
	                 ; AX = PRINT_AND_SAVE_BCD(DL)
	mov dl, al       ; DL에 BCD로 저장된 날짜를 unpack하여 저장한다

	; 구분자 출력
	mov byte[es:di], '/'
	add di, 2

	; 달 출력 및 unpack
	movzx bx, dh
	call PRINT_AND_SAVE_BCD
	mov dh, al       ; DH에 월 unpack

	; 구분자 출력
	mov byte[es:di], '/'
	add di, 2

 	;연도 출력 및 unpack
	movzx bx, ch
	call PRINT_AND_SAVE_BCD
	mov ch, al       ; CH에 세기 unpack

	movzx bx, cl
	call PRINT_AND_SAVE_BCD
	shr cx, 8        ; 오른쪽 시프트로, CH만 남긴다
	imul cx, 100     ; CH는 원래 100의 자리므로 100을 곱한다
	add cx, ax       ; 세기인 CH에 년도인 CX를 더해서 4자리 년도 완성

; 요일 출력
; 1900년 1월 1일부터의 날짜를 세어서 7로 나누어 상대적 요일을 구할 것이다
; 현재 DL=일자, DL=달, CX=년도가 저장되어 있다.

	; 일 계산
	dec dl           ; 1900년 1월 1일부터의 날짜이므로 현재날짜 - 1일
	movzx ax, dl     ; ax는 이제 1900년1월1일부터의 지난날짜
	
	; 월 계산
	; 1월 부터의 날짜를 세야한다 
	; 원래 목표는 올해 초부터 지난 달까지의 일수를 더해 7로 나눠야 한다
	; 이는 이전 달까지의 일수 합을 7로 나눈 나머지를 더한 것과 동일하다
	movzx bx, dh     ; 배열에 접근하기 위해 BX에 DH(달)을 집어넣는다
	cmp bx, 3        ; DH(달)가 3월 이후 인지 검사한다
	                 ; 1월의 경우 지난달이 없고, 
	                 ; 2월의 경우 1월의 일수만 더해주면 되므로
	                 ; 올해에 대한 윤년계산이 필요없다
	jna .AFTER_CONSIDER_LEAP_YEAR
	inc cx           ; 나머지 달의 경우, 올해도 윤년계산이 필요하다
	dec ax           ; 올해까지 윤년계산을 하면서 1년이 추가되므로 미리 제거한다
	.AFTER_CONSIDER_LEAP_YEAR:
	movzx bx, byte[DAYS_UNTIL_MONTH + bx -1]
	                 ; DAYS_~는 지난달까지의 일수 합을 7로 나눈 나머지
	                 ; (bx-1)이 0일때 가리키는 1월은 0이 더해지므로 
	                 ; 케이스분류를 피할 수 있다
	add bx, ax       ; 년도 계산은 윤년 계산이 들어가서 나눗셈을 해야한다
	                 ; 이 줄 부터는 bx로 일수의 합을 관리해야 한다
	
	; 연도 계산
	; 1900년 부터의 날짜를 세야한다
	; 1900년 1월 1일 부터 올해 1월 1일 까지는 
	; 1900년 부터 작년까지의 연 * 365와
	; 1900년 부터 작년까지 윤년의 횟수를 더하는 방식으로 구할 수 있다
	; 365는 7로 나눈 나머지가 1이므로 년도별로 1씩만 더해도 된다
	add bx, cx       ; 작년까지의 일수만 필요하지만, CX를 더했다
	sub bx, 1901     ; CX는 작년+1 이므로 1900+1을 뺀다

	; 윤년 계산
	; 1900년 부터 작년까자의 윤년의 횟수는 작년까지의 윤년의 개수에서 
	; 1900년까지의 윤년의 개수를 빼서 구할 수 있다

	; 작년까지의 윤년의 횟수를 구한다
	mov si, cx       ; div에서 cx로 제수를 전달해야 하기 때문에 si로 년도를 관리한다
	mov cx, 4        ; 4로 나눠지면 윤년이다
	call DIVIDE_SI_BY_CX
	add bx, ax
	
	mov cx, 100      ; 100으로 나눠지면 윤년이 아니지만, 4로 나뉘면서 더해졌다
	call DIVIDE_SI_BY_CX
	sub bx, ax
 
	mov cx, 400      ; 400으로 나눠지면 윤년이다
	call DIVIDE_SI_BY_CX
	add bx, ax

	; 1900년까지의 윤년은
	; 1900 / 4 + 1900 / 400 - 1900/100이고 이는
	; 475 + 4 - 19로 460 번 발생했다
	sub bx, 460      ; 1900년까지의 윤년을 뺀다

	; 7로 나누기
	mov si, bx       ; DIVIDE~ 함수는 SI를 CX로 나눈다
	mov cx, 7        ; AX에 몫, DX에 나머지가 저장된다
	call DIVIDE_SI_BY_CX
	shl dx, 2        ; 4배 곱해서 요일 주소를 찾는다
	mov si, DAYS     ; Mon0Tue0... 의 형태를 하고 있다
	add si, dx       ; 시작주소에 인덱스주소를 더해 요일 주소를 구한다
	add di, 2        ; 명세에 맞게 한칸 띄어쓴다
	call PRINTMESSAGE

	; os 로딩 메시지 출력
	mov di, 320      ; 글짜 출력 위치를 세번째 줄로 바꾼다
	mov si, IMAGELOADINGMESSAGE
	call PRINTMESSAGE

; 디스크 초기화
RESETDISK:
	xor ax, ax	     ; 디스크를 초기화하는 인터럽트(13H - 0H)
	xor dl, dl       ; 플로피 디스크에 관한 작업임을 알린다 0
	int 0x13
	jc HANDLEDISKERROR
	                ; 에러가 발생하면 에러 처리로 이동한다

	; 디스크의 내용을 메모리로 복사할 어드레스(ES:BX)를 0x10000으로 설정한다
	mov si, 0x1000  ; OS 이미지를 복사할 어드레스(0x10000)를
	                ; 세그먼트 레지스터 값으로 변환
	mov es, si      ; ES 세그먼트 레지스터에 값 설정
	mov bx, 0x0000  ; BX 레지스터에 0x0000을 설정하여 복사할
	                ; 어드레스를 0x1000:0000(0x10000)으로 최종 설정


	mov di, word[TOTALSECTORCOUNT]	; 복사할 OS 이미지의 섹터 수를 DI 레지스터에 설정

; 디스크 읽기
READDATA:					;디스크를 읽는 코드의 시작
	; 모든 섹터를 다 읽었는지 확인
	dec di		; 복사할 섹터 수를 1 감소
	jnge READEND	; di < 0이라면 다 복사 했으므로 READEND로 이동


	;;;구현 아이디어 : 트랙불러오기를 다중 For문으로 짜면 라인을 좀 아낄 수 있을 듯

	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	BIOS Read Function 호출
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov ax, 0x201				; 서비스2번 / 읽을 섹터 1개
	mov ch, byte[TRACKNUMBER]	; 읽을 트랙 번호 설정
	mov cl, byte[SECTORNUMBER]	; 읽을 섹터 번호 설정
	mov dh, byte[HEADNUMBER]	; 읽을 헤드 번호 설정
	xor dl, dl					; 읽을 드라이브 번호(0=Floppy) 설정
	int 0x13					; 인터럽트 서비스 수행
	; 0x13 - 02h
	; 입력 AX, CX, DX
	; 출력 CF:에러, AH:리턴코드, AL:읽은 데이터 수
;	jc HANDLEDISKERROR			; 에러가 발생했다면 HANDLEDISKERROR로 이동

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	복사할 어드레스와 트랙, 헤드, 섹터 어드레스 계산
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	add si, 0x0020	; 512(0x200)바이트만큼 읽었으므로, 이를 세그먼트 레지스터 값으로 변환
	mov es, si		; ES 세그먼트 레지스터에 더해서 어드레스를 한 섹터 만큼 증가

	; 한 섹터를 읽었으므로 섹터 번호를 증가시키고 마지막 섹터까지 읽었는지 판단
	; 마지막 섹터가 아니면 섹터 읽기로 이동해서 다시 섹터 읽기 수행
	mov al, byte[SECTORNUMBER]		; 섹터 번호를 AL 레지스터에 설정
	inc al					; 섹터 번호를 1 증가
	mov byte[SECTORNUMBER], al		; 증가시킨 섹터 번호를 SECTORNUMBER에 다시 설정
	cmp al, 19						; 증가시킨 섹터 번호를 19와 비교
	jl READDATA						; 섹터 번호가 19 미만이라면 READDATA로 이동

	; 마지막 섹터까지 읽었으면(섹터 번호가 19이면) 헤드를 토글(0->1, 1->0)하고,
	; 섹터 번호를 1로 설정
	xor byte[HEADNUMBER], 0x01		; 헤드 번호를 0x01과 XOR 하여 토글(0->1, 1->0)
	mov byte[SECTORNUMBER], 0x01	; 섹터 번호를 다시 1로 설정

	; 만약 헤드가 1->0으로 바뀌었으면 양쪽 헤드를 모두 읽은 것이므로
	; 아래로 이동하여 트랙 번호를 1 증가
	cmp byte[HEADNUMBER], 0x00		; 헤드 번호를 0x00과 비교
	jne READDATA					; 헤드 번호가 0이 아니면 READDATA로 이동

	; 트랙을 1 증가시킨 후 다시 섹터 읽기로 이동
	add byte[TRACKNUMBER], 0x1	; 트랙 번호를 1 증가
	jmp READDATA					; READDATA로 이동

; 디스크 읽기 완료
READEND:
	mov ax, 0xb800  ; 디스크를 읽으며 ES를 사용했으므로, 
	mov es, ax
	mov di, 358      ; 디스크 로딩 메시지 뒤로 비디오 메모리 인덱스를 옮긴다
	mov si, LOADINGCOMPLETEMESSAGE
	call PRINTMESSAGE
	jmp 0x1000:0x0000; 0x10000으로 이동해서 커널코드 실행한다

HANDLEDISKERROR:
; 이 함수는 디스크 에러가 발생했을 때 에러 메시지를 출력한다
	mov si, DISKERRORMESSAGE
	call PRINTMESSAGE
	jmp $            ; 현재 위치에서 무한 루프 수행한다

PRINTMESSAGE:
; 이 함수는 SI가 가리키는 문자열을 출력한다
; 이 함수는 다음의 동작조건이 있고, CL을 사용한다
; SI : 문자열의 주소
; ES : 비디오 메모리
; DI : 비디오 메모리상의 인덱스
; 이 함수는 리턴하는 데이터가 없다
.MESSAGELOOP:
	mov cl, byte[si] ; 메시지 한글자를 가져온다
	cmp cl, 0        ; 0과 비교한다
	je .MESSAGEEND   ; 0이면 종료한다
	mov byte[es:di], cl
	                 ; 비디오 메모리에 저장한다
	inc si           ; 메시지 인덱스를 글자하나 증가시킨다
	add di, 2        ; 비디오 메모리 인덱스는 글자하나, 속성하나 증가시킨다
	jmp .MESSAGELOOP ;
.MESSAGEEND:
	ret

PRINT_AND_SAVE_BCD:
; 이 함수는 세가지 일을 한다
; BX에 있는 BCD압축된 문자열을 unpack하고
; 이를 [출력]하고
; AX에 unpack하여 [저장]한다
; 이 함수는 다음의 동작조건이 있고, AX를 사용한다
; BX : 8비트로 BCD되어있는 2자리 숫자
; ES : 비디오 메모리
; DI : 비디오 메모리상의 인덱스
	; BCD는 8비트를 4비트씩 나누어 1자리씩 저장한 방식이다
	shl bx, 4        ; 4번 시프트하여 BCD를 bh에 걸치도록한다
	movzx ax, bh     ; [저장] AX에 10의 자리를 넣는다
	imul ax, 10      ; [저장] 현재 AX는 10의 자리이므로 10배 곱한다
	add bh, '0'      ; [출력] 숫자를 ascii로 변경한다
	mov byte[es:di], bh
	shr bx, 4        ; bx를 오른쪽 쉬프트한다
	and bl, 0fh      ; 10의 자리를 제거한다
	add al, bl       ; [저장] ax에 1의 자리도 더한다
	add bl, '0'      ;
	mov byte[es:di+2], bl
	add di, 4        ; [출력] 라인을 아낀다
	ret

DIVIDE_SI_BY_CX:
; 이 함수는 나누기 연산의 중복라인을 없애기 위해 작성하였다
; 이 함수는 다음의 동작조건이 있고, DX를 사용한다
; SI : 피제수
; CX : 제수(나눌 수)
	xor dx, dx       ; 16비트 나누기를 위해 DX를 정리한다
	mov ax, si       ; DIV연산을 위해 AX에 년도를 옮긴다
	div cx
	ret
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	데이터 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 부트 로더 메시지
MESSAGE1                db 'MINT64 OS Boot Loader Start~!!', 0	; 출력할 메시지 정의
DISKERRORMESSAGE        db 'DISK ERROR', 0		; 마지막은 0으로 설정하여 .MESSAGELOOP에서
IMAGELOADINGMESSAGE	    db 'OS Image Loading...', 0 ; 문자열이 종료되었음을 알 수 있도록 함
LOADINGCOMPLETEMESSAGE	db 'Complete~!!', 0
CURRENTDATEMESSAGE 	    db 'Current Date: ',0
DAYS_UNTIL_MONTH        db 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5
DAYS                    db 'Mon', 0, 'Tue', 0, 'Wed', 0, 'Thu', 0, 'Fri', 0, 'Sat', 0, 'Sun', 0

; 디스크 읽기에 필요한 변수들
SECTORNUMBER            db 0x02	; OS 이미지가 시작하는 섹터 번호를 저장하는 영역
HEADNUMBER              db 0x00	; OS 이미지가 시작하는 헤드 번호를 저장하는 영역
TRACKNUMBER             db 0x00	; OS 이미지가 시작하는 트랙 번호를 저장하는 영역
times 510-($ - $$)      db 0x00

db 0x55
db 0xAA
  
; good

