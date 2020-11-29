SUBDIRS = src

# Compiler flags common to all build modes
CFLAGS_comm := -std=c11 -m64
CFLAGS_comm += -Wall -Wextra -Werror
CFLAGS_comm += -Wformat-security -Wduplicated-cond -Wfloat-equal -Wshadow -Wconversion -Wjump-misses-init -Wlogical-not-parentheses -Wnull-dereference
CFLAGS_comm += -D_GNU_SOURCE

# Mode-specific compiler flags
CFLAGS_debug := -g
CFLAGS_opt   := -O -flto

# The default mode
MODE = opt

CFLAGS = $(CFLAGS_$(MODE))
CFLAGS+= $(CFLAGS_comm)

HDR :=
SRC_src := src/main.c
SRC := Makefile $(SRC_src)
OBJ_src := $(patsubst src/%.c,obj/%.o,$(SRC_src))
OBJ := $(OBJ_src)
BIN := ls

.PHONY: clean clobber tar help

default: all

all: $(BIN)

help:
	@echo "Targets:"
	@echo "    all     - build optimized version (default)"
	@echo "    tar     - package source and makefiles"
	@echo "    clean   - remove object and generated source files"
	@echo "	   clobber - clean and remove auto-generated dependencies"
	@echo
	@echo "Options:"
	@echo "    MODE=opt   - build optimized version (default)"
	@echo "        =debug - build debug version"

DEPS = $(OBJ:.o=.d)
INCLUDES = $(patsubst %,-I %/,$(SUBDIRS))

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJ)

clean:
	$(RM) $(BIN)
	$(RM) $(OBJ)
	$(RM) $(GEN_SRC)

clobber: clean
	$(RM) $(DEPS)

tar:
	@tar czvf ls.tar.gz $(SRC) $(HDR) > /dev/null
	@echo "Source tree packed into ls.tar.gz"

obj/%.o: src/%.c
	$(CC) -c $(CFLAGS) $< -o $@

obj/%.d: src/%.c
	@mkdir -p ./obj/
	$(CC) $(CFLAGS) -MT obj/$*.o -MM $< > $@

-include $(DEPS)
