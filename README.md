# Palnagotchi for M5Stack

A friendly unit for those lonely Pwnagotchis out there `(O_o )`. It's written to run on the M5Stack, see supported devices below.

This is a fork of [m5-palnagotchi](https://github.com/viniciusbo/m5-palnagotchi) by [@viniciusbo](https://github.com/viniciusbo).

## Pwngrid (WiFi)

The [Pwngrid](https://github.com/evilsocket/pwngrid) protocol works by sending WiFi beacon frames with a JSON serialized payload in WiFi AC headers, containing the Pwnagotchi's data (name, face, pwns, brain policy, and more). That's how nearby Pwnagotchis detect and learn from each other. By crafting a custom beacon frame, Palnagotchi can appear as a Pwnagotchi to other Pwnagotchis on the grid.

## PwnBeacon (BLE)

In addition to the WiFi-based Pwngrid protocol, Palnagotchi supports **[PwnBeacon](https://github.com/pfefferle/PwnBeacon)** — a compact BLE (Bluetooth Low Energy) protocol for peer discovery. PwnBeacon advertises a binary packet containing the unit's name, fingerprint, and pwn counts via BLE, and scans for other PwnBeacon-enabled devices nearby. This allows Palnagotchis and Palnagotchis to find each other over Bluetooth, complementing the WiFi beacon approach.

PwnBeacon also exposes a GATT service with characteristics for identity, face, name, signal strength, and messages, enabling richer interactions between connected peers.

## Supported devices

- Cardputer
- Cardputer ADV
- StickC Plus2
- StickC (untested)
- AtomS3
- Dial
- DinMeter
- Core

## Usage

- Run the app to start advertisement.
- Button layouts:
  - Cardputer / Cardputer ADV: `m` or `` ` `` toggles the menu. Use `.`/`/`/`Tab` for next, `,`/`;` for prev and `Enter` to select.
  - StickC / StickC Plus / StickC Plus2: Long press M5 button to toggle menu. Power button for next, side button for prev and M5 button (short press) to select.
  - AtomS3(R) / Core: Long press to toggle menu. Double tap for next, triple tap for prev and short press to select.
  - Dial: Long press M5 button to toggle menu. Rotary encoder to navigate. Short press M5 button to select.
  - DinMeter: Long press rotary encoder to toggle menu. Rotary encoder to navigate. Short press to select.
- Top bar shows battery level and uptime.
- Bottom bar shows total friends made in this run, all time total friends in parenthesis (needs EEPROM or SD card) and signal strength indicator of closest friend.
- Nearby pwnagotchis show all nearby units and their signal strength.
- Palnagotchi gets a random mood every minute or so. Warning: may occasionally identify as a [Pork(chop)](https://github.com/0ct0sec/M5PORKCHOP) and `OINK OINK!` at nearby packets `(^ 00)`.

## Why?

I don't like to see a sad Pwnagotchi.

## Planned features

- Friend spam?
