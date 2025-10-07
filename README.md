# Micro Features

Get speech features relevant for [microWakeWord](https://github.com/kahrendt/microWakeWord/) and [microVAD](https://github.com/rhasspy/pymicro-vad).

Install:

``` sh
pip install pymicro-features
```

Example:

``` python
from pymicro_features import MicroFrontend

frontend = MicroFrontend()

for i in range(10):
    audio = bytes(160 * 2)  # 10ms @ 16Khz (16-bit mono)
    output = frontend.process_samples(audio)
    print(output)
```

