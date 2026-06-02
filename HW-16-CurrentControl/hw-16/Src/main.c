/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    FDCAN/FDCAN_Com_Polling/Src/main.c
  * @author  MCD Application Team
  * @brief   This sample code shows how to achieve Polling Process Communication
  *          between two FDCAN units.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TX_ID          (0x111)   /* TX CAN message identifier    */
#define RX_ID          (0x111)   /* RX CAN message identifier    */

/* INA219 I2C address.
   The INA219 default 7-bit address is 0x40.
   In STM32 HAL, we send the address as INA219_ADDR << 1 later. */
#define INA219_ADDR              0x40

/* INA219 register addresses from the INA219 datasheet.
   These tell the sensor which internal register we want to write/read. */
#define INA219_REG_CONFIG        0x00  // Configuration register: sets voltage/current measurement mode
#define INA219_REG_SHUNTVOLTAGE  0x01  // Shunt voltage register
#define INA219_REG_BUSVOLTAGE    0x02  // Bus voltage register
#define INA219_REG_POWER         0x03  // Power register
#define INA219_REG_CURRENT       0x04  // Current register: this is what we read for motor current
#define INA219_REG_CALIBRATION   0x05  // Calibration register: sets current scaling


#define ADC_MIN_SAFE 0
// If ADC position is below this, stop the motor.

#define ADC_MAX_SAFE 4095
// 12-bit ADC max is 4095.
// set 4000 so saturation doesnt happen
// If ADC position is above this, stop the motor.

#define PWM_MAX 2400
// Your TIM1 period is 2400, so compare = 2400 means 100% high.

#define PWM_HALF 1200
// Half of 2400, used for 50% speed test.


#define NSAMPLES 400
// 400 samples total. At 1 kHz, this is 400 ms.

#define CURRENT_FLIP_TIME 100
// Flip desired current every 100 samples.
// 100 samples at 1 kHz = 100 ms.

#define DESIRED_CURRENT_RAW 150
// Desired current in raw INA219 units.
// 150 raw is about 50 mA because raw / 3 ≈ mA.
// Start low for safety.

#define KP_CURRENT .05f
// Proportional gain for current controller.

#define KI_CURRENT 1.0f



/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define COUNTOF(BUFFER) (sizeof((BUFFER)) / sizeof(*(BUFFER)))
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
ADC_HandleTypeDef hadc1;

FDCAN_HandleTypeDef hfdcan1;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
FDCAN_RxHeaderTypeDef rxHeader;
FDCAN_TxHeaderTypeDef txHeader;
uint8_t rxData[16U];
static const uint8_t txData[] = {0x10, 0x32, 0x54, 0x76, 0x98, 0x00, 0x11, 0x22,
                                 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00
                                };

volatile uint8_t state = 0;
// state = 0 means do nothing
// state = 1 means the user pressed 'a', so the interrupt should read ADC/current once

volatile uint32_t latest_position = 0;
// Stores the most recent ADC position value

volatile signed short latest_current_raw = 0;
// Stores the most recent raw INA219 current value


volatile uint8_t motor_killed = 0;
// 0 = motor is allowed
// 1 = safety stopped the motor


volatile signed short measured_current_array[NSAMPLES];
// Actual INA219 current readings.

volatile signed short desired_current_array[NSAMPLES];
// Desired current at each sample.

volatile int samples_collected = 0;
// How many samples were actually collected.

volatile uint32_t position_array[NSAMPLES];
// Stores potentiometer/ADC position during current-control test


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static uint32_t BufferCmp8b(const uint8_t *pBuffer1, const uint8_t *pBuffer2, uint16_t BufferLength);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t read_servo_adc(void)
{
    uint32_t raw = 0;

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        raw = HAL_ADC_GetValue(&hadc1);
    }
    else
    {
        raw = 0;
    }

    HAL_ADC_Stop(&hadc1);

    return raw;
}

/* This handle is created by the BSP Virtual COM Port code.
   We use it because your project does not show a normal huart2 or huart3 in main.c. */
extern UART_HandleTypeDef hcom_uart[];


