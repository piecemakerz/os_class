[BITS 32]

; C 언어에서 호출할 수 있도록 이름을 노출함(Export)
global kReadCPUID, kSwitchAndExecute64bitKernel

SECTION .text	; text 섹션(세그먼트)을 정의

; CPUID 반환
; 파라미터: DWORD dwEAX, DWORD* pdwEAX, * pdwEBX, * pdwECX, * pdwEDX
kReadCPUID:
	push ebp		; 베이스 포인터 레지스터(EBP)를 스택에 삽입
	mov ebp, esp	; 베이스 포인터 레지스터(EBP)에 스택 포인터 레지스터(ESP)의 값을 설정
	push eax		; 함수에서 임시로 사용하는 레지스터로 함수의 마지막 부분에서
	push ebx		; 스택에 삽입된 값을 거내 원래 값으로 복원
	push ecx
	push edx
	push esi

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; EAX 레지스터의 값으로 CPUID 명령어 실행
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov eax, dword[ebp+8]	; 파라미터 1(dwEAX)를 EAX 레지스터에 저장
	cpuid					; CPUID 명령어 실행

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; 반환된 값을 파라미터에 저장
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; *pdwEAX
	mov esi, dword[ebp+12]	; 파마리터 2(pdwEAX)를 ESI 레지스터에 저장
	mov dword[esi], eax		; pdwEAX가 포인터이므로 포인터가 가리키는 어드레스에
							; EAX 레지스터의 값을 저장

	; *pdwEBX
	mov esi, dword[ebp+16]
	mov dword[esi], ebx

	; *pdwECX
	mov esi, dword[ebp+20]
	mov dword[esi], ecx

	; *pdwEDX
	mov esi, dword[ebp+24]
	mov dword[esi], edx

	pop esi					; 함수에서 사용이 끝난 ESI 레지스터부터 EBP 레지스터까지를 스택에
	pop edx					; 삽입된 값을 이용해서 복원
	pop ecx					; 스택은 가장 마지막에 들어간 데이터가 가장 먼저 나오는
	pop ebx					; 자료구조이므로 삽입의 역순으로
	pop eax					; 제거해야 함
	pop ebp					; 베이스 포인터 레지스터(EBP) 복원
	ret						; 함수를 호출한 다음 코드의 위치로 복귀

; IA-32e 모드로 전환하고 64비트 커널을 수행
; 파라미터: 없음
kSwitchAndExecute64bitKernel:
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; CR4 컨트롤 레지스터의 PAE 비트를 1로 설정
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov eax, cr4	; CR4 컨트롤 레지스터의 값을 EAX레지스터에 저장
	or eax, 0x20	; PAE 비트(비트 5)를 1로 설정
	mov cr4, eax	; PAE 비트가 1로 설정된 값을 CR4 컨트롤 레지스터에 저장

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; CR3 컨트롤 레지스터에 RML4 테이블의 어드레스와 캐시 활성화
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov eax, 0x100000	; EAX레지스터에 PML4 테이블이 존재하는 0x100000(1MB)를 저장
	mov cr3, eax		; CR3 컨트롤 레지스터에 0x100000(1MB)를 저장

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; IA32_EFER.LME를 1로 설정하여 IA-32e 모드를 활성화
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov ecx, 0xC0000080	; IA32_EFER MSR 레지스터의 어드레스를 저장
	rdmsr				; MSR 레지스터를 읽기

	or eax, 0x0100		; EAX 레지스터에 저장된 IA32_EFER MSR의 하위 32비트에서
						; LME 비트(비트 8)을 1로 설정
	wrmsr				; MSR 레지스터에 쓰기
