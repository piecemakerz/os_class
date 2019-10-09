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
START:
; 메모리 맵 첫 순회
	mov ax, 0x0000
	mov es, ax
.do_e820:
	mov di, 0x8004			; [es:di]위치에 24바이트 정보를 읽어들인다.
	xor ebx, ebx			; 인터럽트가 사용하는 configuration 값을 0으로 초기화한다.
	xor ebp, ebp			; 사용 가능한 메모리를 가리키는 엔트리 갯수를 bp에 저장한다.
	mov edx, 0x0534D4150	; edx에 "SMAP"을 위치시킨다
	mov eax, 0xe820
	mov [es:di + 20], dword 1
	mov ecx, 24
	int 0x15

	jc short .failed
	mov edx, 0x0534D4150
	cmp eax, edx			; 성공했을 시 eax는 "SMAP"으로 초기화되었을 것이다
	jne short .failed		; 인터럽트 실행 중 오류
	test ebx, ebx			; ebx는 configuration 값으로, 0이라면 리스트의 끝에 도달했음을 의미
							; 첫 순회때 리스트의 끝이라면 이는 계산할 필요가 없다.
	je short .failed
	jmp short .jmpin

; 메모리 맵 재귀순회
.e820lp:
	mov eax, 0xe820
	mov [es:di + 20], dword 1
	mov ecx, 24				; 24바이트 요청
	int 0x15

	jc short .e820f			; carry 플래그가 활성화되었다는 것은 리스트의 끝에 이미 도달했다는 의미이다.
	mov edx, 0x0534D4150

; 24바이트를 온전히 읽었는지 확인
.jmpin:
	jcxz .skipent				; 길이 0의 엔트리를 모두 스킵한다. (CX=0일때 jump)
	cmp cl, 20					; 24바이트를 읽었는지 확인
	jbe short .notext			; 24바이트를 읽지 않은 경우
	test byte[es:di + 20], 1	; 24바이트를 읽었다면, 맨 뒤 4바이트가 지워졌는지 확인
	je short .skipent			; 지워지지 않았다면 해당 엔트리 스킵

; 메모리 영역 길이 확인 및 사용가능 메모리 공간인지 확인
.notext:
	mov ecx, dword[es:di + 8]	; 메모리 영역 길이의 lower uint32_t를 얻는다
	or ecx, dword[es:di + 12]	; 길이가 0인지를 체크하기 위해 upper uint32_t와 or연산을 한다
	jz short .skipent			; 만일 uint64_t의 길이가 0이라면 엔트리를 스킵한다.

	add ebp, dword[es:di + 8]

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

.e820end:					; 출력
	mov ax, 0xB800
	mov es, ax

	mov ax, 0x5241
	mov byte[es:480], ah	; R
	mov byte[es:482], al	; A
	mov ax, 0x4D53
	mov byte[es:484], ah	; M
	mov byte[es:488], al	; S
	mov ax, 0x697A
	mov byte[es:490], ah	; i	
	mov byte[es:492], al	; z
	mov ax, 0x653A
	mov byte[es:494], ah	; e
	mov byte[es:496], al 	; :

	xor edx, edx			; mb단위 환산
	mov eax, ebp
	mov ecx, 0x100000
	div ecx
							; 해당 라인에서 eax는 n자리 
	mov di,508				; 수정 시, MB출력 인덱스도 수정할 것
	mov ecx, 10
	.DIVISIONLOOP:
		xor edx, edx
		div ecx				; 몫 eax, 나머지 edx
		add dl, '0'
		mov byte[es:di], dl
		sub di, 2
		or eax, eax			; 몫이 0이 아닌지 검사한다
		jnz .DIVISIONLOOP	; 몫이 더이상 남지 않으면 루프를 종료한다
	
	mov byte[es:510], 'M'	; M
	mov byte[es:512], 'B'	; B

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
	mov esi, ( SWITCHSUCCESSMESSAGE - $$ + 0x10000 )	; 출력할 메시지의 어드레스를 스택에 삽입
	mov edi, 640
	.MESSAGELOOP:				; 메시지를 출력하는 루프
		mov cl, byte[esi]
		cmp cl, 0				; 복사된 문자와 0을 비교
		je .MESSAGEEND			; 복사한 문자의 값이 0이면 문자열이 종료되었음을 의미하므로
								; .MESSAGEEND로 이동하여 문자 출력 종료
		mov byte[edi+0xB8000], cl		; 0이 아니라면 비디오 메모리 어드레스
										; 0xB8000 + EDI에 문자를 출력
		inc esi				; ESI 레지스터에 1을 더하여 다음 문자열로 이동
		add edi, 2				; EDI 레지스터에 2를 더하여 비디오 메모리의 다음 문자 위치로 이동
								; 비디오 메모리는 (문자, 속성)의 쌍으로 구성되므로 문자만 출력하려면
								; 2를 더해야 함
		jmp .MESSAGELOOP		; 메시지 출력 루프로 이동하여 다음 문자를 출력
	.MESSAGEEND:

	; CS 세그먼트 셀렉터를 커널 코드 디스크립터(0x08)로 변경하면서
	; 0x10200 어드레스(C언어 커널이 있는 어드레스)로 이동
	jmp dword 0x18: 0x10200 ; C 언어 커널이 존재하는 0x10200 어드레스로 이동하여 C언어 커널 수행

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

times 512 - ($ - $$) db 0x00	; 512바이트를 맞추기 위해 남은 부분을 0으로 채움
