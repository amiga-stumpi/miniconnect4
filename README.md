# MiniConnect4

MiniConnect4 is an AmigaOS 1.3 Workbench implementation of Connect Four.
It is designed as a small online-capable game using `bsdsocket.library` and a simple direct host/client connection.

## Features

- Classic 7x6 Connect Four board.
- Local two-player mode.
- Direct TCP host/client online mode for LAN or reachable Internet hosts.
- Small falling-disc animation.
- Optional chat line using the same TCP connection.
- Resizable Workbench window.
- Window position, size and basic settings saved in `MiniConnect4.conf`.
- No GadTools dependency.

## Build

```sh
make clean && make
```

Target binary:

```text
build/MiniConnect4
```

## Controls

- Click a column to drop a disc.
- `Host` waits for another MiniConnect4 client on port 4544.
- `Join` connects to the configured host from `MiniConnect4.conf`.
- Type text and press Enter to send chat when chat is enabled.

## Config

A default config is created/saved as `MiniConnect4.conf` in the current directory.
Edit `host=` and `port=` to set the lobby server used by `Network -> Lobby`.


## Lobby server

Build the Linux lobby server with:

```sh
make server
```

Run it with:

```sh
build/miniconnect4-lobby-server 4544
```

Run it in the background with:

```sh
build/miniconnect4-lobby-server -D 4544
```

MiniConnect4 clients connect through `Network -> Lobby`. Idle players can chat in the lobby and click another idle player to start a game.
