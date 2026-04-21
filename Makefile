LIBRARY = lib/krosshair.so

CCFLAGS = -Wall -std=c99 -ggdb -fPIC -shared -I./include/
LDFLAGS = -ldl -lm

SOURCES = $(shell find src -type f -name "*.c")

.PHONY: all clean install

all:
	mkdir -p lib
	$(CC) $(CCFLAGS) $(SOURCES) $(LDFLAGS) -o $(LIBRARY)

install:
	sudo mkdir -p /usr/lib/krosshair
	sudo cp $(LIBRARY) /usr/lib/krosshair/krosshair.so
	sudo cp krosshair.json /usr/share/vulkan/implicit_layer.d/krosshair.json

clean:
	rm -f $(LIBRARY)
