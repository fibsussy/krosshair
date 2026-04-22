LIBRARY = lib/krosshair.so

CCFLAGS = -Wall -std=c99 -fPIC -shared -I./include/
LDFLAGS = -ldl -lm

# Debug build by default; use `make release` or `make RELEASE=1` for release
ifdef RELEASE
CCFLAGS += -O2 -DNDEBUG
else
CCFLAGS += -ggdb
endif

SOURCES = $(shell find src -type f -name "*.c")

.PHONY: all release clean install

all:
	mkdir -p lib
	$(CC) $(CCFLAGS) $(SOURCES) $(LDFLAGS) -o $(LIBRARY)

release:
	$(MAKE) RELEASE=1 all

install:
	sudo mkdir -p /usr/lib/krosshair
	sudo cp $(LIBRARY) /usr/lib/krosshair/krosshair.so
	sudo cp krosshair.json /usr/share/vulkan/implicit_layer.d/krosshair.json

clean:
	rm -f $(LIBRARY)
