/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main_fork.c
  * @brief          : Основная программа с измененным стилем кода
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
  
/**
 * @brief Структура данных для хранения информации о нажатии кнопки
 */
typedef struct
{
  uint32_t vremya_nachala;      /*!< Время начала нажатия (мс) */
  uint32_t prodolzhitelnost;    /*!< Длительность нажатия (мс) */
  uint8_t kolichestvo_nazhatiy; /*!< Количество нажатий */
  uint8_t tip_nazhatiya;        /*!< Тип нажатия: 0-короткое, 1-длинное */
} InformaciyaONazhatiiKnopki;

/**
 * @brief Структура для хранения информации о причине перезагрузки
 */
typedef struct
{
  uint8_t wdgrst_flag;         /*!< Флаг сброса сторожевым таймером */
  uint8_t pinrst_flag;         /*!< Флаг сброса по ножке */
  uint8_t porst_flag;          /*!< Флаг сброса при включении питания */
  uint8_t sftsrst_flag;        /*!< Флаг программного сброса */
} PrikaziPerezagruzki;
  
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Глобальные переменные для управления светодиодом и кнопкой */
volatile uint8_t modul_raboty_svetodioda = 0;    /* Флаг режима работы светодиода: 0 - выкл, 1 - PWM режим */
uint8_t znachenie_duty_cikla = 0;               /* Текущее значение заполнения PWM (0-100) */
uint8_t napravlenie_izmeneniya = 0;             /* Направление изменения PWM: 0 - увеличение, 1 - уменьшение */
volatile uint32_t vremya_nazhatiya_knopki = 0;  /* Время начала нажатия кнопки (для определения длительности) */
volatile uint32_t vremya_poslednego_nazhatiya = 0; /* Время последнего нажатия кнопки (для IWDG) */
InformaciyaONazhatiiKnopki informaciya_o_nazhatii; /* Структура для хранения информации о нажатиях */
PrikaziPerezagruzki prichina_perezagruzki;      /* Структура для хранения причины перезагрузки */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/**
 * @brief Функция чтения и обработки флагов причины перезагрузки
 * @note Читает регистр RCC_CSR, сохраняет флаги и сбрасывает их
 */
