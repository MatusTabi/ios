CC = gcc
CFLAGS = -c -std=gnu99 -Wall -Wextra -Werror -pedantic
LDLIBS = -pthread -lrt
EXEC = proj2
OBJS = proj2.o

$(EXEC): $(OBJS)

clean:
	rm $(EXEC) $(OBJS)