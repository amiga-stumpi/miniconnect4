CC=/opt/amiga/bin/m68k-amigaos-gcc
CFLAGS=-Os -Wall -Wextra -fomit-frame-pointer -mcrt=nix13 -fno-builtin -DAMIGA_OS13 -Iinclude -I/opt/amiga-netinclude/include
LDFLAGS=-mcrt=nix13
BUILD=build
TARGET=$(BUILD)/MiniConnect4
SERVER_TARGET=$(BUILD)/miniconnect4-lobby-server
OBJS=$(BUILD)/main.o $(BUILD)/util.o $(BUILD)/config.o $(BUILD)/locale.o $(BUILD)/game.o $(BUILD)/ai.o $(BUILD)/gui.o $(BUILD)/draw.o $(BUILD)/net.o $(BUILD)/protocol.o $(BUILD)/chat.o $(BUILD)/sound.o $(BUILD)/sound_data.o

.PHONY: all clean server

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

server: $(SERVER_TARGET)

$(SERVER_TARGET): server/miniconnect4_lobby_server.c | $(BUILD)
	cc -O2 -Wall -Wextra -o $@ $<

clean:
	rm -rf $(BUILD)
