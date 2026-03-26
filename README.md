# Palnagotchi for M5Stack

A friendly unit for those lonely Pwnagotchis out there. It's written to run on the M5Stack, see supported devices below.

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
  - Cardputer / Cardputer ADV: ESC or m toggles the menu. Use arrow keys or tab to navigate and OK to select option. Esc or m to go back to main menu.
  - StickC Plus2: Long press M5 button to toggle menu. Use top and bottom keys to navigate and M5 button (short press) to select option.
  - AtomS3(R): Long press display to toggle menu. Use double/triple tap display to navigate and short press display to select option.
  - Dial: Long press M5 button to toggle menu. Use rotary encoder to navigate. Short press M5 button to select.
  - DinMeter: Long press rotary encoder to toggle menu. Use rotary encoder to navigate. Short press rotary encoder button to select.
- Top bar shows UPS level and uptime.
- Bottom bar shows total friends made in this run, all time total friends between parenthesis (needs EEPROM) and signal strengh indicator of closest friend.
- Nearby pwnagotchis show all nearby units and its signal strength.
- Palnagotchi gets a random mood every minute or so.

## Why?

I don't like to see a sad Pwnagotchi.

## Planned features

- Friend spam?