/* This function writes a 16-bit value to one INA219 register. */
void writeINA219(int reg, int value)
{
    uint8_t buf[3];

    buf[0] = reg;
    // First byte: which INA219 register we want to write to.

    buf[1] = value >> 8;
    // Second byte: upper 8 bits of the 16-bit value.
    // Example: if value = 0x308F, this sends 0x30 first.

    buf[2] = value & 0xFF;
    // Third byte: lower 8 bits of the 16-bit value.
    // Example: if value = 0x308F, this sends 0x8F second.

    HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDR << 1, buf, 3, 10);
    // &hi2c2: use I2C2, which is connected to the INA219.
    // INA219_ADDR << 1: STM32 HAL expects the 7-bit address shifted left by 1.
    // buf: the data we are sending.
    // 3: we send 3 bytes: register, high byte, low byte.
    // 10: timeout in milliseconds.
}


/* This function reads a 16-bit signed value from one INA219 register. */
signed short readINA219(unsigned char reg)
{
    uint8_t buffer[2];

    HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDR << 1, &reg, 1, 10);
    // First, tell the INA219 which register we want to read from.
    // &reg: address of the register variable.
    // 1: send only 1 byte, the register address.
    // 10: timeout in milliseconds.

    HAL_I2C_Master_Receive(&hi2c2, INA219_ADDR << 1, buffer, 2, 10);
    // Then read 2 bytes back from that register.
    // buffer[0] will be the high byte.
    // buffer[1] will be the low byte.
    // 2: receive 2 bytes.
    // 10: timeout in milliseconds.

    signed short value = (buffer[0] << 8) | buffer[1];
    // Combine the two 8-bit bytes into one 16-bit number.
    // buffer[0] << 8 moves the high byte into the upper half.
    // | buffer[1] adds the low byte.

    return value/3;
}


/* This function configures the INA219 current sensor once at startup. */
void init_ina219(void)
{
    unsigned short ina219_calValue = 1024;
    // Calibration value.
    // This sets the scaling for current measurements.
    // The assignment gives 1024, so we use that.

    unsigned short ina219_config = 0x308F;
    // Same as binary 0b0011000010001111 from the assignment.
    // This configures the INA219 measurement settings:
    // - bus voltage range
    // - shunt voltage range
    // - ADC resolution/sample time
    // - continuous shunt and bus measurement mode

    writeINA219(INA219_REG_CALIBRATION, ina219_calValue);
    // Write the calibration value to register 0x05.

    writeINA219(INA219_REG_CONFIG, ina219_config);
    // Write the configuration value to register 0x00.
}


/* This function reads current as a float in milliamps.
   We may not use it for printing because float printing can be annoying on STM32. */
float read_ina219(void)
{
    float ma = 0;

    signed short value = readINA219(INA219_REG_CURRENT);
    // Read the current register, register 0x04.

    ma = value / 3.0;
    // According to the assignment, the raw value is in thirds of a milliamp.
    // So dividing by 3 gives current in mA.

    return ma;
}


/* This function prints position and current to the serial monitor. */
void print_position_and_current(uint32_t position, signed short current_raw)
{
    char msg[80];
    // Character array to store the message before sending it.
    // 80 characters is enough for this short line.

    snprintf(msg, sizeof(msg), "Position: %lu, Current raw: %d\r\n", position, current_raw);
    // Create a string like:
    // Position: 2000, Current raw: 12
    // %lu is for unsigned long / uint32_t.
    // %d is for signed integer.
    // \r\n makes a new line in the serial monitor.

    HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
    // Send the message through the Virtual COM Port.
    // hcom_uart[COM1] is the UART used by BSP COM1.
    // strlen(msg) sends only the actual message length.
    // HAL_MAX_DELAY means wait until the message is sent.
}


extern UART_HandleTypeDef hcom_uart[];
void print_adc_value(uint32_t adc_value)
{
    char msg[50];

    snprintf(msg, sizeof(msg), "Servo ADC value: %lu\r\n", adc_value);

    HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

void check_ina219_connection(void)
{
    char msg[80];

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, INA219_ADDR << 1, 3, 100);

    if (status == HAL_OK)
    {
        snprintf(msg, sizeof(msg), "INA219 found at address 0x%02X\r\n", INA219_ADDR);
    }
    else
    {
        snprintf(msg, sizeof(msg), "INA219 NOT found. Check wiring/address.\r\n");
    }

    HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

void send_string(char *msg)
{
    HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

void motor_off(void)
{
    // Motor off for this DRV8833 setup:
    // both inputs high.
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_MAX);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_MAX);
}

