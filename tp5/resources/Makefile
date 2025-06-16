XENO_CONFIG := $(SDKTARGETSYSROOT)/usr/bin/xeno-config
CFLAGS := $(shell $(XENO_CONFIG) --alchemy --rtdm --cflags)
LDFLAGS := $(shell $(XENO_CONFIG) --alchemy --rtdm --ldflags)
CC := $(shell $(XENO_CONFIG) --cc)

EXECUTABLE := gpio

all: $(EXECUTABLE)

%: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)
