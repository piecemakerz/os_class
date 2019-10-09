[ORG 0x00]
[BITS 16]

SECTION .text
; ���� ���
; Entry.s
; ��밡�� �޸� ũ�� �˾Ƴ���
; Main.c, Page.c
; ������ ���̺�� ����޸� Ȱ��

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	�ڵ� ����
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
START:
; �޸� �� ù ��ȸ
	mov ax, 0x0000
	mov es, ax
.do_e820:
	mov di, 0x8004			; [es:di]��ġ�� 24����Ʈ ������ �о���δ�.
	xor ebx, ebx			; ���ͷ�Ʈ�� ����ϴ� configuration ���� 0���� �ʱ�ȭ�Ѵ�.
	xor ebp, ebp			; ��� ������ �޸𸮸� ����Ű�� ��Ʈ�� ������ bp�� �����Ѵ�.
	mov edx, 0x0534D4150	; edx�� "SMAP"�� ��ġ��Ų��
	mov eax, 0xe820
	mov [es:di + 20], dword 1
	mov ecx, 24
	int 0x15

	jc short .failed
	mov edx, 0x0534D4150
	cmp eax, edx			; �������� �� eax�� "SMAP"���� �ʱ�ȭ�Ǿ��� ���̴�
	jne short .failed		; ���ͷ�Ʈ ���� �� ����
	test ebx, ebx			; ebx�� configuration ������, 0�̶�� ����Ʈ�� ���� ���������� �ǹ�
							; ù ��ȸ�� ����Ʈ�� ���̶�� �̴� ����� �ʿ䰡 ����.
	je short .failed
	jmp short .jmpin

; �޸� �� ��ͼ�ȸ
.e820lp:
	mov eax, 0xe820
	mov [es:di + 20], dword 1
	mov ecx, 24				; 24����Ʈ ��û
	int 0x15

	jc short .e820f			; carry �÷��װ� Ȱ��ȭ�Ǿ��ٴ� ���� ����Ʈ�� ���� �̹� �����ߴٴ� �ǹ��̴�.
	mov edx, 0x0534D4150

; 24����Ʈ�� ������ �о����� Ȯ��
.jmpin:
	jcxz .skipent				; ���� 0�� ��Ʈ���� ��� ��ŵ�Ѵ�. (CX=0�϶� jump)
	cmp cl, 20					; 24����Ʈ�� �о����� Ȯ��
	jbe short .notext			; 24����Ʈ�� ���� ���� ���
	test byte[es:di + 20], 1	; 24����Ʈ�� �о��ٸ�, �� �� 4����Ʈ�� ���������� Ȯ��
	je short .skipent			; �������� �ʾҴٸ� �ش� ��Ʈ�� ��ŵ

; �޸� ���� ���� Ȯ�� �� ��밡�� �޸� �������� Ȯ��
.notext:
	mov ecx, dword[es:di + 8]	; �޸� ���� ������ lower uint32_t�� ��´�
	or ecx, dword[es:di + 12]	; ���̰� 0������ üũ�ϱ� ���� upper uint32_t�� or������ �Ѵ�
	jz short .skipent			; ���� uint64_t�� ���̰� 0�̶�� ��Ʈ���� ��ŵ�Ѵ�.

	add ebp, dword[es:di + 8]

; �̹� ��Ʈ�� ��ŵ�ϱ�
.skipent:
	test ebx, ebx			; ebx�� 0�̶�� ����Ʈ�� ���̴�.
	jne short .e820lp		; �ƴ϶�� ��� ����

; ��� ���� ó��
.e820f:
	clc					; ĳ�� �÷��� clear
	jmp short .e820end	; ��� ����

; ���ͷ�Ʈ ȣ�� ���н�
.failed:
	stc				; ĳ�� �÷��� 1�� ��Ʈ
	jmp short .e820end

.e820end:					; ���
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

	xor edx, edx			; mb���� ȯ��
	mov eax, ebp
	mov ecx, 0x100000
	div ecx
							; �ش� ���ο��� eax�� n�ڸ� 
	mov di,508				; ���� ��, MB��� �ε����� ������ ��
	mov ecx, 10
	.DIVISIONLOOP:
		xor edx, edx
		div ecx				; �� eax, ������ edx
		add dl, '0'
		mov byte[es:di], dl
		sub di, 2
		or eax, eax			; ���� 0�� �ƴ��� �˻��Ѵ�
		jnz .DIVISIONLOOP	; ���� ���̻� ���� ������ ������ �����Ѵ�
	
	mov byte[es:510], 'M'	; M
	mov byte[es:512], 'B'	; B

	jmp .count

.count:
	mov ax, 0x1000	; ��ȣ ��� ��Ʈ�� ����Ʈ�� ���� ��巹��(0x10000)��
					; ���׸�Ʈ �������� ������ ��ȯ
	mov ds, ax		; DS ���׸�Ʈ �������Ϳ� ����
	mov es, ax		; ES ���׸�Ʈ �������Ϳ� ����

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; A20 ����Ʈ Ȱ��ȭ -> p230
	; BIOS�� �̿��� ��ȯ�� �������� �� �ý��� ��Ʈ�� ��Ʈ�� ��ȯ �õ�
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; BIOS ���񽺸� ����ؼ� A20 ����Ʈ�� Ȱ��ȭ
	mov ax, 0x2401		; A20 ����Ʈ Ȱ��ȭ ���� ����
	int 0x15			; BIOS ���ͷ�Ʈ ���� ȣ��

	jc .A20GATEERROR	; A20 ����Ʈ Ȱ��ȭ�� �����ߴ��� Ȯ��
	jmp .A20GATESUCCESS

.A20GATEERROR:
	; ���� �߻� ��, �ý��� ��Ʈ�� ��Ʈ�� ��ȯ �õ�
	in al, 0x92			; �ý��� ��Ʈ�� ��Ʈ(0x92)���� 1����Ʈ�� �о� AL �������Ϳ� ����
	or al, 0x02			; ���� ���� A20 ����Ʈ ��Ʈ(��Ʈ1)�� 1�� ����
	and al, 0xFE		; �ý��� ���� ������ ���� 0xFE�� AND �����Ͽ� ��Ʈ 0�� 0���� ����
	out 0x92, al		; �ý��� ��Ʈ�� ��Ʈ(0x92)�� ����� ���� 1 ����Ʈ ����
.A20GATESUCCESS:
	cli				; ���ͷ�Ʈ�� �߻����� ���ϵ��� ����
	lgdt[GDTR]		; GDTR �ڷᱸ���� ���μ����� �����Ͽ� GDT ���̺��� �ε�

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; ��ȣ ���� ����
	; Disable Paging, Disable Cache, Internal FPU, Disable Align Check,
	; Enable ProtectedMode
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov eax, 0x4000003B
	mov cr0, eax	; CR0 ��Ʈ�� �������Ϳ� ������ ������ �÷��׸� �����Ͽ�
					; ��ȣ ���� ��ȯ

	; Ŀ�� �ڵ� ���׸�Ʈ�� 0x00�� �������� �ϴ� ������ ��ü�ϰ� EIP�� ���� 0x00�� �������� �缳��
	; CS ���׸�Ʈ ������ : EIP (������ ������ ��ɾ� �ּҸ� �����ϴ� ��������)
	jmp dword 0x18: (PROTECTEDMODE - $$ + 0x10000)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; ��ȣ ���� ����

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[BITS 32]			; ������ �ڵ�� 32��Ʈ �ڵ�� ����
PROTECTEDMODE:
	mov ax, 0x20	; ��ȣ ��� Ŀ�ο� ������ ���׸�Ʈ ��ũ���͸� AX �������Ϳ� ����
	mov ds, ax		; DS ���׸�Ʈ �����Ϳ� ����
	mov es, ax		; ES ���׸�Ʈ �����Ϳ� ����
	mov fs, ax		; FS ���׸�Ʈ �����Ϳ� ����
	mov gs, ax		; GS ���׸�Ʈ �����Ϳ� ����

	; ������ 0x00000000~0x0000FFFF ������ 64KB ũ��� ����
	mov ss, ax		; SS ���׸�Ʈ �����Ϳ� ����
	mov esp, 0xFFFE	; ESP ���������� ��巹���� 0xFFFE�� ����
	mov ebp, 0xFFFE	; EBP ���������� ��巹���� 0xFFFE�� ����
	
	; ȭ�鿡 ��ȣ ���� ��ȯ�Ǿ��ٴ� �޽����� ��´�.
	mov esi, ( SWITCHSUCCESSMESSAGE - $$ + 0x10000 )	; ����� �޽����� ��巹���� ���ÿ� ����
	mov edi, 640
	.MESSAGELOOP:				; �޽����� ����ϴ� ����
		mov cl, byte[esi]
		cmp cl, 0				; ����� ���ڿ� 0�� ��
		je .MESSAGEEND			; ������ ������ ���� 0�̸� ���ڿ��� ����Ǿ����� �ǹ��ϹǷ�
								; .MESSAGEEND�� �̵��Ͽ� ���� ��� ����
		mov byte[edi+0xB8000], cl		; 0�� �ƴ϶�� ���� �޸� ��巹��
										; 0xB8000 + EDI�� ���ڸ� ���
		inc esi				; ESI �������Ϳ� 1�� ���Ͽ� ���� ���ڿ��� �̵�
		add edi, 2				; EDI �������Ϳ� 2�� ���Ͽ� ���� �޸��� ���� ���� ��ġ�� �̵�
								; ���� �޸𸮴� (����, �Ӽ�)�� ������ �����ǹǷ� ���ڸ� ����Ϸ���
								; 2�� ���ؾ� ��
		jmp .MESSAGELOOP		; �޽��� ��� ������ �̵��Ͽ� ���� ���ڸ� ���
	.MESSAGEEND:

	; CS ���׸�Ʈ �����͸� Ŀ�� �ڵ� ��ũ����(0x08)�� �����ϸ鼭
	; 0x10200 ��巹��(C��� Ŀ���� �ִ� ��巹��)�� �̵�
	jmp dword 0x18: 0x10200 ; C ��� Ŀ���� �����ϴ� 0x10200 ��巹���� �̵��Ͽ� C��� Ŀ�� ����

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	������ ����
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[BITS 32]
; �Ʒ��� �����͵��� 8����Ʈ�� ���� �����ϱ� ���� �߰�
align 8, db 0

; GDTR�� ���� 8byte�� �����ϱ� ���� �߰�
dw 0x0000
; GDTR �ڷᱸ�� ����
GDTR:
	dw GDTEND-GDT-1		; �Ʒ��� ��ġ�ϴ� GDT ���̺��� ��ü ũ��
	dd (GDT-$$+0x10000)	; �Ʒ��� ��ġ�ϴ� GDT ���̺��� ���� ��巹��

; GDT ���̺� ����
GDT:
	; ��(NULL) ��ũ����, �ݵ�� 0���� �ʱ�ȭ�ؾ� ��
	NULLDescriptor:
		dw 0x0000
		dw 0x0000
		db 0x00
		db 0x00
		db 0x00
		db 0x00

	; IA-32e ��� Ŀ�ο� �ڵ� ���׸�Ʈ ��ũ����
	IA_32eCODEDESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x9A
		db 0xAF
		db 0x00

	; IA-32e ��� Ŀ�ο� ������ ���׸�Ʈ ��ũ����
	IA_32eDATADESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x92
		db 0xAF
		db 0x00

	; ��ȣ ��� Ŀ�ο� �ڵ� ���׸�Ʈ ��ũ����
	CODEDESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x9A
		db 0xCF
		db 0x00

	; ��ȣ ��� Ŀ�ο� ������ ���׸�Ʈ ��ũ����
	DATADESCRIPTOR:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x92
		db 0xCF
		db 0x00
GDTEND:

; ��ȣ ���� ��ȯ�Ǿ��ٴ� �޽���
SWITCHSUCCESSMESSAGE: db 'Switch To Protected Mode Success~!!',0

times 512 - ($ - $$) db 0x00	; 512����Ʈ�� ���߱� ���� ���� �κ��� 0���� ä��
