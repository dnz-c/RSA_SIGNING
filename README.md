# RSA_SIGNING

Bare-metal RSA-2048 key generation on an STM32 NUCLEO-F401RE, using mbedTLS with a custom ADC-noise entropy source. Learning project, not production code.

## Hardware / Toolchain

- Board: NUCLEO-F401RE (STM32F401RET6, Cortex-M4 @ 84 MHz, 96 KB RAM, 512 KB flash)
- IDE: STM32CubeIDE + STM32CubeMX (HAL)
- Library: mbedTLS 3.6.5 (vendored under `Middleware/mbedTLS`)

## What it does

1. Initializes USART2 (115200 8N1), routed through the ST-LINK virtual COM port, with `printf` retargeted to it.
2. Registers a custom entropy source with mbedTLS (`adc_entropy_source`): the LSB of 12-bit ADC1 samples on channel 0 (PA0 / Arduino A0), with the pin left unconnected.
3. Seeds CTR_DRBG from that entropy pool.
4. Generates an RSA-2048 keypair via `mbedtls_rsa_gen_key()`, public exponent 65537.
5. Prints the result over UART.

## Build and run

1. Open the project in STM32CubeIDE.
2. Select the **Release** build configuration (`-O2`). Debug (`-O0`) works but key generation is far slower.
3. Build, then flash with Run or Debug.
4. Open a serial terminal on the ST-LINK COM port at 115200 baud.

Key generation runs in software with no hardware acceleration and takes a long time, so expect a wait before the success message appears.

## mbedTLS configuration notes

Settings in `mbedtls_config.h` that matter on this target:

- `MBEDTLS_NO_PLATFORM_ENTROPY` is defined. The F401 has no hardware RNG and no OS entropy source.
- `MBEDTLS_HAVE_TIME` and `MBEDTLS_FS_IO` are disabled (no RTC clock source or filesystem).
- `MBEDTLS_NET_C` is disabled (no sockets).
- `MBEDTLS_AES_C` stays enabled because CTR_DRBG depends on it.
- `MBEDTLS_HAVE_ASM` is only safe with optimization enabled. At `-O0` the bignum inline assembly fails with register-pressure errors.
- mbedTLS 3.6 rejects RSA key generation below 1024 bits (`-0x4080`).

The config is not yet trimmed to the minimum. The image still contains unused modules (TLS, PSA, ECC, extra ciphers).