static void ProchitatPerezagruzki(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Функция создания задержки в микросекундах
 * @param us Количество микросекунд для задержки
 * @note Использует цикл процессора с счетчиком DWT для точной задержки
 */
static void Mikrosekundnaya_Zaderzhka(uint32_t us)
{
  uint32_t nachalo = DWT->CYCCNT;
  uint32_t chastota = HAL_RCC_GetHCLKFreq() / 1000000;  /* Частота в МГц */
  while ((DWT->CYCCNT - nachalo) < (us * chastota));
}

/**
 * @brief Функция программного управления яркостью светодиода через PWM
 * @param duty Значение заполнения импульса (0-100)
 * @note Реализует программный PWM с периодом 1мс (10мкс на 1%)
 * @note Светодиод подключен к выводу PC13, активный низкий уровень
 */
static void Upravlenie_Svetodiodom_PWM(uint8_t duty)
{
  /* Включение светодиода на заданный процент периода */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  Mikrosekundnaya_Zaderzhka(duty * 10);  /* 10 мкс на 1% duty cycle */
  
  /* Выключение светодиода на оставшийся процент периода */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  Mikrosekundnaya_Zaderzhka((100 - duty) * 10);
}

/**
 * @brief Функция плавного изменения яркости светодиода
 * @note Автоматически изменяет значение duty cycle от 0 до 100 и обратно
 * @note Изменяет направление при достижении крайних значений
 */
static void Obnovit_Znachenie_PWM(void)
{
  if (napravlenie_izmeneniya == 0)
  {
    /* Увеличиваем яркость */
    znachenie_duty_cikla++;
    if (znachenie_duty_cikla >= 100)
    {
      znachenie_duty_cikla = 100;
      napravlenie_izmeneniya = 1;  /* Меняем направление на уменьшение */
    }
  }
  else
  {
    /* Уменьшаем яркость */
    znachenie_duty_cikla--;
    if (znachenie_duty_cikla == 0)
    {
      znachenie_duty_cikla = 0;
      napravlenie_izmeneniya = 0;  /* Меняем направление на увеличение */
    }
  }
}

/**
 * @brief Функция чтения и обработки флагов причины перезагрузки
 * @note Читает регистр RCC_CSR, сохраняет флаги и сбрасывает их
 */
static void ProchitatPerezagruzki(void)
{
  uint32_t rcc_csr;
  
  /* Чтение регистра RCC_CSR */
  rcc_csr = RCC->CSR;
  
  /* Сохранение флагов причины перезагрузки */
  prichina_perezagruzki.wdgrst_flag = (rcc_csr & RCC_CSR_WDGRSTF) != 0;
  prichina_perezagruzki.pinrst_flag = (rcc_csr & RCC_CSR_PINRSTF) != 0;
  prichina_perezagruzki.porst_flag = (rcc_csr & RCC_CSR_PORRSTF) != 0;
  prichina_perezagruzki.sftsrst_flag = (rcc_csr & RCC_CSR_SFTRSTF) != 0;
  
  /* Сброс флагов записи RCC_CSR_RMVF */
  RCC->CSR |= RCC_CSR_RMVF;
}

/* USER CODE END 0 */

/**
  * @brief  Основная функция программы
  * @retval int Код возврата (не используется)
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* Инициализация микроконтроллера--------------------------------------------------------*/

  /* Сброс всех периферийных устройств, инициализация Flash интерфейса и Systick */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Настройка системного тактового генератора */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Инициализация всех настроенных периферийных устройств */
  MX_GPIO_Init();
  
  /* USER CODE BEGIN 2 */
  /* Инициализация счетчика циклов DWT для точных задержек */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* Чтение и обработка флагов причины перезагрузки */
  ProchitatPerezagruzki();

  /* Инициализация переменных управления PWM - по умолчанию режим выключен */
  modul_raboty_svetodioda = 0;
  znachenie_duty_cikla = 0;
  napravlenie_izmeneniya = 0;
  vremya_nazhatiya_knopki = 0;
  vremya_poslednego_nazhatiya = HAL_GetTick(); /* Инициализация времени последнего нажатия */
  
  /* Инициализация структуры информации о нажатиях */
  informaciya_o_nazhatii.vremya_nachala = 0;
  informaciya_o_nazhatii.prodolzhitelnost = 0;
  informaciya_o_nazhatii.kolichestvo_nazhatiy = 0;
  informaciya_o_nazhatii.tip_nazhatiya = 0;
  
  /* Устанавливаем светодиод в выключенное состояние (активный низкий уровень) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Бесконечный цикл программы */
  /* USER CODE BEGIN WHILE */
  
  while (1)
  {
    /* Мониторинг состояния кнопки PA0 (активный низкий уровень) */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET && vremya_nazhatiya_knopki == 0)
    {
      /* Фиксация времени начала нажатия */
      vremya_nazhatiya_knopki = HAL_GetTick();
      vremya_poslednego_nazhatiya = HAL_GetTick(); /* Обновление времени последнего нажатия */
      informaciya_o_nazhatii.vremya_nachala = HAL_GetTick();
      informaciya_o_nazhatii.kolichestvo_nazhatiy++;
    }
    /* Проверка отпускания кнопки */
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET && vremya_nazhatiya_knopki != 0)
    {
      /* Расчет длительности нажатия */
      uint32_t prodolzhitelnost_nazhatiya = HAL_GetTick() - vremya_nazhatiya_knopki;
      vremya_nazhatiya_knopki = 0;
      
      /* Сохранение информации о нажатии в структуру */
      informaciya_o_nazhatii.prodolzhitelnost = prodolzhitelnost_nazhatiya;
      informaciya_o_nazhatii.tip_nazhatiya = (prodolzhitelnost_nazhatiya >= 2000) ? 1 : 0;

      /* Определение типа нажатия */
      if (prodolzhitelnost_nazhatiya >= 5000)
      {
        NVIC_SystemReset();
      }
      else
      {
        /* Короткое нажатие: переключение режима работы светодиода */
        modul_raboty_svetodioda = !modul_raboty_svetodioda;

        /* Инициализация параметров PWM при включении режима */
        if (modul_raboty_svetodioda)
        {
          znachenie_duty_cikla = 0;
          napravlenie_izmeneniya = 0;
        }
        else
        {
          /* Выключение светодиода при выходе из PWM режима */
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        }
      }
    }

    /* Проверка времени бездействия (1 минута = 60000 мс) */
    if ((HAL_GetTick() - vremya_poslednego_nazhatiya) > 60000)
    {
      NVIC_SystemReset(); /* Перезагрузка устройства при бездействии более 1 минуты */
    }

    /* Основной цикл работы в PWM режиме */
    if (modul_raboty_svetodioda)
    {
      /* Плавное изменение яркости светодиода */
      Upravlenie_Svetodiodom_PWM(znachenie_duty_cikla);
      Obnovit_Znachenie_PWM();
    }
    /* Если modul_raboty_svetodioda == 0, светодиод выключен */
  }
  /* USER CODE END 3 */
}

