#all: BootLoader Disk.img
all: BootLoader Kernel32 Disk.img

BootLoader:
	@echo 
	@echo ============== Build Boot Loader ===============
	@echo 
	
#00.BootLoader 디렉터리의 make 실행
	make -C 00.BootLoader

	@echo 
	@echo =============== Build Complete ===============
	@echo 
	
Kernel32:
	@echo
	@echo =============== Build 32bit Kernel ===============
	@echo 
	
	make -C 01.Kernel32
	
	@echo 
	@echo =============== Build Complete ===============
	@echo

Disk.img: 00.BootLoader/BootLoader.bin 01.Kernel32/Kernel32.bin
#Disk.img: 00.BootLoader/BootLoader.bin
	@echo 
	@echo =========== Disk Image Build Start ===========
	@echo 

	#cat $^ > Disk.img
	#cp 00.BootLoader/BootLoader.bin Disk.img
	./ImageMaker.exe $^
	
	@echo 
	@echo ============= All Build Complete =============
	@echo

run:
	qemu-system-x86_64 -L . -fda Disk.img -m 64 -localtime -M pc -rtc base=localtime
	
clean:
	make -C 00.BootLoader clean
	make -C 01.Kernel32 clean
	make -C 04.Utility clean
	rm -f Disk.img	
