# Solwind plugins

The plugins of [Solwind](https://github.com/kerimozgurtukenmez/Solwind): what drives a laptop's fans or keyboard light. Solwind itself comes without any; you install the ones your laptop needs, and only those are downloaded.

```sh
solwind plugins                          # the plugins, and which fit this laptop
solwind plugin install HP/hp-wmi-keyboard
```

Solwind's catalog (`plugin-catalog.json` in the Solwind repository) lists every plugin with a pinned commit of this repository. `solwind plugin install` downloads only that plugin's folder at exactly that commit (git checks the commit id), builds it on your machine against the installed Solwind and puts it into `~/.local/share/solwind/plugins`. No sudo.

| Plugin | For | Tested |
|---|---|---|
| `NBFC/nbfc` | Fans through [NBFC-Linux](https://github.com/nbfc-linux/nbfc-linux) (needs NBFC with a config for your laptop) | HP Victus 16-r0xxx |
| `HP/hp-wmi-fan` | HP Victus / Omen fans without NBFC, through the hp-wmi driver's `pwm1` and `pwm2`. Works on the boards the driver opens both files on (the [hp-wmi-fan-and-backlight-control](https://github.com/TUXOV/hp-wmi-fan-and-backlight-control) module's list); `solwind fan-permission` gives the write permission, and NBFC must be stopped and its plugin turned off | Victus 16-r0xxx (8BBD), with the module patched for that board: the stock module doesn't list it |
| `MSI/msi-ec-keyboard` | MSI single-color keyboards, brightness only (`msiacpi::kbd_backlight`, needs the [msi-ec](https://github.com/BeardOverflow/msi-ec) module; not the official kernel's) | no |
| `OpenRGB/openrgb` | RGB keyboards the kernel doesn't expose, such as MSI's SteelSeries ones, through [OpenRGB](https://openrgb.org): one color and brightness. Needs OpenRGB installed; starts its server on 127.0.0.1 if it isn't running | no (tested against a fake server only) |
| `HP/hp-wmi-keyboard` | HP single-zone RGB keyboard light (`hp::kbd_backlight`, needs the [hp-wmi-fan-and-backlight-control](https://github.com/TUXOV/hp-wmi-fan-and-backlight-control) module), with effects | HP Victus 16-r0xxx |
| `HP/hp-wmi-keyboard-zones` | HP four-zone RGB keyboards (`hp::kbd_zoned_backlight-*`, same module) | no |
| `HP/hp-wmi-rgb-zones` | HP four-zone keyboards with older copies of that module (`rgb_zones`) | no |

## Writing one

A plugin is a folder `<Brand>/<name>/`, one per kernel module or driver, with a `CMakeLists.txt` that calls `solwind_plugin()`, a `Plugin.cpp` (the class solwindd loads) and `plugin.json.in` (its id, kind, description). It implements the interfaces of Solwind's SDK (`FanPlugin` or `KeyboardPlugin`, installed with Solwind under `<prefix>/include/solwind`). Plugins only read and write what a kernel driver or a fan service offers; the safety rules (the safety limit, checking that a speed took, handing the fans back) stay in Solwind.

Build one against an installed Solwind:

```sh
cmake -S HP/hp-wmi-keyboard -B build -DCMAKE_PREFIX_PATH=/usr/local && cmake --build build
solwind plugin install --source . HP/hp-wmi-keyboard     # installs a local copy (not checked against the catalog)
```

A plugin runs inside solwindd, as your user. Solwind only loads one built for its own SDK, from files nobody else can change.

## License

GPL-3.0 (see LICENSE), like Solwind.
