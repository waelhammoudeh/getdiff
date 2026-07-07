# this "makefile" is for getdiff (download differs) program. It is to be
# placed in the root directory along with src and myinclude directories.
# getdiff/
#   |--src/
#   |--myinclude/
#   |--makefile
# 
# This program requires "libcurl" to be installed in the system.
# Run make from the root directory, it will build "getdiff" executable there.
# Note: there is no uninstall target! clean does NOT undo install.

SRC_DIR := src
OBJ_DIR := obj
EXEC := getdiff

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CPPFLAGS := -Imyinclude -MMD -MP

CFLAGS ?= -O2
CFLAGS += -Wall
LDLIBS := -lcurl

.PHONY: all clean

all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) :
	mkdir -p $@

clean:
	@$(RM) -rv $(OBJ_DIR) $(EXEC)

-include $(OBJ:.o=.d)

# PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

install: $(EXEC)

	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(EXEC) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/doc/getdiff/
	install -m 644 getdiff.conf.example $(DESTDIR)$(PREFIX)/doc/getdiff/
	install -m 644 getdiff.help $(DESTDIR)$(PREFIX)/doc/getdiff/
	install -m 644 README.changeFiles.md $(DESTDIR)$(PREFIX)/doc/getdiff/
	install -m 644 LICENSE $(DESTDIR)$(PREFIX)/doc/getdiff/
