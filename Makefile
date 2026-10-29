GCC := gcc
FLAGS := -Wall
target=proxy_server
SRCS = src/main.c src/config.c

all:$(target)

$(target):$(SRCS)
	$(GCC) $(FLAGS) $^ -o $@


.PHONY:clean

clean:
	rm -f proxy_server