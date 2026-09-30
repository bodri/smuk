# STM32H503RBTx LQFP64 pin contract
Frozen from supplied CubeMX screenshot (2026-09-27):
PA0 IR1MA
PA1 IR10MA
PA2 IR100MA
PC2 MV_ON
PC3 VRANGE
PA3 VCP_RX
PA4 VCP_TX
PA5 ILD2
PA6 SPI1_MISO
PC4 ICAL
PC5 SPI1_SCK
PB0 IR2A
PC6 NDRDY
PC7 SPI1_MOSI
PC9 NRESET
PA10 VCAL
PA13 SWDIO
PA14 SWCLK
PB3 SWO
PC12 IR100UA

PA8 remains RCC_MCO_1 as shown, but is not required by the SMU core.

Not frozen until schematic nets are confirmed: AD5686 bus/CS/LDAC, PA enable,
COMPLIANCE_ACTIVE, external watchdog heartbeat, five range-gate ADC readbacks,
+9 V readback, rail monitoring, and temperature channels.
