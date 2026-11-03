# Example: OTA over HTTPS (Y70XX)

This example demonstrates a firmware OTA (over-the-air) update flow over HTTPS, using a **SIMCom Y70XX** module (`Salvus-SIMCom`) as the data connection. The device periodically polls a JSON manifest for a newer firmware release, downloads it in chunks into the inactive OTA partition (resuming from where it left off if the connection drops), verifies its SHA-256 hash against the one declared in the manifest, and only then switches the boot partition and restarts.

As in the other Salvus examples, the code is organized in layers with their own `init`/`start` lifecycle: the layer that drives the OTA state machine (`ota`) only ever talks to the generic `simcom_application_*`/`salvus_simcom_*` functions, never to Y70XX-specific details, which live in `module/simcom/y70xx`.

## Example architecture

The code in `main/application/` is organized as follows, orchestrated by `application_init()`/`application_start()` ([main/application/application.c](main/application/application.c)):

```
application
├── logging                    Initializes logging (Salvus-Logging)
│   └── esp_log                  ESP implementation
├── peripheral                 Initializes the peripherals used by the example
│   ├── gpio                     Abstraction of the pins used by the communication module
│   │   └── esp32s3_gpio           ESP32-S3 implementation (Salvus-HAL-GPIO)
│   ├── usart                    Abstraction of the USART used by the communication module
│   │   └── esp32s3_usart          ESP32-S3 implementation (Salvus-HAL-USART)
│   └── memory                   Abstraction of the non-volatile memory holding OTA credentials
│       └── esp32s3_nvm            ESP32-S3 implementation (Salvus-HAL-NVM, ESP NVS backend)
├── memory
│   └── nvm
│       └── ota                  Reads the OTA server hostname and TLS credentials from NVM
├── module                     Initializes and turns on the selected communication module
│   └── simcom                   Facade over the SIMCom module used for connectivity
│       └── y70xx                  Y70XX implementation (Salvus-SIMCom) — cellular network,
│                                     module power/responsiveness, no implementation details leak out
└── ota                         OTA state machine: manifest polling, download, hash verification
    └── sha256                    SHA-256 hashing of the downloaded firmware (PSA Crypto API)
```

Only `module/simcom/y70xx` knows Y70XX-specific details (pins, baudrate recovery, PDP/APN, PSM, IMEI); `ota_application.c` only ever calls the generic `simcom_application_*` functions and the HTTP client exposed by `Salvus-SIMCom` (`salvus_simcom_*_http_*`). Supporting a new communication module would mean adding a new folder under `module/simcom/` (analogous to `y70xx/`) and a matching option in [main/Kconfig.projbuild](main/Kconfig.projbuild)/[main/CMakeLists.txt](main/CMakeLists.txt), without touching `ota/`.

## What the example does

### Initialization (`application_init`)

1. **Logging**: initializes `Salvus-Logging` with the ESP implementation.
2. **Peripherals** (`peripheral_application_init`): initializes, in order, the NVM unit (`memory`, ESP NVS on the `nvs` partition), the GPIO pins and the USART used by the communication module.
3. **Module** (`module_application_init` → `simcom_application_init` → `y70xx_application_init`): creates the Y70XX instance bound to the configured pins/USART.
4. **OTA** (`ota_application_init`): creates the event queue, the periodic manifest-check timer and the OTA task, but doesn't start anything yet.

### Start (`application_start`)