/**
  * @brief  Функция конфигурации системного тактового генератора
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef struktura_init_oscillyatorov = {0};
  RCC_ClkInitTypeDef struktura_init_chasov = {0};

  /* Настройка выходного напряжения внутреннего регулятора */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /* Инициализация RCC генераторов согласно заданным параметрам */
  struktura_init_oscillyatorov.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  struktura_init_oscillyatorov.HSEState = RCC_HSE_ON;
  struktura_init_oscillyatorov.PLL.PLLState = RCC_PLL_ON;
  struktura_init_oscillyatorov.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  struktura_init_oscillyatorov.PLL.PLLM = 16;
  struktura_init_oscillyatorov.PLL.PLLN = 214;
  struktura_init_oscillyatorov.PLL.PLLP = RCC_PLLP_DIV4;
  struktura_init_oscillyatorov.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&struktura_init_oscillyatorov) != HAL_OK)
  {
    Error_Handler();
  }

  /* Инициализация тактовых сигналов CPU, AHB и APB шин */
  struktura_init_chasov.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  struktura_init_chasov.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  struktura_init_chasov.AHBCLKDivider = RCC_SYSCLK_DIV1;
  struktura_init_chasov.APB1CLKDivider = RCC_HCLK_DIV2;
  struktura_init_chasov.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&struktura_init_chasov, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /* Включение системы безопасности тактового сигнала (Clock Security System) */
  HAL_RCC_EnableCSS();
}

/**
  * @brief  Функция инициализации GPIO
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef struktura_init_gpio = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* Включение тактирования GPIO портов */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Настройка начального состояния вывода светодиода */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /* Конфигурация вывода светодиода PC13 */
  struktura_init_gpio.Pin = GPIO_PIN_13;
  struktura_init_gpio.Mode = GPIO_MODE_OUTPUT_PP;
  struktura_init_gpio.Pull = GPIO_NOPULL;
  struktura_init_gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &struktura_init_gpio);

  /* Конфурация вывода кнопки PA0 */
  struktura_init_gpio.Pin = GPIO_PIN_0;
  struktura_init_gpio.Mode = GPIO_MODE_INPUT;
  struktura_init_gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &struktura_init_gpio);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Добавление подтяжки для кнопки PA0 (кнопка подключена к GND) */
  struktura_init_gpio.Pin = GPIO_PIN_0;
  struktura_init_gpio.Mode = GPIO_MODE_INPUT;
  struktura_init_gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &struktura_init_gpio);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Функция обработки ошибок
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* Пользователь может добавить свою реализацию для обработки состояния ошибки HAL */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Функция отчета об ошибке assert
  * @param  file: указатель на имя исходного файла
  * @param  line: номер строки с ошибкой
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* Пользователь может добавить свою реализацию для вывода имени файла и номера строки,
     например: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */