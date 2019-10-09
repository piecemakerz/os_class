[ORG 0x00]
[BITS 16]

SECTION .text
; 과제 요약
; Entry.s
; 사용가능 메모리 크기 알아내기
; Main.c, Page.c
; 페이지 테이블로 가상메모리 활용

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 15h 인터럽트를 통해 시스템의 메모리 맵 정보를 알아낸다.
; 이후 사용 가능한 메모리 크기를 추출한다.
; 메모리 크기는 ebp 레지스터에 저장한다.
START:
	mov ax, 0x0000
	mov es, ax
; 메모리 맵 첫 순회
.do_e820:
	mov di, 0x0004			; 15h 인터럽트는 [es:di]위치에 ecx에 저장된 크기만큼의
							; 메모리 정보를 읽어들인다.
							; 따라서 [es:di] 위치에 20바이트 정보를 읽어들인다.
	xor ebx, ebx			; 인터럽트가 사용하는 configuration 값을 0으로 초기화한다.
	xor ebp, ebp			; 사용 가능한 메모리를 가리키는 엔트리 갯수를 bp에 저장한다.
	mov edx, 0x0534D4150	; edx에 "SMAP"을 위치시킨다
	mov eax, 0xe820			; 메모리 맵 기능을 사용하기 위해 eax=0xe820 값을 준 후 인터럽트를 호출한다.
	mov ecx, 20				; 20바이트 정보 읽어오기
	int 0x15				; 인터럽트 서비스 호출

	jc short .failed		; 실패했을 경우 예외처리
	mov edx, 0x0534D4150	; 인터럽트 호출 후 edx가 초기화될 수 있으므로 edx에 "SMAP"을 재위치시킨다.
	cmp eax, edx			; 성공했을 시 eax는 "SMAP"으로 초기화되었을 것이다
	jne short .failed		; 인터럽트 실행 중 오류 발생 예외처리
	test ebx, ebx			; ebx는 configuration 값으로, 0이라면 리스트의 끝에 도달했음을 의미
							; 첫 순회때 리스트의 끝이라면 이는 계산할 필요가 없다.
	je short .failed
	jmp short .jmpin

; 메모리 맵 재귀순회
.e820lp:
	mov eax, 0xe820
;	mov [es:di + 20], dword 1
	mov ecx, 20				; 24바이트 요청
	int 0x15

	jc short .e820f			; carry 플래그가 활성화되었다는 것은 리스트의 끝에 이미 도달했다는 의미이다.
	mov edx, 0x0534D4150

; 20바이트를 온전히 읽었는지 확인
.jmpin:
	jcxz .skipent				; 길이 0의 엔트리를 모두 스킵한다. (CX=0일때 jump)

; 메모리 영역이 사용 가능한 메모리 공간인지 확인하고 길이 확인
.notext:
	mov ecx, dword[es:di + 8]	; 메모리 영역 길이의 lower uint32_t를 얻는다
	or ecx, dword[es:di + 12]	; 길이가 0인지를 체크하기 위해 upper uint32_t와 or연산을 한다
	jz short .skipent			; 만일 uint64_t의 길이가 0이라면 엔트리를 스킵한다.

	add ebp, dword[es:di + 8]	; 메모리가 사용 가능한 공간이라면 ebp에 저장

; 이번 엔트리 스킵하기
.skipent:
	test ebx, ebx			; ebx가 0이라면 리스트의 끝이다.
	jne short .e820lp		; 아니라면 재귀 시작

; 재귀 종료 처리
.e820f:
	clc					; 캐리 플래그 clear
	jmp short .e820end	; 재귀 종료

; 인터럽트 호출 실패시
.failed:
	stc				; 캐리 플래그 1로 세트
	jmp short .e820end

; ebp에 저장된 사용 가능 메모리 크기 출력
.e820end:
	mov ax, 0xB800
	mov es, ax
	mov di, 480

	; 사용 가능 메모리 크기는 바이트 단위이므로, 이를 MB 단위로 치환한다.
	xor edx, edx
	mov eax, ebp
	mov ecx, 0x100000
	div ecx

	; 1000의 자릿수 계산
	xor edx, edx
	mov ecx, 1000
	div ecx

	add eax, '0'
	mov byte[es:di], al
	sub eax, '0'

	; 100의 자릿수 계산
	mov eax, edx
	xor edx, edx
	mov ecx, 100
	div ecx

	add eax, '0'
	mov byte[es:di+2], al
	sub eax, '0'

	; 10, 1의 자릿수 계산
	mov eax, edx
	xor edx, edx
	mov ecx, 10
	div ecx

	add eax, '0'
	mov byte[es:di+4], al

	add edx, '0'
	mov byte[es:di+6], dl

	mov byte[es:di+8], 'M'
	mov byte[es:di+10], 'B'

	jmp .count

.count:
	mov ax, 0x1000	; 보호 모드 엔트리 포인트의 시작 어드레스(0x10000)를
					; 세그먼트 레지스터 값으로 변환
	mov ds, ax		; DS 세그먼트 레지스터에 설정
	mov es, ax		; ES 세그먼트 레지스터에 설정

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; A20 게이트 활성화 -> p230
	; BIOS를 이용한 전환이 실패했을 때 시스템 컨트롤 포트로 전환 시도
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; BIOS 서비스를 사용해서 A20 게이트를 활성화
	mov ax, 0x2401		; A20 게이트 활성화 서비스 설정
	int 0x15			; BIOS 인터럽트 서비스 호출

	jc .A20GATEERROR	; A20 게이트 활성화가 성공했는지 확인
	jmp .A20GATESUCCESS

.A20GATEERROR:
	; 에러 발생 시, 시스템 컨트롤 포트로 전환 시도
	in al, 0x92			; 시스템 컨트롤 포트(0x92)에서 1바이트를 읽어 AL 레지스터에 저장
	or al, 0x02			; 읽은 값에 A20 게이트 비트(비트1)를 1로 설정
	and al, 0xFE		; 시스템 리셋 방지를 위해 0xFE와 AND 연산하여 비트 0를 0으로 설정
	out 0x92, al		; 시스템 컨트롤 포트(0x92)에 변경된 값을 1 바이트 설정
.A20GATESUCCESS:
	cli				; 인터럽트가 발생하지 못하도록 설정
	lgdt[GDTR]		; GDTR 자료구조를 프로세서에 설정하여 GDT 테이블을 로드

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; 보호 모드로 진입
	; Disable Paging, Disable Cache, Internal FPU, Disable Align Check,
	; Enable ProtectedMode
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov eax, 0x4000003B
	mov cr0, eax	; CR0 컨트롤 레지스터에 위에서 저장한 플래그를 설정하여
					; 보호 모드로 전환

	; 커널 코드 세그먼트를 0x00를 기준으로 하는 것으로 교체하고 EIP의 값을 0x00을 기준으로 재설정
	; CS 세그먼트 셀렉터 : EIP (다음에 실행할 명령어 주소를 저장하는 레지스터)
	jmp dword 0x18: (PROTECTEDMODE - $$ + 0x10000)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; 보호 모드로 진입

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[BITS 32]			; 이하의 코드는 32비트 코드로 설정
PROTECTEDMODE:
	mov ax, 0x20	; 보호 모드 커널용 데이터 세그먼트 디스크립터를 AX 레지스터에 저장
	mov ds, ax		; DS 세그먼트 셀렉터에 설정
	mov es, ax		; ES 세그먼트 셀렉터에 설정
	mov fs, ax		; FS 세그먼트 셀렉터에 설정
	mov gs, ax		; GS 세그먼트 셀렉터에 설정

	; 스택을 0x00000000~0x0000FFFF 영역에 64KB 크기로 생성
	mov ss, ax		; SS 세그먼트 셀렉터에 설정
	mov esp, 0xFFFE	; ESP 레지스터의 어드레스를 0xFFFE로 설정
	mov ebp, 0xFFFE	; EBP 레지스터의 어드레스를 0xFFFE로 설정
	
	; 화면에 보호 모드로 전환되었다는 메시지를 찍는다.
	push ( SWITCHSUCCESSMESSAGE - $$ + 0x10000 )	; 출력할 메시지의 어드레스를 스택에 삽입
	push 4											; 화면 Y 좌표(2)를 스택에 삽입
	push 0											; 화면 X 좌표(0)를 스택에 삽입
	call PRINTMESSAGE								; PRINTMESSAGE 함수 호출
	add esp, 12										; 삽입한 파라미터 제거

	; CS 세그먼트 셀렉터를 커널 코드 디스크립터(0x08)로 변경하면서
	; 0x10200 어드레스(C언어 커널이 있는 어드레스)로 이동
	jmp dword 0x18: 0x10200 ; C 언어 커널이 존재하는 0x10200 어드레스로 이동하여 C언어 커널 수행

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	함수 코드 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 메시지를 출력하는 함수
;	스택에 x좌표, y좌표, 문자열
PRINTMESSAGE:
	push ebp		; 베이스 포인터 레지스터(BP)를 스택에 삽입
	mov ebp, esp	; 베이스 포인터 레지스터(BP)에 스택 포인터 레지스터(SP)의 값을 설정
	push esi
	push edi
	push eax
	push ecx
	push edx

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; X, Y의 좌표로 비디오 메모리의 어드레스를 계산함
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; Y 좌표를 이용해서 먼저 라인 어드레스를 구함
	mov eax, dword[ebp+12]		; 파라미터 2(화면 좌표 Y)를 EAX 레지스터에 설정
	mov esi, 160				; 한 라인의 바이트 수(2 * 80컬럼)를 ESI 레지스터에 설정
	mul esi						; EAX 레지스터와 ESI 레지스터를 곱하여 화면 Y 어드레스 계산
	mov edi, eax				; 계산된 화면 Y 어드레스를 EDI 레지스터에 설정

	; X 좌표를 이용해서 2를 곱한 후 최종 어드레스를 구함
	mov eax, dword[ebp+8]		; 파라미터 1(화면 좌표 X)를 EAX 레지스터에 설정
	mov esi, 2					; 한 문자를 나타내는 바이트 수(2)를 ESI 레지스터에 설정
	mul esi						; EAX레지스터와 ESI 레지스터를 곱하여 화면 X 어드레스 계산
	add edi, eax				; 화면 Y의 어드레스와 계산된 X 어드레스를 더해서
								; 실제 비디오 메모리 어드레스를 계산

	; 출력할 문자열의 어드레스
	mov esi, dword[ebp+16]		; 파라미터 3(출력할 문자열의 어드레스)

	.MESSAGELOOP:				; 메시지를 출력하는 루프
		mov cl, byte[esi]		; ESI 레지스터가 가리키는 문자열의 위치에서 한 문자를
								; CL 레지스터에 복사
								; CL 레지스터는 ECX 레지스터의 하위 1바이트를 의미
								; 문자열을 1바이트면 충분하므로 ECX 레지스터의 하위 1바이트만 사용

		cmp cl, 0				; 복사된 문자와 0을 비교
		je .MESSAGEEND			; 복사한 문자의 값이 0이면 문자열이 종료되었음을 의미하므로
								; .MESSAGEEND로 이동하여 문자 출력 종료

		mov byte[edi+0xB8000], cl		; 0이 아니라면 비디오 메모리 어드레스
										; 0xB8000 + EDI에 문자를 출력

		add esi, 1				; ESI 레지스터에 1을 더하여 다음 문자열로 이동
		add edi, 2				; EDI 레지스터에 2를 더하여 비디오 메모리의 다음 문자 위치로 이동
								; 비디오 메모리는 (문자, 속성)의 쌍으로 구성되므로 문자만 출력하려면
								; 2를 더해야 함

		jmp .MESSAGELOOP		; 메시지 출력 루프로 이동하여 다음 문자를 출력

	.MESSAGEEND:
		pop edx					; 함수에서 사용이 끝난 EDX 레지스터부터 EBP 레지스터까지를
		pop ecx					; 스택에 삽입된 값을 이용해서 복원
		pop eax					; 스택은 가장 마지막에 들어간 데이터가 가장 먼저 나오는
		pop edi					; LIFO 자료구조이므로 삽입의 역순으로 제거(pop)해야 한다.
		pop esi
		pop ebp					; 베이스 포인터 레지스터(EBP) 복원
		ret						; 함수를 호출한 다음 코드의 위치로 복귀

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	데이터 영역
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[BITS 32]
; 아래의 데이터들을 8바이트에 맞춰 정렬하기 위해 추가
align 8, db 0

