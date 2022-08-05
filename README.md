Musix player
============

```
➜  msxp music/C64/Last_Ninja_2.sid
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃ Last Ninja 2 ( The Street (loader)) / Matt Gray                                           ┃
┣━━━━━━━━━━━━━━━┳━━━━━━┳━━━━━━━┳━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ 00:15 / 05:03 ┃ SONG ┃ 03/13 ┃ FORMAT ┃ SID                                               ┃
┗━━━━━━━━━━━━━━━┻━━━━━━┻━━━━━━━┻━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
```

### Building & Running

```
make
build/debug/msxp music/Warhawk.sap
```

### Installing

Copy `msxp` to your path and the `data` directory as
`/usr/local/share/musix` or `~/.local/share/musix`
(Or keep the `data` directory along side the excutable).

### Using

`msxp [options] <musicfiles ...>`

* `-d` run in background
* `-n` play next file
* `-p` play previous file
* `-s <no>` Set subsong
* `-o` Write audio to stdout
* `-q` Quit background player
* `-a` Add files to queue instead of replacing queue

### Text UI

* `[ENTER]` / `[BACKSPACE]` for next/previous file
* `[LEFT]` / `[RIGHT]` for sub song
* `[ESC]` to detach and keep playing
* `q` to quit

(Run without file arguments to activate UI again)

### Converting to MP3

`msxp -o <file> | lame -r file.mp3`

### Playing multiple files
(Examples require downloaded copy of MODLAND :)

Play all tracked music by Purple Motion (and shuffle it)
```
fd ".(mod|xm|s3m)$" ~/MODLAND/*/Purple\ Motion | sort -R | msxp && msxp -c
```

### LUA & Themes

Copy `data/example.lua` to `~/.config/musix.lua` and edit it to change
the theme etc (TBC).
