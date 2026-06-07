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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "core_cm4.h"
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

/* USER CODE BEGIN PV */
/* Переменные для обработки кнопки и PWM */
volatile uint8_t led_pwm_active = 0;    /* Флаг активного PWM режима: 0 - выкл, 1 - вкл */
uint8_t pwm_duty = 0;                   /* Текущий duty cycle PWM (0-100) */
uint8_t pwm_direction = 0;              /* Направление изменения PWM: 0 - рост, 1 - спад */
volatile uint32_t button_press_time = 0; /* Время нажатия кнопки (для детекции длительного нажатия) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Задержка в микросекундах для PWM */
static void Delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t freq = HAL_RCC_GetHCLKFreq() / 1000000;  /* Частота в МГц */
  while ((DWT->CYCCNT - start) < (us * freq));
}

/* Функция получения текущего уровня RDP */
static uint8_t Get_RDP_Level(void)
{
  FLASH_OBProgramInitTypeDef FLASH_OBInitStruct;
  
  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();
  
  HAL_FLASHEx_OBGetConfig(&FLASH_OBInitStruct);
  
  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
  
  return FLASH_OBInitStruct.RDPLevel;
}

/* Функция моргания светодиодом */
static void Blink_LED(uint32_t count)
{
  for (uint32_t i = 0; i < count; i++)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(100);
  }
}

/* Функция установки RDP уровня */
static void Set_RDP(uint8_t level)
{
  FLASH_OBProgramInitTypeDef FLASH_OBInitStruct;

  /* Разблокировка Flash и Option Bytes */
  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();

  /* Чтение текущей конфигурации Option Bytes */
  HAL_FLASHEx_OBGetConfig(&FLASH_OBInitStruct);

  /* Настройка RDP уровня */
  FLASH_OBInitStruct.OptionType = OPTIONBYTE_RDP;
  FLASH_OBInitStruct.RDPLevel = (level == 1) ? OB_RDP_LEVEL_1 : OB_RDP_LEVEL_0;

  /* Программирование Option Bytes */
  if (HAL_FLASHEx_OBProgram(&FLASH_OBInitStruct) != HAL_OK)
  {
    /* Ошибка программирования */
    Error_Handler();
  }

  /* Запуск изменения Option Bytes */
  HAL_FLASH_OB_Launch();

  /* Блокировка обратно */
  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
}

/* Функция программного PWM для светодиода 
 * Светодиод на PC13 активный низкий (0 = горит, 1 = выкл)
 */
static void PWM_LED(uint8_t duty)
{
  /* Светодиод ГОРИТ на duty% периода (низкий уровень) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  Delay_us(duty * 10);  /* 10 мкс на 1% duty cycle */
  
  /* Светодиод ВЫКЛЮЧЕН на (100-duty)% периода (высокий уровень) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  Delay_us((100 - duty) * 10);
}

/* Функция плавного изменения яркости */
static void Update_PWM(void)
{
  if (pwm_direction == 0)
  {
    pwm_duty++;
    if (pwm_duty >= 100)
    {
      pwm_duty = 100;
      pwm_direction = 1;  /* Меняем направление на спад */
    }
  }
  else
  {
    pwm_duty--;
    if (pwm_duty == 0)
    {
      pwm_duty = 0;
      pwm_direction = 0;  /* Меняем направление на рост */
    }
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
  
  /* USER CODE BEGIN 2 */
  /* Инициализация DWT для задержек в микросекундах */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* Инициализация переменных PWM - по умолчанию светодиод выключен */
  led_pwm_active = 0;
  pwm_duty = 0;
  pwm_direction = 0;
  button_press_time = 0;

  /* Выключаем светодиод изначально (активный низкий - SET = выкл) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  while (1)
  {
    /* Проверка нажатия кнопки PA0 (активный низкий уровень) */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET && button_press_time == 0)
    {
      /* Запоминаем время нажатия */
      button_press_time = HAL_GetTick();
    }
    /* Отпускание кнопки */
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET && button_press_time != 0)
    {
      /* Вычисляем длительность нажатия */
      uint32_t press_duration = HAL_GetTick() - button_press_time;
      button_press_time = 0;

      /* Проверяем, длительное ли это нажатие (>= 2 секунды) */
      if (press_duration >= 2000)
      {
        /* Длительное нажатие: моргаем 3 раза и переключаем RDP */
        Blink_LED(3);
        uint8_t current_rdp = Get_RDP_Level();
        if (current_rdp == OB_RDP_LEVEL_1)
        {
          /* RDP включен, выключаем (Level 0) - произойдет сброс и очистка */
          Set_RDP(0);
          /* Код после этой точки не должен выполняться из-за сброса */
        }
        else
        {
          /* RDP выключен, включаем (Level 1) */
          Set_RDP(1);
          /* Код продолжает работать, RDP активен */
        }
      }
      else
      {
        /* Короткое нажатие: переключаем режим PWM */
        led_pwm_active = !led_pwm_active;

        /* При включении сбрасываем параметры PWM */
        if (led_pwm_active)
        {
          pwm_duty = 0;
          pwm_direction = 0;
        }
        else
        {
          /* При выключении гасим светодиод (активный низкий - SET = выкл) */
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        }
      }
    }

    if (led_pwm_active)
    {
      /* Режим плавного PWM: зажигаем и гасим светодиод */
      PWM_LED(pwm_duty);
      Update_PWM();
    }
    /* Если led_pwm_active == 0, светодиод выключен */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 214;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Добавляем подтяжку для кнопки PA0 (кнопка на GND - нужна pull-up) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */