CC = gcc
test_cpyhash: test_cpyhash.o
	${CC} -o test_cpyhash test_cpyhash.o

test_cpyhash.o: ../src/cpyhash.h ../src/cpyhashlib.h

.PHONY: clean
clean:
	-rm test_cpyhash test_cpyhash.o