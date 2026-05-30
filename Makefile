CC = gcc
CFLAGS = -Wall -Werror -fPIC -O2 -Iinclude
LDFLAGS = -shared -ldl
TARGET = libcapi.so
STATIC = libcapi.a
SRCS = src/libcapi.c src/calib.c src/features.c src/builtins.c
OBJS = $(SRCS:.c=.o)
INSTALL_LIB = /usr/lib
INSTALL_INC = /usr/include/capi

EXT_DIR = extensions
EXT_SO = $(EXT_DIR)/socket.so

.PHONY: all clean install uninstall test cacli extensions

all: $(TARGET) $(STATIC) cacli extensions

extensions: $(EXT_SO)

$(EXT_SO): $(EXT_DIR)/socket.c
	@mkdir -p $(EXT_DIR)
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(STATIC): $(OBJS)
	ar rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(OBJS)
	$(CC) $(CFLAGS) -o tests/test_main tests/test_main.c $(OBJS) -ldl
	@echo "Running tests..."
	@cd tests && ./test_main

cacli: $(TARGET)
	@echo "Building cacli..."
	@cd cacli && go build -o cacli

install: $(TARGET) cacli extensions
	sudo mkdir -p $(INSTALL_INC)
	sudo cp $(TARGET) $(INSTALL_LIB)/
	sudo cp $(STATIC) $(INSTALL_LIB)/
	sudo cp include/libcapi.h include/calib.h $(INSTALL_INC)/
	sudo cp cacli/cacli /usr/local/bin/cacli
	sudo mkdir -p /usr/capi/libs
	sudo cp $(EXT_SO) /usr/capi/libs/
	sudo ldconfig

uninstall:
	sudo rm -f $(INSTALL_LIB)/$(TARGET)
	sudo rm -f $(INSTALL_LIB)/$(STATIC)
	sudo rm -rf $(INSTALL_INC)
	sudo rm -f /usr/local/bin/cacli
	sudo ldconfig

clean:
	rm -f $(OBJS) $(TARGET) $(STATIC) tests/test_main cacli/cacli
	rm -f $(EXT_DIR)/*.so
