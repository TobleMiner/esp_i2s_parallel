The ESP32 i2s parallel driver
=============================

Inspired by [willz1200/i2s_parallel_queued](https://github.com/willz1200/i2s_parallel_queued)

This is an esp-idf component utilizing the powers of the i2s peripherals LCD mode and DMA for fast parallel data transfer.
Due to the inherent limitations of the LCD mode the transfer operation is transmit only.

Be warned, parallel I2S on the esp32 is weird.

# I2S0 vs I2S1

Off the bat there are some key differences between the two I2S peripherals in the ESP32:

## True 8 bit mode

I2S0 does not have a true 8 bit mode. Even if you do only need 8 bits of bus-width the
data samples provided in memory must still have a width of 16 bit. The upper 8 bits are
simply ignored.

ISS1 on the other hand has a true 8 bit mode. Samples must not be extended to 16 bit.

## Clock speed

This difference somewhat results from the differences in true 8 bit mode.

The maximum speed achievable depends on the width of the data read from memory.

For I2S1 it is peripheral_clk / 2 in 8 bit mode while it is peripheral_clk / 4 for I2S0 even in 8 bit mode.

In 16 bit mode it is peripheral_clk / 4 for both I2S0 and I2S1.
