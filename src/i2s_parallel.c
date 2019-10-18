#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/periph_ctrl.h>
#include <rom/gpio.h>
#include <soc/gpio_sig_map.h>

#include <i2s_parallel.h>

static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};

static inline int get_bus_width(i2s_parallel_sample_width_t width) {
  switch(width) {
    case I2S_PARALLEL_WIDTH_8:
      return 8;
    case I2S_PARALLEL_WIDTH_16:
      return 16;
    case I2S_PARALLEL_WIDTH_24:
      return 24;
    default:
      return -1;
  }
}

static void iomux_set_signal(int gpio, int signal) {
  if(gpio < 0) {
    return;
  }
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, GPIO_MODE_DEF_OUTPUT);
  gpio_matrix_out(gpio, signal, false, false);
}

static void dma_reset(i2s_dev_t* dev) {
  dev->lc_conf.in_rst = 1;
  dev->lc_conf.in_rst = 0;
  dev->lc_conf.out_rst = 1;
  dev->lc_conf.out_rst = 0;
}

static void fifo_reset(i2s_dev_t* dev) {
  dev->conf.rx_fifo_reset = 1;
//  while(dev->state.rx_fifo_reset_back);
  dev->conf.rx_fifo_reset = 0;
  dev->conf.tx_fifo_reset = 1;
//  while(dev->state.tx_fifo_reset_back);
  dev->conf.tx_fifo_reset = 0;
}

static void dev_reset(i2s_dev_t* dev) {
  fifo_reset(dev);
  dma_reset(dev);
  dev->conf.rx_reset=1;
  dev->conf.tx_reset=1;
  dev->conf.rx_reset=0;
  dev->conf.tx_reset=0;
}

esp_err_t i2s_parallel_driver_install(i2s_port_t port, i2s_parallel_config_t* conf, intr_handler_t irq_hndlr, void* priv) {
  if(port < I2S_NUM_0 || port >= I2S_NUM_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  if(conf->sample_width < I2S_PARALLEL_WIDTH_8 || conf->sample_width >= I2S_PARALLEL_WIDTH_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  if(conf->sample_rate > I2S_PARALLEL_CLOCK_HZ || conf->sample_rate < 1) {
    return ESP_ERR_INVALID_ARG;
  }

  volatile int iomux_signal_base;
  volatile int iomux_clock;
  // Initialize I2S peripheral
  if (port == I2S_NUM_0) {
      periph_module_reset(PERIPH_I2S0_MODULE);
      periph_module_enable(PERIPH_I2S0_MODULE);
      iomux_clock = I2S0O_WS_OUT_IDX;

      iomux_signal_base = I2S0O_DATA_OUT0_IDX;
      if(conf->sample_width == I2S_PARALLEL_WIDTH_8) {
        iomux_signal_base += 8;
      }
      if(conf->sample_width == I2S_PARALLEL_WIDTH_16) {
        iomux_signal_base += 8;
      }
  } else {
      periph_module_reset(PERIPH_I2S1_MODULE);
      periph_module_enable(PERIPH_I2S1_MODULE);
      iomux_clock = I2S1O_WS_OUT_IDX;

      iomux_signal_base = I2S1O_DATA_OUT0_IDX;
      if(conf->sample_width == I2S_PARALLEL_WIDTH_16) {
        iomux_signal_base = I2S1O_DATA_OUT8_IDX;
      }
  }

  // Setup GPIOs
  int bus_width = get_bus_width(conf->sample_width);
  for(int i = 0; i < bus_width; i++) {
    iomux_set_signal(conf->gpios_bus[i], iomux_signal_base + i);
  }
  iomux_set_signal(conf->gpio_clk, iomux_clock);

  // Setup I2S peripheral
  i2s_dev_t* dev = I2S[port];
  dev_reset(dev);

  // Set i2s mode to LCD mode
  dev->conf2.val = 0;
  dev->conf2.lcd_en = 1;

  // Setup i2s clock
  dev->sample_rate_conf.val = 0;
  dev->sample_rate_conf.rx_bits_mod = bus_width;
  dev->sample_rate_conf.tx_bits_mod = bus_width;
  dev->sample_rate_conf.rx_bck_div_num = 1;
  dev->sample_rate_conf.tx_bck_div_num = 1;

  // No support for fractional dividers (could probably be ported from official serial i2s driver though)
  dev->clkm_conf.val = 0;
  dev->clkm_conf.clka_en = 0;
  dev->clkm_conf.clkm_div_a = 0;
  dev->clkm_conf.clkm_div_b = 0;
  dev->clkm_conf.clkm_div_num = I2S_PARALLEL_CLOCK_HZ / conf->sample_rate;

  // Some fifo conf I don't quite understand 
  dev->fifo_conf.val = 0;
  // Dictated by datasheet
  dev->fifo_conf.rx_fifo_mod_force_en = 1;
  dev->fifo_conf.tx_fifo_mod_force_en = 1;
  // Not really described for non-pcm modes, although datasheet states it should be set correctly even for LCD mode
  dev->fifo_conf.tx_fifo_mod = 1;
  dev->fifo_conf.tx_fifo_mod = 1;
  // Pobably relevant for buffering from the DMA controller
  dev->fifo_conf.rx_data_num = 32; //Thresholds. 
  dev->fifo_conf.tx_data_num = 32;
  // Enable DMA support
  dev->fifo_conf.dscr_en = 1;

  dev->conf1.val = 0;
  dev->conf1.tx_stop_en = 0;
  dev->conf1.tx_pcm_bypass = 1;
  
  dev->conf_chan.val = 0;
  dev->conf_chan.tx_chan_mod = 1;
  dev->conf_chan.rx_chan_mod = 1;
  
  //Invert ws to be active-low... ToDo: make this configurable
  dev->conf.tx_right_first = 1;
  dev->conf.rx_right_first = 1;
  
  dev->timing.val = 0;

  if(irq_hndlr) {
    esp_intr_alloc(ETS_I2S0_INTR_SOURCE + port, 0, irq_hndlr, priv, NULL);
  }

  return ESP_OK;
}

esp_err_t i2c_parallel_send_dma(i2s_port_t port, lldesc_t* dma_descriptor) {
  if(port < I2S_NUM_0 || port >= I2S_NUM_MAX) {
    return ESP_ERR_INVALID_ARG;
  }

  i2s_dev_t* dev = I2S[port];
  // Stop all ongoing DMA operations
  dev->out_link.stop = 1;
  dev->out_link.start = 0;
  dev->conf.tx_start = 0;
  dev_reset(dev);

  // Configure DMA burst mode
  dev->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
  // Set address of DMA descriptor
  dev->out_link.addr = dma_descriptor;
  // Start DMA operation
  dev->out_link.start = 1;
  dev->conf.tx_start = 1;

  return ESP_OK;
}
