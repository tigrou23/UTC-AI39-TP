XENO_CONFIG := $(SDKTARGETSYSROOT)/usr/bin/xeno-config
CFLAGS := $(shell $(XENO_CONFIG) --alchemy --cflags)
LDFLAGS := $(shell $(XENO_CONFIG) --alchemy --ldflags)
CC := $(shell $(XENO_CONFIG) --cc)

EXECUTABLE := pathfinder

all: $(EXECUTABLE)

%: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)
