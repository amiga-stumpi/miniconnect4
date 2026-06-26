CC=/opt/amiga/bin/m68k-amigaos-gcc
CFLAGS=-Os -Wall -Wextra -fomit-frame-pointer -mcrt=nix13 -fno-builtin -DAMIGA_OS13 -Iinclude -I/opt/amiga-netinclude/include
LDFLAGS=-mcrt=nix13
BUILD=build
TARGET=$(BUILD)/MiniConnect4
OBJS=$(BUILD)/main.o $(BUILD)/util.o $(BUILD)/config.o $(BUILD)/game.o $(BUILD)/gui.o $(BUILD)/draw.o $(BUILD)/net.o $(BUILD)/protocol.o $(BUILD)/chat.o

.PHONY: all clean

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -rf $(BUILD)
