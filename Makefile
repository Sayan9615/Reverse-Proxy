GCC := gcc
FLAGS := -Wall

TARGET = proxy_server
BACKEND_TARGET = backend_dummy

SRCS = src/main.c src/config.c src/health.c src/balancer.c src/proxy.c src/logger.c
BACKEND_SRCS = src/backend_dummy.c

all: $(TARGET) $(BACKEND_TARGET)

$(TARGET): $(SRCS)
	$(GCC) $(FLAGS) $^ -o $@

$(BACKEND_TARGET): $(BACKEND_SRCS)
	$(GCC) $(FLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f $(TARGET) $(BACKEND_TARGET) proxy.log
