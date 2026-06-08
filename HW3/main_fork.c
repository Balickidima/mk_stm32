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
 * @brief Структура для хранения сохраненных данных во Flash памяти
 */
typedef struct
{
  InformaciyaONazhatiiKnopki poslednyaya_struktura;  /*!< Последняя сохраненная структура */
  uint32_t vremya_sohraneniya;                       /*!< Время сохранения */
  uint8_t flag_sohraneniya;                          /*!< Флаг наличия сохраненных данных */
} SohrannennyeDannie;
  
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
InformaciyaONazhatiiKnopki informaciya_o_nazhatii; /* Структура для хранения информации о нажатиях */
SohrannennyeDannie sohrannennye_dannie;         /* Структура для сохраненных данных */
#define FLASH_ADDR_SOHRAHENIYA  0x0803F000       /* Адрес во Flash для сохранения данных (последняя страница) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/**
 * @brief Функция сохранения структуры данных во Flash память
 * @param data Указатель на данные для сохранения
 * @param size Размер данных в байтах
 * @note Функция разблокирует Flash, сохраняет данные и блокирует обратно
 */
static void SohranitVDannyeVFlash(uint8_t *data, uint32_t size);

/**
 * @brief Функция чтения сохраненных данных из Flash памяти
 * @param data Указатель на буфер для чтения
 * @param size Размер данных для чтения
 * @note Функция проверяет наличие сохраненных данных
 */
static void ProchitatDannieIzFlash(uint8_t *data, uint32_t size);

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
 * @brief Функция визуальной индикации морганием светодиода
 * @param count Количество морганий
 * @note Светодиод моргает с интервалом 100мс между состояниями
 */
static void Zamerzhenie_Svetodioda(uint32_t count)
{
  for (uint32_t i = 0; i < count; i++)
  {
    /* Включение светодиода (активный низкий уровень) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(100);
    /* Выключение светодиода */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(100);
  }
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
 * @brief Функция сохранения структуры данных во Flash память
 * @param data Указатель на данные для сохранения
 * @param size Размер данных в байтах
 * @note Функция разблокирует Flash, сохраняет данные и блокирует обратно
 */
static void SohranitVDannyeVFlash(uint8_t *data, uint32_t size)
{
  /* Разблокировка Flash памяти */
  HAL_FLASH_Unlock();
  
  /* Очистка страницы Flash перед записью */
  FLASH_EraseInitTypeDef erase_init;
  erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase_init.Banks = FLASH_BANK_1;
  erase_init.Sector = 5;   /* Номер сектора для адреса 0x0803F000 (сектор 5, 128KB) */
  erase_init.NbSectors = 1;
  erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  uint32_t page_error;

  if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
  {
    Error_Handler();
  }
  
  /* Запись данных во Flash без задержек */
  for (uint32_t i = 0; i < size; i += 4)
  {
    uint32_t word_data = 0;
    if (i + 3 < size)
    {
      word_data = ((uint32_t)data[i] << 24) | 
                  ((uint32_t)data[i+1] << 16) | 
                  ((uint32_t)data[i+2] << 8) | 
                  ((uint32_t)data[i+3]);
    }
    else
    {
      /* Дозапись последних байтов */
      for (uint32_t j = 0; j < (size - i); j++)
      {
        word_data |= ((uint32_t)data[i+j] << (24 - j*8));
      }
    }
    
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_ADDR_SOHRAHENIYA + i, word_data) != HAL_OK)
    {
      Error_Handler();
    }
  }
  
  /* Блокировка Flash памяти */
  HAL_FLASH_Lock();
}

/**
 * @brief Функция чтения сохраненных данных из Flash памяти
 * @param data Указатель на буфер для чтения
 * @param size Размер данных для чтения
 * @note Функция проверяет наличие сохраненных данных и обнуляет буфер, если данные не инициализированы (все 0xFF)
 */
static void ProchitatDannieIzFlash(uint8_t *data, uint32_t size)
{
  uint8_t all_ff = 1;
  /* Чтение данных из Flash */
  for (uint32_t i = 0; i < size; i++)
  {
    data[i] = *(__IO uint8_t*)(FLASH_ADDR_SOHRAHENIYA + i);
    if (data[i] != 0xFF)
      all_ff = 0;
  }
  /* Если все байты 0xFF, то данные не инициализированы, обнуляем буфер */
  if (all_ff)
  {
    for (uint32_t i = 0; i < size; i++)
      data[i] = 0;
  }
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

  /* Инициализация переменных управления PWM - по умолчанию режим выключен */
  modul_raboty_svetodioda = 0;
  znachenie_duty_cikla = 0;
  napravlenie_izmeneniya = 0;
  vremya_nazhatiya_knopki = 0;
  
  /* Инициализация структуры информации о нажатиях */
  informaciya_o_nazhatii.vremya_nachala = 0;
  informaciya_o_nazhatii.prodolzhitelnost = 0;
  informaciya_o_nazhatii.kolichestvo_nazhatiy = 0;
  informaciya_o_nazhatii.tip_nazhatiya = 0;
  
  /* Инициализация структуры сохраненных данных */
  sohrannennye_dannie.flag_sohraneniya = 0;
  sohrannennye_dannie.vremya_sohraneniya = 0;
  
  /* Попытка чтения сохраненных данных из Flash */
  ProchitatDannieIzFlash((uint8_t*)&sohrannennye_dannie, sizeof(SohrannennyeDannie));

  /* Проверка, что данные корректны (не все 0xFF) */
  if (sohrannennye_dannie.flag_sohraneniya == 0xFF)
  {
    /* Данные не инициализированы, сбрасываем структуру */
    sohrannennye_dannie.flag_sohraneniya = 0;
    sohrannennye_dannie.vremya_sohraneniya = 0;
    sohrannennye_dannie.poslednyaya_struktura.vremya_nachala = 0;
    sohrannennye_dannie.poslednyaya_struktura.prodolzhitelnost = 0;
    sohrannennye_dannie.poslednyaya_struktura.kolichestvo_nazhatiy = 0;
    sohrannennye_dannie.poslednyaya_struktura.tip_nazhatiya = 0;
  }

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
        /* Длинное нажатие (>=5 секунд): сохранение данных во Flash */
        Zamerzhenie_Svetodioda(5);
        
        /* Копирование текущей структуры в сохраняемые данные */
        sohrannennye_dannie.poslednyaya_struktura = informaciya_o_nazhatii;
        sohrannennye_dannie.vremya_sohraneniya = HAL_GetTick();
        sohrannennye_dannie.flag_sohraneniya = 1;
        
        /* Сохранение данных во Flash */
        SohranitVDannyeVFlash((uint8_t*)&sohrannennye_dannie, sizeof(SohrannennyeDannie));
        
        /* Визуальное подтверждение сохранения */
        Zamerzhenie_Svetodioda(2);
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