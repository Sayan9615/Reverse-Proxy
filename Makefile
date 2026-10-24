GCC := gcc
FLAGS := -Wall
target=proxy_server


all:$(target)

$(target):src/main.c
	$(GCC) $(FLAGS) $< -o $@


.PHONY:clean

clean:
	rm -f proxy_server