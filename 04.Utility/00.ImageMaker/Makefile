all: ImageMaker.exe

#ImageMaker 빌드
ImageMaker.exe: ImageMaker.c
	gcc -o $@ $<
	
#소스 파일을 제외한 나머지 파일 정리
clean:
	rm -f ImageMaker.exe