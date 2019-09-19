[ORG 0x00]	; 코드의 시작 어드레스를 0x00으로 설정
[BITS 16]	; 이하의 코드는 16비트 코드로 설정

SECTION .text	; text 섹션(세그먼트) 정의

jmp 0x07C0:START	; CS 세그먼트 레지스터에 0x07C0을 복사하면서, START 레이블로 이동
					; BIOS는 메모리 주소 0x07C00에 부트 로더를 복사한다.
					; 부트로더는 512바이트짜리 코드이므로 코드 영역을 0x07C00으로 설정한다

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	MINT64 OS에 관련된 환경 설정 값
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
TOTALSECTORCOUNT:	dw	0x02	; 부트 로더를 제외한 MINT64 OS 이미지의 크기
								; 최대 1152 섹터까지 가능

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

START:
	mov ax, 0x07C0	; 부트 로더의 시작 어드레스(0x7C00)를 세그먼트 레지스터 값으로 변환
	mov ds, ax		; DS 세그먼트 레지스터(데이터 영역 레지스터)에 설정
	mov ax, 0xB800	; 비디오 메모리(화면 출력)의 시작 어드레스(0xB8000)를 세그먼트 레지스터 값으로 변환
	mov es, ax		; ES 세그먼트 레지스터(문자열 관련 레지스터)에 설정

	;스택을 0x0000:0000~0x0000:FFFF 영역에 64KB(0x10000) 크기로 생성
	mov ax, 0x0000	; 스택 세그먼트의 시작 어드레스(0x0000)를 세그먼트 레지스터 값으로 변환
	mov ss, ax		; SS 세그먼트 레지스터(스택 기준주소 저장)에 설정
	mov sp, 0xFFFE	; SP 레지스터의 어드레스를 0xFFFE로 설정
	mov bp, 0xFFFE	; BP 레지스터의 어드레스를 0xFFFE로 설정

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	화면을 모두 지우고, 속성 값을 녹색으로 설정
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov si, 80 * 25 * 2	; SI 레지스터를 마지막 위치로 초기화
.SCREENCLEARLOOP:
	mov byte[es:si-2], 0	; 0으로 화면을 삭제한다
	mov byte[es:si-1], 0x0A	; 속성에 0x0A(검은 바탕에 밝은 녹색)을 복사
	sub si, 2	; sub연산과 더불어 si-2 == 0 일때 zf 플래그가 1로 셋팅 됨
				; 즉, 모든 위치를 지웠을 때, 플래그가 세팅됨
	jg .SCREENCLEARLOOP		; jz는 zf 플래그가 0일때 작동 (아직 스크린을 다 지우지 못한 것)
							; .SCREENCLEARLOOP 레이블로 이동
	; mov si, 0				; 어차피 문자열 시작주소를 집어넣으므로 초기화 할 필요가 없다
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	화면 상단에 시작 메시지 출력
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov si, MESSAGE1	; si에 출력할 문자열 시작주소를 넘긴다
	call PRINTMESSAGE	; PRINTMESSAGE 함수 호출
	mov di, 160			; di를 2째 줄로 넘긴다
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	화면 상단에 현재 날짜 출력
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	mov si, CURRENTDATEMESSAGE
	call PRINTMESSAGE 	; 날짜 안내 메시지를 출력한다

.GETREALTIME:
	;시간 호출 인터럽트
	mov ah, 04h		; 인자 04h: 날짜
	int 1Ah			; 인터럽트 번호

	;일자 출력
	movzx bx, dl 			; bx에 입력하면
	call PRINT_AND_SAVE_BCD	; BCD를 출력하고, ax에 저장
	mov dl, al	 			; ax에 저장된 날짜를 dl에 저장
	mov byte[es:di], '/'
	add di, 2

	;달 출력
	movzx bx, dh			; bx에 입력하면
	call PRINT_AND_SAVE_BCD	; ax에 달 저장됨
	mov dh, al	; dh에 달 저장
	mov byte[es:di], '/'
	add di, 2

	;연도 출력; (다음 줄 부터 HHLL년을 HH세기와 LL년으로 이해하면 됨)
	movzx bx, ch			; bx에 입력하면
	call PRINT_AND_SAVE_BCD ; ax에 세기 저장됨.
	mov ch, al	 ; ch에 세기 저장

	movzx bx, cl			; bx에 입력하면
	call PRINT_AND_SAVE_BCD ; ax에 년 저장됨.
	shr cx, 8	 ; 오른쪽 쉬프트로, 년도를 없애버림.
	imul cx, 100 ; 세기에 100을 곱함.
	add cx, ax	 ; cx에 년도를 더해서 HHLL 완성

	; 요일 출력
	; 1일부터의 날짜를 세야한다
	; dl에 일자, dh에 달, cx에 년도가 저장되어 있다.
	dec dl		 ; 1900년 1월 1일부터의 날짜이므로 현재날짜 - 1일
	movzx ax, dl ; ax는 이제 1900년1월1일부터의 지난날짜 (ex. 1900년1월2일이라면 ax=1)
	
	; 1월 부터의 날짜를 세야한다 
	; 올해 초부터 지난 달까지의 일수를 더해 7로 나눠야 한다
	; 이전 달까지의 일수 합을 7로 나눈 나머지를 더하자
	movzx bx, dh ; 배열에 접근하기 위해 bx에 dh(달)을 집어넣는다
	cmp bx, 3	 ; dh(달)가 3월 이후 인지 검사한다
	jl .AFTER_CONSIDER_LEAP_YEAR	; 3월 이전이라면 윤년계산이 필요없다
		; 1월의 경우 지난달이 없고, 
		; 2월의 경우 1월의 일수만 더해주면 되므로 문제가 없다
		add cx, 1    ; 나머지 달의 경우, 올해도 윤년계산이 필요하다
		dec ax		 ; 올해까지 윤년계산을 하면서 1년이 추가되므로 미리 제거한다
	.AFTER_CONSIDER_LEAP_YEAR:
	; 1월은 0이 더해지므로 케이스분류 할 필요없음
	movzx bx, byte[DAYS_UNTIL_MONTH + bx -1]
	add bx, ax ; 이 줄 부터는 bx로 일수의 합을 관리한다
	
	; 1900년 부터의 날짜를 세야한다
	; 1900년 1월 1일 부터 올해 1월 1일 까지는 
	; 1900년 부터 작년까지의 연 * 365와
	; 1900년 부터 작년까지 윤년의 횟수를 더하는 방식으로 구할 수 있다
	; 365는 7로 나눈 나머지가 1이므로 1씩만 더해도 된다
	add bx, cx
	sub bx, 1901
	; 먼저 윤년
	; 1900년 부터 작년까자의 윤년의 횟수는 작년까지의 윤년의 개수에서 
	; 1900년까지의 윤년의 개수를 빼서 구할 수 있다
	; 1900년까지의 윤년은
	; 1900 / 4 + 1900 / 400 - 1900/100이고 이는
	; 475 + 4 - 19로 460 번 발생했다
	mov si, cx ; div에서 cx로 제수를 전달해야 하기 때문에 si로 년도를 관리한다
	
	mov cx, 4   ; 4로 나눠지면 윤년이다
	call DIVIDE_YEAR_BY_CX
	add bx, ax
	
	mov cx, 100 ; 100으로 나눠지면 윤년이 아니지만, 4로 나뉘면서 더해졌다
	call DIVIDE_YEAR_BY_CX
	sub bx, ax
 
	mov cx, 400 ; 400으로 나눠지면 윤년이다
	call DIVIDE_YEAR_BY_CX
	add bx, ax

	sub bx, 460 ; 1900년까지의 윤년을 뺀다

	xor dx, dx
	mov ax, bx
	mov cx, 7
	div cx
	shl dx, 2
	mov si, DAYS
	add si, dx
	add di, 2
	call PRINTMESSAGE
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	OS 이미지를 로딩한다는 메시지 출력
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov di, 320
	mov si, IMAGELOADINGMESSAGE	; 출력할 메시지의 어드레스를 스택에 삽입
	call PRINTMESSAGE			; PRINTMESSAGE 함수 호출

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	디스크에서 OS 이미지를 로딩
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	디스크를 읽기 전에 먼저 리셋
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

