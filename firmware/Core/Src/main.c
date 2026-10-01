/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpdma.h"
#include "gpio.h"
#include "icache.h"
#include "spi.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "ads131m03.h"
#include "ads131m03_port.h"
#include "calibration_store.h"
#include "smu_cal_seq.h"
#include "smu_calibration.h"
#include "smu_measurement.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */

volatile bool ads_ok = false;
ads131m03_bringup_result_t ads_result = {0};
static volatile bool ads_dma_enabled = false;
const ads131m03_dma_status_t* ads_dma_status = NULL;

/* Latest measurement/debug information */
volatile uint32_t foreground_frames = 0;

ads131m03_dma_frame_t adc_frame;
smu_measurement_outputs_t smu_outputs;

// -------------------------

smu_cal_seq_t cal_seq;
volatile bool test_start_vcal = false;

volatile bool test_vcal_capture_gnd = false;
volatile bool test_vcal_capture_1v5 = false;
volatile bool test_vcal_capture_3v0 = false;

volatile int vcal_active_point = -1;

/* Uncalibrated nominal VMEAS */
volatile float vcal_x[3] = {0.0f, 0.0f, 0.0f};

/* Calibrated CALBUS reference */
volatile float vcal_y[3] = {0.0f, 0.0f, 0.0f};

volatile bool vcal_point_valid[3] = {false, false, false};

volatile float vcal_fit_gain = 1.0f;
volatile float vcal_fit_offset = 0.0f;

volatile float vcal_residual[3] = {0.0f, 0.0f, 0.0f};

volatile bool vcal_fit_ready = false;
volatile bool vcal_test_fault = false;

