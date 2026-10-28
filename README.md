# Minute Thé

A tea brewing timer for Pebble Time 2, written in C for the Emery platform
(200 × 228 display).

Based on [TeaTime by Steve Atkins](https://github.com/wttw/pebble-teatime).

## Features

- Seven built-in tea profiles that can be enabled individually.
- Customizable tea names, preparation notes, banner colors and brewing times.
- First alert and brewing end time adjustable from 30 seconds to 15 minutes,
  in 30-second increments.
- Clock visible throughout the brewing session.
- Progress bar and a glass cup illustration with animated steam.
- Liquid color changes based on brewing progress and the selected color family.
- One vibration at the first alert and two at the end.
- Optional sound alerts: one beep at the first alert and two at the end.
- Set both times to the same value to receive only the final alert.
- Dark and light themes.
- Clay configuration with saved preferences.

The liquid level stays fixed; only its color changes. Depending on the selected
alert times, some color stages may be skipped.

Sound alerts respect the watch's mute setting.
Keep the app open during brewing to receive alerts.

## Languages

Configuration and built-in tea profiles are available in:

- English
- French
- German
- Spanish
- Italian
- Portuguese

Custom tea names and preparation notes remain as saved and are not
automatically translated.

## Compatibility

This version targets Pebble Time 2 (`emery`) only.

## Building

With the Pebble SDK installed, run from the project root:

```sh
pebble build
```

Install in the emulator:

```sh
pebble install --emulator emery
```

Install on a watch paired with a phone, replacing `<phone-IP>` with the phone's
IP address:

```sh
pebble install --phone <phone-IP>
```

## Project structure

- `src/c/minute_the.c`: native application and user interface.
- `src/pkjs/`: Clay configuration, translations and phone-side communication.
- `resources/images/tea.png`: custom cup icon created for Minute Thé.
- `package.json`: application metadata, resources and configuration keys.

## License and credits

Original code: Copyright (c) 2014 Steve Atkins.
Minute Thé adaptation: Copyright (c) 2026 WayeM.

Distributed under the MIT License. See [LICENSE.md](LICENSE.md) and
[CREDITS.md](CREDITS.md).
