ROOT = ../..
BUILDL = $(BUILD)/drivers/$(NAME)
BIN = $(BUILD)/driver_$(NAME).bin
LIBC = $(ROOT)/libc
LDCONF = $(LIBC)/ld.conf
SUBDIRS = . $(filter-out Makefile $(wildcard *.*),$(wildcard *))
BUILDDIRS = $(addprefix $(BUILDL)/,$(SUBDIRS))
DEPS = $(shell find $(BUILDDIRS) -mindepth 0 -maxdepth 1 -name "*.d")
APP = $(NAME).app
APPCPY = $(BUILD)/apps/$(APP)
LIBCA = $(BUILD)/libc.a

CC = gcc
# Note: we need -Wl,--build-id=none atm to prevent ld to generate the .note.gnu.build-id
# This seems to be put at the beginning of the binary and therefore the entry-point changes
CFLAGS = -nostdlib -nostartfiles -nodefaultlibs -I$(LIBC)/include -I../../lib/h \
	-Wl,-T,$(LDCONF) -Wl,--build-id=none $(CDEFFLAGS) $(ADDFLAGS)

ifeq ($(LINKTYPE),static)
	ADDLIBS += $(LIBCA)
endif

# sources
CSRC = $(shell find $(SUBDIRS) -mindepth 0 -maxdepth 1 -name "*.c")

# objects
START = $(BUILD)/libc_startup.o
COBJ = $(patsubst %.c,$(BUILDL)/%.o,$(CSRC))

.PHONY: all clean

all:	$(APPCPY) $(BIN)

$(BIN):	$(BUILDDIRS) $(APPDST) $(LDCONF) $(COBJ) $(START) $(ADDLIBS)
		@echo "	" LINKING $(BIN)
ifeq ($(LINKTYPE),static)
		@$(CC) $(CFLAGS) -o $(BIN) $(START) $(COBJ) $(ADDLIBS);
else
		@$(CC) $(CFLAGS) $(DLNKFLAGS) -o $(BIN) -lc $(START) $(COBJ) $(ADDLIBS);
endif
		@echo "	" COPYING ON DISK
		$(ROOT)/tools/disk.sh copy $(BIN) /sbin/$(NAME)

$(APPCPY): $(APP)
		$(ROOT)/tools/disk.sh copy $(APP) /apps/$(NAME)
		cp $(APP) $(APPCPY)

$(BUILDDIRS):
		@for i in $(BUILDDIRS); do \
			if [ ! -d $$i ]; then mkdir -p $$i; fi \
		done;

$(BUILDL)/%.o:		%.c
		@echo "	" CC $<
		@$(CC) $(CFLAGS) -o $@ -c $< -MMD

-include $(DEPS)

clean:
		rm -f $(APPCPY) $(BIN) $(COBJ) $(DEPS)