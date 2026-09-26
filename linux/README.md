# Cricket for Linux

A native Linux port of Cricket, the multi-server IRC client for Haiku OS. It is
written with Qt 6, KDE's own toolkit, so it looks and behaves like a normal KDE
Plasma app and runs natively on Wayland (and X11).

The Haiku version in the repository root is unchanged. This port lives in `linux/`
and shares none of its BeAPI code, but it reads the same `cricketConfig.txt`
format.

## Install on CachyOS / Arch

```sh
sudo pacman -S --needed base-devel cmake git qt6-base qt6-wayland aspell aspell-en openssl
git clone https://github.com/ablyssx74/cricket.git
cd cricket/linux/packaging
makepkg -si
```

This installs `cricket` into your application menu (Internet → Cricket).

### Or build and run without installing

```sh
cd cricket/linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cricket
```

`sudo cmake --install build` installs it to `/usr/local`.

Command-line options:

| Option | Meaning |
|---|---|
| `--no-autoconnect` | Start without connecting to networks marked "Connect on startup" |
| `--debug` | Print raw IRC traffic to the terminal |

## Features

* Several networks at once, with a server → channel/query tree
* TLS, with optional certificate verification per server
* SASL PLAIN, and CertFP (TLS client certificate) with SASL EXTERNAL
* One-click CertFP keypair generation. If you are connected, Cricket sends
  NickServ `CERT ADD` for you.
* Automatic NickServ IDENTIFY when SASL isn't used
* Alternate nicks when your nick is taken
* Auto-join channels (keys supported), plus commands to run after connecting
* Auto-reconnect with backoff (5s → 60s, gives up after 8 tries), then rejoins
  your channels
* mIRC colours and formatting (bold, italic, underline, strikethrough, reverse,
  hex colours)
* Clickable links (opened in your default browser)
* Emoticons shown as emoji, plus an emoticon picker
* Per-nick colour rules (wildcards) and stable automatic nick colours
* Ignore list with `*` and `?` wildcards
* Highlighting when your nick is mentioned, and KDE desktop notifications for
  mentions and private messages
* aspell spell checking in the input line. Right-click a word for suggestions.
* Gemini live translation of incoming and/or outgoing messages
* Channel list browser (`/list`) with filtering and sorting
* Channel modes dialog (+m +s +i +t +n +c, key, limit), ban list, op/voice/kick/ban
  from the user list
* Knock and invite prompts, and prompts for channel keys and invite-only channels
* Away status (with away-notify), nick tab-completion, input history
* Per-server background image and opacity, font sizes, and timestamp interval
* Optional chat logging to `~/.local/share/cricket/logs`
* Checks GitHub for new versions

## Files

| Path | Contents |
|---|---|
| `~/.config/cricket/cricketConfig.txt` | Settings (JSON, same format as the Haiku build) |
| `~/.config/cricket/certs/` | CertFP certificates and keys |
| `~/.local/share/cricket/logs/` | Chat logs, if enabled |

To bring your Haiku settings over, copy
`/boot/home/config/settings/cricket/cricketConfig.txt` (and the `certs` folder if
you use CertFP) into `~/.config/cricket/`.

## Keyboard

| Key | Action |
|---|---|
| Enter | Send |
| Up / Down | Input history |
| Tab | Complete a nick (press again to cycle) |
| Alt+Up / Alt+Down | Previous / next buffer |
| Ctrl+J | Join a channel |
| Ctrl+L | Clear the current buffer |
| Ctrl+Q | Quit |

## Commands

`/join #chan [key]`, `/part [#chan] [reason]`, `/msg nick text`,
`/query nick`, `/me action`, `/nick new`, `/topic text`, `/whois nick`,
`/notice target text`, `/ctcp nick VERSION|PING|TIME`,
`/op`, `/deop`, `/voice`, `/devoice nick…`, `/kick nick [reason]`,
`/ban` / `/unban nick|mask`, `/banlist`, `/invite nick [#chan]`, `/mode …`,
`/knock #chan`, `/away [msg]`, `/back`, `/ignore mask`, `/unignore mask`,
`/list`, `/names`, `/clear`, `/close`, `/connect`, `/disconnect`,
`/reconnect`, `/quit [msg]`, `/quote RAW`. Any other `/command` goes to the
server as is. Start a line with `//` to send text that begins with a slash.

## Not ported (yet)

* Timed op/deop and timed unban from the Haiku context menus
* The custom-drawn chat renderer toggle (the Linux view always renders rich text)
* PortMapper / DCC (also not wired up on Haiku yet)
