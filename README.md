# Coffee Maker Matter Plug-in Unit

This example exposes a single Matter On-Off Plug-in Unit endpoint for a coffee maker that is controlled through two GPIO lines:

- **GPIO IN** (`CONFIG_COFFEE_MAKER_GPIO_IN`, default GPIO2): digital status input. HIGH means the coffee maker is physically on, LOW means physically off. This pin is the **source of truth** for the Matter `OnOff` attribute — the attribute always tracks this pin, including external changes made at the device itself.
- **GPIO OUT** (`CONFIG_COFFEE_MAKER_GPIO_OUT`, default GPIO4): digital toggle output. A HIGH pulse of `CONFIG_COFFEE_MAKER_TOGGLE_PULSE_MS` (default 200ms) toggles the coffee maker's on/off state; there is no "set to X" line, so a pulse is only fired when the commanded state actually differs from the live state of GPIO IN.

If the coffee maker doesn't respond to a toggle pulse (e.g. a stuck relay), the attribute is force-corrected back to the GPIO IN reading after `CONFIG_COFFEE_MAKER_VERIFY_DELAY_MS` (default 1000ms) — GPIO IN always wins.

## Coffee Maker Configuration

To update the GPIO pins or timing, follow these steps:

1. Open a terminal.
1. Run the following command to access the configuration menu:
`idf.py menuconfig`
1. Navigate to the "Coffee Maker" menu.
1. Update `COFFEE_MAKER_GPIO_IN`, `COFFEE_MAKER_GPIO_OUT`, the pulse/verify timing, and the factory reset button GPIO/level (**use only available GPIO pins for the target chip**).

See the [docs](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html) for more information about building and flashing the firmware.

## 1. Additional Environment Setup

No additional setup is required.

## 2. Post Commissioning Setup

No additional setup is required.

## 3. Device Performance

### 3.1 Memory usage

The following is the Memory and Flash Usage.

-   `Bootup` == Device just finished booting up. Device is not
    commissionined or connected to wifi yet.
-   `After Commissioning` == Device is connected to wifi and is also
    commissioned and is rebooted.
-   device used: esp32c3_devkit_m
-   tested on:
    [6a244a7](https://github.com/espressif/esp-matter/commit/6a244a7b1e5c70b0aa1bf57254f19718b0755d95)
    (2022-06-16)

|                         | Bootup | After Commissioning |
|:-                       |:-:     |:-:                  |
|**Free Internal Memory** | 212KB   |127KB                |

**Flash Usage**: Firmware binary size: 1.40MB

This should give you a good idea about the amount of free memory that is
available for you to run your application's code.

Applications that do not require BLE post commissioning, can disable it using app_ble_disable() once commissioning is complete.