RESETDISK:					;디스크를 리셋하는 코드의 시작
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	BIOS Reset Function 호출
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	; 인터럽트
	xor ax, ax	; 서비스 번호 0,
	xor dl, dl	; 드라이브 번호(0=Floppy)
	int 0x13
	; 에러가 발생하면 에러 처리로 이동
	jc HANDLEDISKERROR

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	디스크에서 섹터를 읽음
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; 디스크의 내용을 메모리로 복사할 어드레스(ES:BX)를 0x10000으로 설정
	mov si, 0x1000			; OS 이미지를 복사할 어드레스(0x10000)를
							; 세그먼트 레지스터 값으로 변환
	mov es, si				; ES 세그먼트 레지스터에 값 설정
	mov bx, 0x0000			; BX 레지스터에 0x0000을 설정하여 복사할
							; 어드레스를 0x1000:0000(0x10000)으로 최종 설정


	mov di, word[TOTALSECTORCOUNT]	; 복사할 OS 이미지의 섹터 수를 DI 레지스터에 설정

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
READEND:
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	OS이미지가 완료되었다는 메시지를 출력
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov ax, 0xb800
	mov es, ax
	mov di, 358
	mov si, LOADINGCOMPLETEMESSAGE		; 출력할 메시지의 어드레스를 스택에 삽입
	call PRINTMESSAGE				; PRINTMESSAGE 함수 호출
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;	로딩한 가상 OS 이미지 실행
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	jmp 0x1000:0x0000

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	함수 코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 디스크 에러를 처리하는 함수
HANDLEDISKERROR:
	mov si, DISKERRORMESSAGE	; 에러 문자열의 어드레스를 스택에 삽입
	call PRINTMESSAGE		; PRINTMESSAGE 함수 호출
	jmp $					; 현재 위치에서 무한 루프 수행
; 메시지를 출력하는 함수
; PARAM: x 좌표, y 좌표, 문자열
PRINTMESSAGE:
.MESSAGELOOP:				; 메시지를 출력하는 루프
	mov cl, byte[si]		; SI 레지스터가 가리키는 문자열의 위치에서 한 문자를
							; CL 레지스터에 복사
							; CL 레지스터는 CX 레지스터의 하위 1바이트를 의미
	cmp cl, 0				; 복사된 문자와 0을 비교
	je .MESSAGEEND			; 복사한 문자의 값이 0이면 문자열이 종료되었음을 의미하므로
							; .MESSAGEEND로 이동하여 문자 출력 종료

	mov byte[es:di], cl		; 0이 아니라면 비디오 메모리 어드레스 0xB800:di에 문자를 출력
	inc si, 				; SI 레지스터에 1을 더하여 다음 문자열로 이동
	add di, 2				; DI 레지스터에 2를 더하여 비디오 메모리의 다음 문자 위치로 이동
							; 비디오 메모리는 (문자, 속성)의 쌍으로 구성되므로 문자만 출력하려면
							; 2를 더해야 함

	jmp .MESSAGELOOP		; 메시지 출력 루프로 이동하여 다음 문자를 출력

.MESSAGEEND:
	ret					; 함수를 호출한 다음 코드의 위치로 복귀

PRINT_AND_SAVE_BCD:		; BCD를 bx에 입력받고, 화면에 [출력]하고 ax에 [저장]한다
	shl bx, 4			; bx에 입력받은 BCD를 상위, 하위비트로 나눈다
	movzx ax, bh		; [저장] ax에 10의 자리를 넣는다
	imul ax, 10			; [저장] ax는 10의 자리이므로 10배 한다
	add bh, '0'			; [출력] 숫자를 ascii로 변경한다
	mov byte[es:di], bh ; 
	shr bx, 4			; bx를 오른쪽 쉬프트한다
	and bl, 0fh			; 10의 자리를 제거한다
	add al, bl			; [저장] ax에 1의 자리도 더한다
	add bl, '0'			
	mov byte[es:di+2], bl
	add di, 4			; [출력] 한번에 더해서 라인을 아낀다
	ret					; 함수를 호출한 다음 코드의 위치로 복귀
DIVIDE_YEAR_BY_CX:
	xor dx, dx
	mov ax, si
	div cx
	ret
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	데이터 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 부트 로더 메시지
MESSAGE1				db 'MINT64 OS Boot Loader Start~!!', 0	; 출력할 메시지 정의
DISKERRORMESSAGE		db 'DISK ERROR', 0		; 마지막은 0으로 설정하여 .MESSAGELOOP에서
IMAGELOADINGMESSAGE	db 'OS Image Loading...', 0 ; 문자열이 종료되었음을 알 수 있도록 함
LOADINGCOMPLETEMESSAGE	db 'Complete~!!', 0
CURRENTDATEMESSAGE 	db 'Current Date: ',0
DAYS_UNTIL_MONTH  db 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5
DAYS db 'Mon', 0, 'Tue', 0, 'Wed', 0, 'Thu', 0, 'Fri', 0, 'Sat', 0, 'Sun', 0

; 디스크 읽기에 필요한 변수들
SECTORNUMBER		db 0x02	; OS 이미지가 시작하는 섹터 번호를 저장하는 영역
HEADNUMBER			db 0x00	; OS 이미지가 시작하는 헤드 번호를 저장하는 영역
TRACKNUMBER			db 0x00	; OS 이미지가 시작하는 트랙 번호를 저장하는 영역
times 510-($ - $$)	db 0x00

db 0x55
db 0xAA


