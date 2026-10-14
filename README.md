# PixelPoach

A homebrew tool for Game Boy Advance hardware that pulls photos from a real Game Boy Camera
onto the flash cart's save file, and sends them back so the camera keeps them.

By Scott Ratchford — [scottratchford.com](https://scottratchford.com)

Licensed under the **MIT License** — see [LICENSE.md](LICENSE.md).

## Where it came from

PixelPoach is the inverse of [GBScribe](https://github.com/KyberCritter/GBScribe), my homebrew
word processor and pixel-art tool for Game Boy Advance hardware that prints to a real Game Boy
Printer. GBScribe drives the printer by bit-banging the link port in general-purpose GPIO mode;
its printer layer was extracted into [AdvancePrint](https://github.com/KyberCritter/AdvancePrint).
PixelPoach extends that same link driver in the other direction, to speak the camera's
LINK/TRANSFER protocol and capture the photos it trades.

## Why it's a GBA app, not a Game Boy Color app

The target hardware is a Game Boy Advance running an **EZ-Flash Omega Definitive Edition**.
That cart runs `.gb`/`.gbc` files through **Goomba Color**, a software emulator on the GBA's ARM
CPU, which has no serial link implementation.
This prevents the use of accessories like the printer.

This requires PixelPoach to be a **native GBA application**.
It communicates with the Game Boy Camera by bit-banging the link port in general-purpose GPIO mode.

```txt
GBA ── link port (GPIO mode) ── GBC/GB-Pocket link cable ── Game Boy Camera
```

## Quick start

1. Build the ROM and copy it to the flash cart's SD card.
2. Boot PixelPoach on the GBA and connect the GBA to the GBC running the Game Boy Camera with
   the link cable.
3. To pull a photo, pick **Receive photo** on the GBA, then on the camera press the SELECT button
   to open the menu, move UP to LINK and press A, move DOWN to TRANSFER and press A, then choose
   SEND and pick the picture.
4. To put it back, pick **Send photo** on the GBA, then on the camera press SELECT, move UP to
   LINK and press A, move DOWN to TRANSFER and press A, then choose RECEIVE.
5. Power off, pull `PixelPoach.sav` off the SD card, and run the extractor to turn the photos
   into images (see below).

## Extracting photos

PixelPoach stores photos in a Game Boy Camera-format `.sav`. Pull the `.sav` off the flash cart's
SD card, then convert the photos to images with the bundled extractor:

```sh
python tools/extract_sav.py PixelPoach.sav            # -> photos/photo_01.png, ...
python tools/extract_sav.py PixelPoach.sav -o out --green --scale 6
```

- `-o, --outdir` — where to write the images (default `photos`)
- `-s, --scale` — integer upscale factor (default 4)
- `--green` — use the classic Game Boy green palette instead of grayscale

PNG output needs [Pillow](https://pillow.readthedocs.io/) (`pip install pillow`); without it the
script writes 24-bit BMP files, which any image viewer opens. Each photo is 128×112, written as
`photo_NN` in the order the camera lists them.

## Features

- Receives a photo the camera sends over LINK/TRANSFER and stores it in a camera-format `.sav`
  on the flash cart (the same 128 KB layout the camera's own SRAM uses)
- Sends a stored photo back, so a trade that would normally delete the source leaves the camera
  holding the original
- Browse and view stored photos on the GBA screen, in the camera's 128×112 2 bpp format
- The save backend (FLASH1M or plain SRAM/FRAM, as on the EZ-Flash Omega) is detected at runtime

## Controls

### Menu

| Action | Key |
| --- | --- |
| Change selection | D-Pad |
| Confirm | A |
| Back | B |

### Receive

On the camera, press the SELECT button to open the menu, move UP to LINK and press A, move DOWN
to TRANSFER and press A, then choose SEND and pick the photo. On the GBA, press A to start
listening: a `n/4102 bytes` counter tracks the transfer data (the 2 header bytes
are excluded), and the complete photo appears on
screen as soon as it arrives.

### Send

The GBA shows the stored photos one at a time. Move with the D-Pad; the `X/Y` counter in the
bottom-left is the photo's number, and the bottom bar shows the **A** send and **B** cancel
buttons. Press A to send the photo on screen, or B to go back. On the camera, press the SELECT
button to open the menu, move UP to LINK and press A, move DOWN to TRANSFER and press A, then
choose RECEIVE. The GBA shows `Photo sent` and the camera keeps the original.

## Building

Requires Docker (uses the official `devkitpro/devkitarm` image):

```sh
./build.sh          # -> PixelPoach.gba
./build.sh clean
```

Host-side tests:

```sh
make -C tests/host run
```

The portable layers (checksums, save layout, 2 bpp decode, trade round-trip) are verified without
hardware by compiling them with plain gcc. Generated assets (the font) are committed.

## Layout

```txt
src/        application source
  link/     GBA link-port bit-bang driver (master + slave, alternating clock)
  cam/      camera trade protocol, .sav builder, photo store
  ui/       title, menu, receive/send scenes, photo viewer
  storage/  save backend (runtime-detected FLASH1M or SRAM)
  gfx/      generated font
tests/host/ host-side unit tests
tools/      deploy, mGBA capture, and .sav extraction scripts
```

## Legal

PixelPoach is free software, licensed under the MIT License — see [LICENSE.md](LICENSE.md).

Not affiliated with, endorsed by, or sponsored by Nintendo. Game Boy, Game Boy Color, Game Boy Advance, Game Boy Camera and Game Boy Printer are trademarks of Nintendo.