volatile bool test_vcal_commit = false;
volatile bool vcal_commit_ok = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_6 && ads_dma_enabled) {
        ads131m03_dma_drdy_isr();
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi->Instance == SPI1) {
        ads131m03_dma_complete_isr();
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi) {
    if (hspi->Instance == SPI1) {
        ads131m03_dma_error_isr();
    }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_SPI1_Init();
    MX_ICACHE_Init();
    /* USER CODE BEGIN 2 */

    /*
     * ------------------------------------------------------------
     * 1. Load persistent calibration
     * ------------------------------------------------------------
     *
     * smu_cal_store_init() DOES NOT erase Flash.
     *
     * smu_calibration_init():
     *   - loads newest valid Flash calibration
     *   - or uses unity defaults if Flash is empty
     *   - applies calibration to smu_measurement
     */
    smu_cal_store_init();
    smu_calibration_init();
    smu_cal_seq_init(&cal_seq);

    /*
     * ------------------------------------------------------------
     * 2. ADS131M03 bring-up
     * ------------------------------------------------------------
     */
    ads_ok = ads131m03_bringup_run(&ads_result);

    /*
     * ------------------------------------------------------------
     * 3. Start ADS DMA acquisition
     * ------------------------------------------------------------
     */
    if (ads_ok) {
        ads131m03_dma_init();

        ads_dma_status = ads131m03_dma_get_status();

        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);
        ads_dma_enabled = true;
    } else {
        Error_Handler();
    }

    /* USER CODE END 2 */

    /* Initialize led */
    BSP_LED_Init(LED_GREEN);

    /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

    /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
    BspCOMInit.BaudRate = 115200;
    BspCOMInit.WordLength = COM_WORDLENGTH_8B;
    BspCOMInit.StopBits = COM_STOPBITS_1;
    BspCOMInit.Parity = COM_PARITY_NONE;
    BspCOMInit.HwFlowCtl = COM_HWCONTROL_NONE;
    if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE) {
        Error_Handler();
    }

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */

    while (1) {
        ads131m03_dma_frame_t f;

        /*
         * ========================================================
         * ADC acquisition
         * ========================================================
         */

        while (ads131m03_dma_pop(&f)) {
            ads131m03_frame_t mf = {0};

            /*
             * Optional debugger copy you already had.
             */
            adc_frame = f;
            foreground_frames++;

            /*
             * Adapter from DMA frame to measurement frame.
             */
            mf.ch[0] = f.ch0;
            mf.ch[1] = f.ch1;
            mf.ch[2] = f.ch2;
            mf.crc_ok = true;

            /*
             * Convert raw ADC counts -> calibrated physical values
             * and update filters.
             */
            if (smu_measurement_process_frame(&mf)) {
                smu_measurement_outputs_t out;

                smu_measurement_get_outputs(&out);

                /*
                 * Keep latest result globally visible to debugger
                 * and calibration code.
                 */
                smu_outputs = out;
            }

            /*
             * Feed this ADC sample to the calibration sequencer.
             *
             * Must run once per popped frame, inside this loop:
             * the outer loop spins far faster than the ADC output
             * rate, so sampling "f" out here would double-count
             * stale frames (or read uninitialized stack memory on
             * iterations where nothing was popped).
             */
            if ((cal_seq.state == CAL_SEQ_DISCARD) || (cal_seq.state == CAL_SEQ_ACQUIRE)) {
                if (cal_seq.target == CAL_TARGET_VOLTAGE) {
                    smu_cal_seq_adc_frame(&cal_seq, f.ch1, f.ch2);
                } else {
                    smu_cal_seq_adc_frame(&cal_seq, f.ch0, f.ch2);
                }
            }
        }

        /*
         * --------------------------------------------------------
         * Temporary debugger trigger
         * --------------------------------------------------------
         */
        if (test_start_vcal) {
            test_start_vcal = false;

            (void)smu_cal_seq_start(&cal_seq, CAL_TARGET_VOLTAGE, CALBUS_P1V5);
        }

        if (test_vcal_capture_gnd) {
            test_vcal_capture_gnd = false;

            vcal_active_point = 0;
            vcal_fit_ready = false;
            vcal_test_fault = false;

            if (!smu_cal_seq_start(&cal_seq, CAL_TARGET_VOLTAGE, CALBUS_0V)) {
                vcal_active_point = -1;
                vcal_test_fault = true;
            }
        }

        if (test_vcal_capture_1v5) {
            test_vcal_capture_1v5 = false;

            vcal_active_point = 1;
            vcal_fit_ready = false;
            vcal_test_fault = false;

            if (!smu_cal_seq_start(&cal_seq, CAL_TARGET_VOLTAGE, CALBUS_P1V5)) {
                vcal_active_point = -1;
                vcal_test_fault = true;
            }
        }

        if (test_vcal_capture_3v0) {
            test_vcal_capture_3v0 = false;

            vcal_active_point = 2;
            vcal_fit_ready = false;
            vcal_test_fault = false;

            if (!smu_cal_seq_start(&cal_seq, CAL_TARGET_VOLTAGE, CALBUS_P3V)) {
                vcal_active_point = -1;
                vcal_test_fault = true;
            }
        }

        if (cal_seq.result_ready) {
            cal_seq.result_ready = false;

            if ((vcal_active_point >= 0) && (vcal_active_point < 3)) {
                int p = vcal_active_point;

                /*
                 * CH1:
                 * Raw ADC average -> ADC volts ->
                 * nominal reconstructed SENSE voltage.
                 *
                 * Do NOT apply stored voltage calibration here.
                 */
                vcal_x[p] = smu_calibration_vcal_nominal_voltage(cal_seq.target_average);

                /*
                 * CH2:
                 * Use already-calibrated CALBUS as our reference.
                 */
                vcal_y[p] = smu_calibration_vcal_calbus_voltage((int32_t)cal_seq.calbus_average);

                vcal_point_valid[p] = true;

                vcal_active_point = -1;

                /*
                 * Automatically calculate the fit once all
                 * three manually acquired points exist.
                 */
                if (vcal_point_valid[0] && vcal_point_valid[1] && vcal_point_valid[2]) {
                    float gain = 0.0f, offset = 0.0f, residual[3] = {0.0f, 0.0f, 0.0f};

                    if (smu_calibration_vcal_fit(vcal_x, vcal_y, &gain, &offset, residual)) {
                        vcal_fit_gain = gain;
                        vcal_fit_offset = offset;
                        vcal_residual[0] = residual[0];
                        vcal_residual[1] = residual[1];
                        vcal_residual[2] = residual[2];
                        vcal_fit_ready = true;
                    } else {
                        vcal_test_fault = true;
                    }
                }
            }
        }

        if (test_vcal_commit) {
            test_vcal_commit = false;
            vcal_commit_ok = false;

            /*
             * Only allow commit after a successful complete
             * three-point calibration.
             */
            if (vcal_fit_ready && vcal_point_valid[0] && vcal_point_valid[1] && vcal_point_valid[2] && !vcal_test_fault && !cal_seq.fault) {
                vcal_commit_ok = smu_calibration_vforce_commit(vcal_fit_gain, vcal_fit_offset);
            }
        }

        /*
         * --------------------------------------------------------
         * 1 ms calibration sequencer tick
         * --------------------------------------------------------
         */
        static uint32_t cal_last_tick = 0;

        uint32_t now = HAL_GetTick();

        if (now != cal_last_tick) {
            uint32_t elapsed_ms = now - cal_last_tick;
            cal_last_tick = now;
            smu_cal_seq_tick_elapsed_ms(&cal_seq, elapsed_ms);
        }

        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_CSI;
    RCC_OscInitStruct.CSIState = RCC_CSI_ON;
    RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 122;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 10;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /** Configure the programming delay
     */
    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* USER CODE BEGIN 4 */

void ads131m03_port_log(const char* text) {
    (void)text;
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @param None
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
