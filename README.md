The ESP32 i2s parallel driver
=============================

Inspired by [willz1200/i2s_parallel_queued](https://github.com/willz1200/i2s_parallel_queued)

This is an esp-idf component utilizing the powers of the i2s peripherals LCD mode and DMA for fast parallel data transfer.
Due to the inherent limitations of the LCD mode the transfer operation is transmit only.
