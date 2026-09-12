# Install Cardputer Oracle

This application targets **M5Stack Cardputer ADV (ESP32-S3)** and the M5Apps launcher.
Use a FAT32 microSD card. The original Cardputer is not a supported target.

## What you need

- A charged Cardputer **ADV**, a microSD card and a computer with a card reader.
- One complete Oracle release ZIP for your language. You do not need Python,
  PlatformIO, ElevenLabs or an API key to install a ready-made offline release.
- A USB-C **data** cable only if M5Apps is not installed yet. Once M5Apps works,
  Oracle installation and updates can be done with the card reader alone.
- Optional: a configured LiteLLM server for online Tarot and a supported BQ95
  camera/printer for paper receipts. Neither is required for offline fortunes.

## First time: install M5Apps on the device

Skip this section if the device already boots into M5Apps with its Installer app.
M5Apps is the launcher; Oracle is an application installed inside it. Installing
a launcher replaces the device's current firmware, so preserve anything you need
from the existing setup before doing this.

1. Install [M5Burner from M5Stack](https://docs.m5stack.com/en/download).
2. Connect the ADV to the computer using a USB data cable. Charging alone does
   not prove the cable carries data. Check for a new device/serial port in
   M5Burner; try a direct computer port and another cable if none appears.
3. If download mode is needed, set the ADV power switch to OFF, hold **G0 (Go)**
   while powering the device on, then release it. This follows
   [M5Stack's ADV download-mode instructions](https://docs.m5stack.com/en/uiflow2/cardputer-adv/program).
4. In M5Burner, select the **CardPuter** group, find **M5Apps**, select the detected
   port and use **Burn**, following its prompts. Use a release supporting ADV.
5. Restart normally, without holding Go, and verify that M5Apps opens.

See the [M5Apps installation instructions](https://github.com/d4rkmen/M5Apps#installation)
for launcher-specific changes. Do not flash the Oracle application BIN at address
`0x0`: it is not a complete flash image containing a bootloader and partition table.

## Choose a language

Release archives end in `-en.zip`, `-ru.zip` or `-zh.zip` (Simplified Chinese). English is the default.
Each archive contains a matching application BIN and ORACLE folder: interface,
fortunes, voice recordings, Tarot output and receipts use the same language.
Install both parts when changing languages. A mismatched SD catalog is rejected;
the firmware falls back to its own small, silent deck.

Keep ORACLE/fonts/chinese12.bin for Chinese Tarot's full glyph coverage. Chinese
offline text and the UI also have embedded glyphs. The Chinese build accepts
English/Pinyin questions and returns Chinese answers; it has no Chinese IME yet.

| Archive suffix | Interface, fortunes, voice and paper | Build command, if building from source |
| --- | --- | --- |
| `-en.zip` | English; default | `python tools/build_release.py` |
| `-ru.zip` | Russian | `python tools/build_release.py --language ru` |
| `-zh.zip` | Simplified Chinese, Mandarin voice | `python tools/build_release.py --language zh` |

Use the complete ZIP, not just a standalone `.bin`. The repository's `sdcard/`
folder also contains development media and legacy Russian resources; it is not
the ready-to-install English or Chinese pack. A source build produces the matching
resources under `build/packs/<language>/ORACLE`.

## Prepare the microSD

1. Turn off the Cardputer before removing its card. Insert the card into the
   computer's reader and confirm you selected the microSD, not another drive.
2. Back up existing files. Use **FAT32** for this application; do not assume that
   a filesystem readable by the launcher is also supported by Oracle. Formatting
   erases the card and is unnecessary if it is already working as FAT32.
3. Extract the selected ZIP on the computer. Reserve space for its **extracted**
   contents, not just the compressed ZIP size.
4. Copy the whole `ORACLE` folder directly to the card root. Copy the matching
   `Oracle-ADV-0.6.0-en.bin`, `-ru.bin` or `-zh.bin` alongside it. Keep filenames
   and subfolders intact. Do not nest them under the ZIP's enclosing directory.
5. If updating, preserve your private `ORACLE/config.json` as described below.
6. Safely eject the card, put it back into the powered-off ADV, then turn it on.

Correct English layout (the card's volume name can be anything):

```text
microSD root/
├── Oracle-ADV-0.6.0-en.bin
└── ORACLE/
    ├── index.json
    ├── cards/coat.json                 # 120 card JSON files
    ├── images/… .jpg                   # shared illustrations
    ├── audio/en/coat.wav               # 120 voice files
    ├── print/coat.bin                  # 120 receipt rasters
    ├── config.example.json
    ├── tls/roots.pem
    └── config.json                     # optional, created by you
```

For Russian, audio paths are `ORACLE/audio/coat.wav` (no `ru/` subfolder).
For Chinese, they are `ORACLE/audio/zh/coat.wav`, and the pack also contains
`ORACLE/fonts/chinese12.bin`. These are examples; copy the archive's layout as-is.
`microSD/Oracle-ADV-0.6.0-en/ORACLE/` and `microSD/ORACLE/ORACLE/` are incorrect.

## Install the application from SD

1. Open **Installer → SD** in M5Apps.
2. Select the root-level Oracle BIN matching the copied resources. Ignore macOS
   companion files beginning with `._`. Files in `ORACLE/print/` are receipt data,
   not installable applications, despite also ending in `.bin`.
3. Follow the installer's prompts to install or replace the Oracle application.
   Keep power connected or the battery charged until installation finishes.
4. Return to the launcher and open the installed application. Copying a BIN to
   SD alone does not install it; copying new resources alone does not update firmware.
5. Check that the header shows **ORACLE / 06.0** and **SD**. `DEMO` means that the
   full catalog was not loaded. Battery percentage is independent of this status.
6. Draw a fortune: check its picture, scrolling text and voice. Switch themes
   with left/right. Test Tarot and printing separately if configured.

After installation the app runs from device flash. Keep the `ORACLE` resources
on the card for normal use. The root-level installer BIN can be retained for
reinstallation or removed; it is not needed for playback.

## What each SD file does

| Path under the card root | Purpose | Keep or edit? |
| --- | --- | --- |
| `ORACLE/index.json` | Language, card IDs and theme index | Keep with its matching pack; do not hand-edit |
| `ORACLE/cards/*.json` | Individual fortune text, title and asset paths | Keep; changing text requires rebuilding speech and receipts |
| `ORACLE/images/*.jpg` | Screen illustrations | Keep filenames; referenced by cards |
| `ORACLE/audio/**/*.wav` | Offline narration | Keep the selected language's exact paths |
| `ORACLE/print/*.bin` | Pre-rendered offline receipts | Keep for printing; these are not firmware |
| `ORACLE/fonts/chinese12.bin` | Additional Chinese glyphs for online Tarot | Keep in the Chinese pack; never install it as firmware |
| `ORACLE/config.example.json` | Public settings template containing placeholders | Copy it to `config.json` to configure Tarot |
| `ORACLE/config.json` | Your Wi-Fi passwords and LiteLLM token | Optional for offline use; private; preserve across updates |
| `ORACLE/tls/roots.pem` | Trusted certificates for HTTPS | Keep for Tarot; not an API key |
| `ORACLE/diagnostic.txt` | Boot/reset diagnostic generated on the device | Optional; useful when reporting a reset |
| Root-level `README.md`, `SHA256SUMS`, `third_party/` from ZIP | Instructions, integrity hashes and licenses | Keep with your downloaded archive; not required by the device |

The release omits real credentials. Audio metadata JSON files in the source tree
are build-time records and are not required on the card. Synthesized button and
ritual sounds live in firmware and do not need additional SD sound files.

## Update, switch language or roll back

1. Keep a copy of the previously working ZIP. Back up the current `ORACLE` folder,
   especially `config.json`, to your computer.
2. Replace the on-card `ORACLE` resource folder with the complete new one and
   restore your private `config.json` into it. Preserve unrelated launcher folders.
   Folder replacement in a file manager can delete the old contents: make the backup first.
3. Copy and install the BIN from the **same** new ZIP through M5Apps.
4. Eject/reinsert safely and confirm `SD`, the chosen language and voice playback.

Use this procedure for both upgrades and language changes. Only one pack can be
active at `/ORACLE` at a time; there is no runtime language selector. To roll back,
install the previous BIN and restore the previous matching resources and config.
You do not need to reinstall M5Apps for a normal Oracle update.

## Optional Tarot configuration

Copy `ORACLE/config.example.json` to `ORACLE/config.json`. Edit it as plain UTF-8
JSON, not a rich-text document; make sure it is not accidentally named `config.json.txt`.

```json
{
  "schema": 1,
  "wifi": [{"ssid": "YOUR_HOTSPOT_SSID", "password": "YOUR_WIFI_PASSWORD"}],
  "tarot": {
    "host": "litellm.example.com",
    "model": "openai/gpt-5.6-luna",
    "token": "YOUR_LITELLM_VIRTUAL_KEY"
  }
}
```

Replace every placeholder. `wifi` accepts one to four saved networks in preference
order; SSIDs are case-sensitive. Use a reachable 2.4GHz network. `host` is a DNS
hostname **without** `https://`, a port or a path; the firmware sends HTTPS requests
to `/v1/responses`. `model` must exactly match a model available to your proxy key.
The device reads these settings separately from M5Apps' Wi-Fi configuration.
There is currently no on-device Oracle Wi-Fi password editor or network-saving UI.

Keep `tls/roots.pem`. HTTPS also needs working time synchronization. If your
server uses a different certificate chain, configure the corresponding trusted
root; do not disable certificate checks. The device connects directly to your
proxy and does not require a Mac to stay nearby. Offline themes still need no config.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No USB port, but the device charges | Data-capable cable, another computer port, then ADV download mode; charging is not a data test |
| `Chip id mismatch` | ADV/ESP32-S3 target, the actual Oracle application BIN, and a complete extracted download; do not force an incompatible image |
| BIN is not visible in Installer | Unzip first, put the application BIN at card root, use the SD source, check FAT32 and reinsert the card |
| Header says `DEMO` | `ORACLE/index.json` at the root path, intact catalog, matching BIN/pack language, FAT32 and card seating |
| Wrong language or silent fallback cards | Install the BIN and resources from the same language ZIP; do not copy the legacy source `sdcard/` over a release |
| No recorded voice | Confirm `SD`, unmute with M, raise volume with +, check WAV paths, press R on an offline result; online Tarot has no narration |
| Missing Chinese characters | Restore `ORACLE/fonts/chinese12.bin` from the Chinese ZIP; do not rename or install the font BIN |
| Wi-Fi network is not found | Hotspot is on, SSID spelling matches, and 2.4GHz is available; on compatible iPhones try Maximize Compatibility |
| Tarot fails while offline cards work | `config.json` syntax/name, reachable proxy, authorized model/key, TLS roots and network time; launcher Wi-Fi settings alone are insufficient |
| Printer not found or no paper | Open the BQ95 FunPrint QR screen, disable its auto-off, check paper and battery, then press P; other units may need a driver identifier change |
| `NVS` / `M5Apps` error on printing | Install the current Oracle BIN; older builds used incompatible Bluetooth persistence. Do not erase launcher NVS as a first fix |
| Reset immediately after drawing | Install the current matching BIN/pack, check battery/card, then save `ORACLE/diagnostic.txt` for diagnosis |

When reporting a problem, include the language/version, exact screen message and
whether it happened before animation, image, voice or printing. Share the diagnostic
file if available, but leave out your private `config.json`. New builds are software
verified; hardware behavior still needs checking on your own device.

## Controls

On the home screen, left/right changes themes; any other key starts a fortune.
The physical comma and slash keys work as left/right without Fn.
On a result, N/Enter/Space draws again, P prints a copy, R replays speech,
I shows the picture, and A toggles automatic scrolling. Scroll with ;/W and ./S.
M mutes audio, +/- adjusts volume, and Esc goes back. The physical backtick key
acts as Esc. On the question screen, Tab switches the typing layout between
English and Russian; it does not change the build's interface or reading language.

## Offline and online

The three offline themes contain 120 fortunes and need no account or API key.
Without a usable SD pack there are six silent fallback fortunes per offline theme.

For AI Tarot, copy ORACLE/config.example.json to ORACLE/config.json on the card
and fill in your own 2.4GHz Wi-Fi networks, HTTPS Responses-compatible gateway host, model and token.
The host must be reachable by the device. Keep the supplied TLS roots file.
Never commit your actual configuration. An empty question requests a general reading.
AI Tarot currently produces text and a printable receipt, without generated speech.
See [OpenAI-compatible endpoint setup](https://github.com/m0rg0t/cardputer-oracle/blob/main/docs/OPENAI_COMPATIBLE.md) for the precise API, model, token and TLS requirements. Chat Completions-only servers are not supported.

## Printing

The BQ95 camera must be on the FunPrint QR screen, with auto power-off disabled.
One automatic print is attempted when a fortune is revealed. P searches again or
prints another copy of that same result. An interrupted receipt is not automatically
retried; check the paper before pressing P. The large artwork is 336 dots wide on
a 384-dot receipt. The driver identifies the tested BQ95 unit; other units may need
the identifier changed in firmware/src/bq95_printer.cpp.

The new language builds are tested in software; installation, sound and Bluetooth
printing still need confirmation on physical hardware.
