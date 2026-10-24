# ES8311 driver

`es8311.cpp` is from Waveshare's official ESP32-S3-Touch-LCD-1.85B repository at revision `139e6db584f3737fcfc6a958ee83b79fb69d317c`, `Examples/Arduino-V3.2.0/examples/03_audio_out_no_tf/es8311.cpp`.

Copyright 2015–2022 Espressif Systems (Shanghai) CO LTD. Licensed under Apache-2.0: https://www.apache.org/licenses/LICENSE-2.0

Source: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/blob/139e6db584f3737fcfc6a958ee83b79fb69d317c/Examples/Arduino-V3.2.0/examples/03_audio_out_no_tf/es8311.cpp

Local changes check allocation failure and incomplete I2C reads. The accompanying small headers declare the API subset and register addresses used by this implementation. The device uses the existing shared Wire bus and the board's GPIO9 amplifier control. The Apache license is included in [LICENSE](LICENSE).