; GDTR의 끝을 8byte로 정렬하기 위해 추가
dw 0x0000
; GDTR 자료구조 정의
GDTR:
	dw GDTEND-GDT-1		; 아래에 위치하는 GDT 테이블의 전체 크기
	dd (GDT-$$+0x10000)	; 아래에 위치하는 GDT 테이블의 시작 어드레스

; GDT 테이블 정의
GDT:
	; 널(NULL) 디스크립터, 반드시 0으로 초기화해야 함
	NULLDescriptor:
		dw 0x0000
		dw 0x0000
		db 0x00
		db 0x00
		db 0x00
		db 0x00

	; IA-32e 모드 커널용 코드 세그먼트 디스크립터
	IA_32eCODEDESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x9A
		db 0xAF
		db 0x00

	; IA-32e 모드 커널용 데이터 세그먼트 디스크립터
	IA_32eDATADESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x92
		db 0xAF
		db 0x00

	; 보호 모드 커널용 코드 세그먼트 디스크립터
	CODEDESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x9A
		db 0xCF
		db 0x00

	; 보호 모드 커널용 데이터 세그먼트 디스크립터
	DATADESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x92
		db 0xCF
		db 0x00
GDTEND:

; 보호 모드로 전환되었다는 메시지
SWITCHSUCCESSMESSAGE: db 'Switch To Protected Mode Success~!!',0
ERROR: db 'error',0

times 512 - ($ - $$) db 0x00	; 512바이트를 맞추기 위해 남은 부분을 0으로 채움