void motor_forward_50(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_MAX);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_HALF);
}

void motor_reverse_50(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_HALF);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_MAX);
}

void test_motor_pwm(void)
{
    send_string("PWM test starting\r\n");

    motor_forward_50();
    HAL_Delay(200);   // less than 1 second, short safety test

    motor_off();
    HAL_Delay(200);

    motor_reverse_50();
    HAL_Delay(200);

    motor_off();

    send_string("PWM test done\r\n");
}



void set_motor_pwm(int pwm)
{
    // pwm range: -2400 to +2400.
    // pwm > 0: one direction.
    // pwm < 0: opposite direction.
    // pwm = 0: motor off.

    if (pwm > PWM_MAX)
    {
        pwm = PWM_MAX;
    }

    if (pwm < -PWM_MAX)
    {
        pwm = -PWM_MAX;
    }

    if (pwm > 0)
    {
        // Forward:
        // CH1 stays high.
        // CH2 decreases as speed increases.
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_MAX);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_MAX - pwm);
    }
    else if (pwm < 0)
    {
        // Reverse:
        // CH2 stays high.
        // CH1 decreases as speed increases.
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_MAX + pwm);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_MAX);
    }
    else
    {
        motor_off();
    }
}






/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* STM32C0xx HAL library initialization:
       - Configure the Flash prefetch
       - Systick timer is configured by default as source of time base, but user
         can eventually implement his proper time base source (a general purpose
         timer for example or other time source), keeping in mind that Time base
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
         handled in milliseconds basis.
       - Low Level Initialization
     */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* Configure LED1 and LED2 */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);

  /* Configure User push-button in interrupt mode */
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  /* Configure reception filter to Rx FIFO 0 */
  FDCAN_FilterTypeDef        sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0U;
  sFilterConfig.FilterType   = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = TX_ID;
  sFilterConfig.FilterID2    = 0x7FF;
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /**
    *  Configure global filter:
    *    - Filter all remote frames with STD and EXT ID
    *    - Reject non matching frames with STD ID and EXT ID
    */
  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                   FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Prepare Tx message Header */
  txHeader.Identifier          = TX_ID;
  txHeader.IdType              = FDCAN_STANDARD_ID;
  txHeader.TxFrameType         = FDCAN_DATA_FRAME;
  txHeader.DataLength          = FDCAN_DLC_BYTES_16;
  txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  txHeader.BitRateSwitch       = FDCAN_BRS_ON;
  txHeader.FDFormat            = FDCAN_FD_CAN;
  txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  txHeader.MessageMarker       = 0U;

  /**
    * Configure and enable Tx Delay Compensation, required for BRS mode.
    * TdcOffset default recommended value: DataTimeSeg1 * DataPrescaler
    * TdcFilter default recommended value: 0
    */
  if (HAL_FDCAN_ConfigTxDelayCompensation(&hfdcan1,
                                          (hfdcan1.Init.DataPrescaler * hfdcan1.Init.DataTimeSeg1), 0U) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_EnableTxDelayCompensation(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start FDCAN controller */
  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  send_string("COM started. Type a to read ADC/current.\r\n");

  check_ina219_connection();//debugging


  init_ina219();  // Initialize the INA219 once before we start reading current.

  HAL_TIM_Base_Start_IT(&htim2);

  // Start with motor off before enabling PWM
  motor_off();

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

  // Make sure it stays off after PWM starts
  motor_off();

  send_string("PWM ready. Send a to read sensors, p to test motor.\r\n");


  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_BLUE);

  /* Initialize User push-button without interrupt mode. */
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint8_t received_char;

      HAL_UART_Receive(&hcom_uart[COM1], &received_char, 1, HAL_MAX_DELAY);

      if (received_char == '\n' || received_char == '\r')
      {
          continue;
      }

      if (received_char == 'a')
      {
          send_string("Starting current control test\r\n");

          motor_killed = 0;
          samples_collected = 0;

          state = 1;

          while (state == 1)
          {
              // Wait until interrupt finishes the 400-sample test
          }

          if (motor_killed == 1)
          {
              send_string("SAFETY STOP: ADC outside safe range\r\n");
          }

          send_string("index,desired,current,position\r\n");

          for (int i = 0; i < samples_collected; i++)
          {
              char msg[80];

              snprintf(msg, sizeof(msg), "%d,%d,%d,%lu\r\n",
                       i,
                       desired_current_array[i],
                       measured_current_array[i],
					   position_array[i]);

              HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
          }

          send_string("Done\r\n");
      }

      if (received_char == 'p'){

		  test_motor_pwm();

		  if (motor_killed == 1)
		  {
			  send_string("SAFETY STOP: ADC outside safe range\r\n");
		  }

	}

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = ENABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 1;
  hfdcan1.Init.NominalSyncJumpWidth = 12;
  hfdcan1.Init.NominalTimeSeg1 = 35;
  hfdcan1.Init.NominalTimeSeg2 = 12;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 6;
  hfdcan1.Init.DataTimeSeg1 = 17;
  hfdcan1.Init.DataTimeSeg2 = 6;
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10805D88;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2400;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 48000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  BSP User push-button callback
  * @param  Button Specifies the pin connected EXTI line
  * @retval None.
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
  if (Button == BUTTON_USER)
  {
    /* Turn LED1 off */
    BSP_LED_Off(LED1);

    /* Add message to TX FIFO */
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, txData) != HAL_OK)
    {
      Error_Handler();
    }

    /* Delay for simple button debounce */
    HAL_Delay(100U);
  }
}

