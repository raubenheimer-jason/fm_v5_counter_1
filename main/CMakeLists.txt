idf_component_register(
    SRCS
        "app_main.c"
        "components/rtc/rtc.c"
        "components/fram/fram.c"
        "components/ntp/ntp.c"
        "components/wifi/wifi.c"
        "components/gpio/gpio.c"
        "components/time_init/time_init.c"
        "components/crypto/ecc.c"
        "components/crypto/ecdsa.c"
        "components/crypto/nn.c"
        "components/crypto/prng.c"
        "components/crypto/secp256r1.c"
        "components/crypto/sha256.c"
        "components/jwt/jwt.c"
        "components/mqtt/mqtt.c"
        "components/status/status.c"
        "upload_task.c"
        "fram_task.c"
    INCLUDE_DIRS
        "."
)