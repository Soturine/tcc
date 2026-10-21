"""Infraestrutura reprodutivel de avaliacao offline do FallDetector do firmware.

Somente biblioteca padrao. O detector nunca e reimplementado em Python: as series
normalizadas sao entregues ao executavel `fall_replay`, compilado a partir de
`src/fall_detector.cpp` e `src/sensor_signal.cpp`.
"""

SCHEMA_VERSION = 1
