# STM32H503CBU6 integration boundary
No package pins or Cube peripheral instances are frozen yet.
Recommended final ownership: dedicated ADS131M03 SPI+DMA with DRDY EXTI; separate AD5686 SPI if pin mux permits; 1 kHz scheduler timer; MCU ADC scan for five gate readbacks, rails and temperatures; EXTI COMPLIANCE_ACTIVE; five range GPIOs; PA request; external-watchdog heartbeat; CALBUS/relay GPIOs; LDAC/RESET; SWD and host UART.
Fail-safe rules: PA request inactive at reset, ranges OFF, CAL relays open. IWDG is serviced only by the top-level scheduler after critical health checks. Negative rails must be translated into the MCU ADC input range.
