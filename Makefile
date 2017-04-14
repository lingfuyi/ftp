target:ftp
	gcc -o ftp ftp.c
PHONY:clean
clean:
	rm *.o
