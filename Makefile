default: ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore ext2_checker

ext2_pathtokens.o: ext2_pathtokens.h ext2_pathtokens.c
	gcc -Wall -c ext2_pathtokens.c

ext2_utils.o: ext2.h ext2_utils.h ext2_utils.c
	gcc -Wall -c ext2_utils.c

ext2_mkdir: ext2_mkdir.c ext2_utils.o ext2_pathtokens.o
	gcc -Wall -o ext2_mkdir \
		ext2_mkdir.c ext2_utils.o ext2_pathtokens.o

ext2_cp: ext2_cp.c ext2_utils.o ext2_pathtokens.o
	gcc -Wall -o ext2_cp \
		ext2_cp.c ext2_utils.o ext2_pathtokens.o

ext2_ln: ext2_ln.c ext2_utils.o ext2_pathtokens.o
	gcc -Wall -o ext2_ln \
		ext2_ln.c ext2_utils.o ext2_pathtokens.o

ext2_rm: ext2_rm.c ext2_utils.o ext2_pathtokens.o
	gcc -Wall -o ext2_rm \
		ext2_rm.c ext2_utils.o ext2_pathtokens.o

ext2_restore: ext2_restore.c ext2_utils.o ext2_pathtokens.o
	gcc -Wall -o ext2_restore \
		ext2_restore.c ext2_utils.o ext2_pathtokens.o

ext2_checker: ext2_checker.c ext2_utils.o ext2_pathtokens.o
	gcc -Wall -o ext2_checker\
		ext2_checker.c ext2_utils.o ext2_pathtokens.o

clean:
	rm -rf ext2_utils.o ext2_pathtokens.o \
		ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore ext2_checker