/**
  * @brief  Compares two buffers.
  * @param  pBuffer1 buffer to be compared.
  * @param  pBuffer2 buffer to be compared.
  * @param  BufferLength: buffer's length.
  * @retval 0: pBuffer1 is identical to pBuffer2
  * @retval 1: pBuffer1 differs from pBuffer2
  */
static uint32_t BufferCmp8b(const uint8_t *pBuffer1, const uint8_t *pBuffer2, uint16_t BufferLength)
{
  while (BufferLength--)
  {
    if (*pBuffer1 != *pBuffer2)
    {
      return 1U;
    }

    pBuffer1++;
    pBuffer2++;
  }
  return 0U;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        // This runs every 1 ms if TIM2 is set to 1 kHz.
        static volatile int counter = 0;
        static volatile float eint = 0.0f;

        latest_position = read_servo_adc();
        // Always read the ADC in the interrupt for safety.

        // Safety check every 1 ms
        if (latest_position < ADC_MIN_SAFE || latest_position > ADC_MAX_SAFE)
        {
            motor_off();
            // Immediately turn off the motor if position is outside safe range.

            motor_killed = 1;
            // Remember that safety was triggered.

            state = 0;
            // If the main loop was waiting for the interrupt, release it.

            samples_collected = counter;
            counter = 0;
            eint = 0.0f;
            return;
            // Skip the rest of the interrupt.
        }

        if (state == 1)
        {
            signed short actual_current = readINA219(INA219_REG_CURRENT);
            signed short desired_current;

            // Flip desired current every 100 samples
            if ((counter / CURRENT_FLIP_TIME) % 2 == 0)
            {
                desired_current = DESIRED_CURRENT_RAW;
            }
            else
            {
                desired_current = -DESIRED_CURRENT_RAW;
            }

            int error = desired_current - actual_current;

            eint = eint + error;

            float u = KP_CURRENT * error + KI_CURRENT * eint;

            int pwm_command = (int)u;

            set_motor_pwm(pwm_command);

            desired_current_array[counter] = desired_current;
            measured_current_array[counter] = actual_current;
            position_array[counter] = latest_position;

            counter++;
            samples_collected = counter;

            if (counter >= NSAMPLES)
            {
                motor_off();

                state = 0;

                counter = 0;
                eint = 0.0f;
            }
        }
    }
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  while (1)
  {
    /* Toggle LED2 on */
    BSP_LED_Toggle(LED2);

    /* 1s delay */
    HAL_Delay(1000U);
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
      ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