1. **Peripherals**: no-op (all configuration already happened in `init`).
2. **Module** (`y70xx_application_start`): turns the module on, confirms it responds to `AT` (retrying a baudrate reconfiguration if it doesn't), disables Power Saving Mode, queries/logs the IMEI, and turns the module back off.
3. **OTA**: fires `START_APPLICATION_APPLICATION_EVT`, which the OTA task uses to trigger the first manifest check immediately.

### OTA state machine (`main/application/ota/ota_application.c`)

The state machine (`last_firmware_downloading_state_type_t`) has two states:

- **Waiting for a new release**: on `VERIFY_LAST_FIRMWARE_RELEASE_APPLICATION_EVT` (fired at startup and then every `LAST_FIRMWARE_RELEASE_VERIFICATION_PERIOD_MS` = 60 s), `_query_last_firmware_release_data` turns the module on, connects to the network, initializes HTTPS and downloads `/fw/poc/manifest.json`. Its `version` is compared against the running firmware's (`esp_app_get_description()->version`, i.e. `PROJECT_VER` in [project/CMakeLists.txt](project/CMakeLists.txt)). If the manifest's version is newer, the state machine moves to downloading and fires `DOWNLOAD_LAST_FIRMWARE_RELEASE_APPLICATION_EVT` immediately; otherwise the same check is rescheduled.
- **Downloading firmware**: `_start_ota` begins an ESP-IDF OTA write (`esp_ota_begin`) to the inactive partition (`esp_ota_get_next_update_partition`). `_download_last_firmware_relase` then requests the file named by the manifest's `path` in `FIRMWARE_CHUNCK_SIZE` (4 KiB) chunks, using HTTP `Range` requests, writing each chunk to the OTA partition (`esp_ota_write`) as it arrives and reconnecting the module/network/HTTP session between chunks whenever needed (power, network or HTTP session lost). A failed download is retried up to `MAX_CONTINUOS_FIRMWARE_DOWNLOADING_ATTEMPTS` (3) times before the state machine backs off for `FIRMWARE_DOWNLOAD_RETRY_PERIOD_MS` (60 s) and tries again from the last successful `offset`. Once the whole image has been written, `_verify_last_firmware_hash` recomputes its SHA-256 from the OTA partition and checks it against the manifest's `sha256` field (see [Firmware integrity verification](#firmware-integrity-verification)): on success, the new partition is set as the boot partition and the device restarts (`esp_restart`); on failure, the OTA write is cancelled and the device keeps running the current firmware.

### Manifest format (`manifest.json`)

```json
{
  "version": "1.2.3",
  "size": 303648,
  "sha256": "ac97c3f1b6e9...e4d2",
  "path": "/fw/poc/firmware.bin"
}
```

`version` must parse as `major.minor.patch` (any non-digit separator works, e.g. `.`), `size` is the firmware size in bytes, `sha256` is the lowercase hex SHA-256 of the firmware binary, and `path` is the URL requested (in chunks) to download it.

## OTA credentials (NVM)

Unlike the broker credentials in the `Salvus-SIMCom` `communication_module` example (hardcoded, edited in the source before building), this example reads the HTTPS server hostname and TLS credentials from non-volatile memory (ESP NVS, through `Salvus-HAL-NVM`) on every `_init_http()` call, via [nvm_ota.c](main/application/memory/nvm/ota/nvm_ota.c):

| Key | NVS namespace | Content |
| --- | --- | --- |
| `ota_hostname`   | `storage` | OTA/manifest server hostname (string) |
| `ota_server_crt` | `storage` | Server CA certificate, PEM (string) |
| `ota_client_crt` | `storage` | Client certificate, PEM (string) — only needed if the server requires mTLS |
| `ota_client_key` | `storage` | Client private key, PEM (string) — only needed if the server requires mTLS |

Nothing in the example writes these keys — they must be provisioned on the `nvs` partition before running it, e.g. with the ESP-IDF NVS Partition Generator:

1. Create an `nvs.csv` with one row per key above (`namespace,key,type,encoding,value`), using `type=data` and `encoding=string`.
2. Generate the binary (size must match the `nvs` partition in [partitions.csv](partitions.csv)):
   ```bash
   python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs.csv nvs.bin 0x24000
   ```
3. Flash it to the `nvs` partition:
   ```bash
   esptool.py -p <SERIAL_PORT> write_flash 0x9000 nvs.bin
   ```

## Firmware integrity verification

[sha256_application.c](main/application/ota/sha256/sha256_application.c) wraps the **PSA Crypto API** (`psa_hash_setup`/`psa_hash_update`/`psa_hash_verify`) rather than the legacy `mbedtls/sha256.h` header, which ESP-IDF v6.0.2's bundled mbedTLS 4.x no longer exposes publicly. `sha256_application_verify()` uses `psa_hash_verify()` — a constant-time comparison against the expected hash, as recommended by the PSA Crypto API over computing the hash and comparing it with `memcmp` — and always aborts the hash operation afterwards (`psa_hash_abort`), so that a later OTA attempt can reuse it even if a previous one failed partway through.

## Hardware and pinout (Y70XX)

The example has the microcontroller fixed to **ESP32-S3** (`SALVUS_ESP32S3_MICROCONTROLLER_SELECTED`, set in [project/CMakeLists.txt](project/CMakeLists.txt)) and uses USART `1` for AT commands, plus three GPIO pins:

| Signal        | Pin    | Direction |
| ------------- | ------ | --------- |
| POWER_ENABLE  | GPIO40 | ESP32 output (turns the Y70XX's power supply on/off) |
| STATUS        | GPIO1  | ESP32 input (Y70XX indicates whether it is powered on) |
| RESET/WAKE    | GPIO2  | ESP32 output (used to wake the module from PSM) |
| USART TX      | GPIO42 | — |
| USART RX      | GPIO41 | — |

Serial configuration: `115200 8N1` in normal operation (the example briefly switches to `9600` only while attempting a baudrate reconfiguration, if the module doesn't respond at boot), no hardware flow control. These values are defined in [peripheral_application.h](main/application/peripheral/peripheral_application.h).

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) v6.0.2.
- A board with **ESP32-S3** (the target is fixed via [project/CMakeLists.txt](project/CMakeLists.txt)).
- A SIMCom Y70XX module with an active SIM card, wired as described in [Hardware and pinout](#hardware-and-pinout-y70xx).
- An HTTPS server reachable through the module's data network, serving `manifest.json` and the firmware binary named by its `path`, with support for `Range` requests.
- Access to the `Salvus-Error`, `Salvus-Logging`, `Salvus-HAL-NVM` and `Salvus-SIMCom` repositories/paths (see [main/idf_component.yml](main/idf_component.yml)).
- The `ota_hostname`, `ota_server_crt`, `ota_client_crt` and `ota_client_key` keys provisioned on the `nvs` partition (see [OTA credentials](#ota-credentials-nvm)).

## Building and running

```bash
idf.py build
idf.py -p <SERIAL_PORT> flash monitor
```

To exit the serial monitor, use `Ctrl+]`.

## Expected output

```
I (xxx) OTA_APPLICATION: Actual firmware:
I (xxx)    version: 1.0.0
I (xxx)    compiled in Oct 30 2026-12:00:00
I (xxx) Y70XX_APPLICATION: Communication module IMEI: 123456789012345
I (xxx) OTA_APPLICATION: Requesting manifest.json file...
I (xxx) OTA_APPLICATION: Status code: 200
I (xxx) OTA_APPLICATION: Content length: 95
I (xxx) OTA_APPLICATION: Last firmware:
I (xxx)   version: 1.1.0
I (xxx)   size: 303648
I (xxx)   download path: /fw/poc/firmware.bin
I (xxx) OTA_APPLICATION: Starting new firmware download
I (xxx) OTA_APPLICATION: Downloading progress: 1.350000% (4096/303648 total)
...
I (xxx) OTA_APPLICATION: Finished the downloading of the new firmware
I (xxx) OTA_APPLICATION: Verifying new firmware hash...
I (xxx) OTA_APPLICATION: New firmware hash verified successfully
I (xxx) OTA_APPLICATION: New firmware is valid, finishing OTA update
```

The device then restarts into the new firmware. If no newer release is available, only the "Actual firmware"/manifest-check lines repeat, every `LAST_FIRMWARE_RELEASE_VERIFICATION_PERIOD_MS` (60 s).

## Relevant settings

The [sdkconfig.defaults](sdkconfig.defaults) file enables the required implementations:

```
CONFIG_INCLUDE_ESP_LOG=y
CONFIG_INCLUDE_ESP_GPIO=y
CONFIG_INCLUDE_ESP_USART=y
CONFIG_INCLUDE_ESP_NVS=y
CONFIG_INCLUDE_Y70XX=y
```

[main/Kconfig.projbuild](main/Kconfig.projbuild) adds, at the root level of `menuconfig`, the option `OTA Test Example → Application → Module → SIMCom Module → Select communication module` — today with a single choice (`Y70XX`), the same mechanism the `Salvus-SIMCom` `communication_module` example uses for the module selection.

[partitions.csv](partitions.csv) defines a dual-OTA partition table (`ota_0`/`ota_1`, 0x180000 bytes each) plus `otadata` (active-slot bookkeeping) and the `nvs` partition (0x24000 bytes) used to store the OTA credentials.

Other relevant constants, defined as macros at the top of each file:

- [peripheral_application.h](main/application/peripheral/peripheral_application.h): pinout and USART configuration.
- [y70xx_application.c](main/application/module/simcom/y70xx/y70xx_application.c): `ACCESS_POINT_NAME` (APN) and the timeouts for turning the module on/off, network registration/attachment and PSM.
- [ota_application.c](main/application/ota/ota_application.c): manifest-check period, HTTP timeouts, firmware chunk size (`FIRMWARE_CHUNCK_SIZE`) and download retry policy.

## Project structure

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt                               Selects source directories by Kconfig/target
│   ├── idf_component.yml                            Component dependencies (Salvus-SIMCom, Salvus-HAL-NVM, ...)
│   ├── Kconfig.projbuild                            Communication module selection option
│   ├── main.c                                       app_main: application_init() + application_start()
│   └── application
│       ├── application.{c,h}                        Orchestrates the init/start of the layers below
│       ├── logging/                                  Salvus-Logging initialization (+ esp_log/ impl.)
│       ├── peripheral/                                GPIO, USART and memory (NVM) initialization (+ esp32s3_*/ impl.)
│       ├── memory/
│       │   └── nvm/
│       │       └── ota/                                 Reads OTA hostname/certificates/key from NVM
│       ├── module/
│       │   └── simcom/                                Facade over the SIMCom module
│       │       └── y70xx/                                Y70XX implementation
│       └── ota/                                        OTA state machine (manifest, download, boot switch)
│           └── sha256/                                   SHA-256 hashing of the downloaded firmware
├── project
│   └── CMakeLists.txt                               Project name, version (PROJECT_VER) and target (ESP32-S3)
├── partitions.csv                                   Dual-OTA partition table + nvs partition
├── sdkconfig.defaults                               Default ESP-IDF/Log/GPIO/USART/NVS/Y70XX settings
└── README.md
```
