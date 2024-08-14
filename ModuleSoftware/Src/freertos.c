/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mpu6050.h"
#include "usbd_cdc_if.h"
#include <usart.h>

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
/* USER CODE BEGIN Variables */
MPU6050_t MPU6050;
uint8_t mpuReceiveBuff[READ_IMU_NBYTES];


typedef enum {
  FAIL =   0b00000000,
  FAST =   0b00000001, // during uart/cdc send/recv
  NORMAL = 0b00000010 // mpu6050 working
} blinkModes;
blinkModes ledBlinkMode;
TickType_t lastBlinkSet;

extern uint8_t sdajio[1024];
uint8_t uartCDCBuffer[1024];
extern volatile uint8_t mpuReceiveDone;
extern volatile uint8_t uartTransmitDone;

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId ledBlinkTaskHandle;
osMessageQId CDCuartHandle;
uint8_t _CDCuartBuffer[ 1024 * sizeof( uint8_t ) ];
osStaticMessageQDef_t CDCuartControlBlock;
osMessageQId uartCDCHandle;
uint8_t _uartCDCBuffer[ 1024 * sizeof( uint8_t ) ];
osStaticMessageQDef_t uartCDCControlBlock;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

TickType_t millis(){
  return xTaskGetTickCount()/configTICK_RATE_HZ*1000;
}

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void startLedBlink(void const * argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of CDCuart */
  osMessageQStaticDef(CDCuart, 1024, uint8_t, _CDCuartBuffer, &CDCuartControlBlock);
  CDCuartHandle = osMessageCreate(osMessageQ(CDCuart), NULL);

  /* definition and creation of uartCDC */
  osMessageQStaticDef(uartCDC, 1024, uint8_t, _uartCDCBuffer, &uartCDCControlBlock);
  uartCDCHandle = osMessageCreate(osMessageQ(uartCDC), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 512);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of ledBlinkTask */
  osThreadDef(ledBlinkTask, startLedBlink, osPriorityIdle, 0, 128);
  ledBlinkTaskHandle = osThreadCreate(osThread(ledBlinkTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  HAL_UART_Receive_DMA(&huart2,uartCDCBuffer,1);
  
  /* Infinite loop */
  for(;;)
  {
    osDelay(10);
    if(millis()-lastBlinkSet>3000){
      ledBlinkMode = FAIL;
      lastBlinkSet = millis();
    }

    if(mpuReceiveDone){
      mpuReceiveDone = 0;
      MPU6050_fill(&MPU6050,mpuReceiveBuff);

      HAL_I2C_Mem_Read_DMA(&hi2c1,MPU6050_ADDR,DATA_START_REG,1,mpuReceiveBuff,READ_IMU_NBYTES);
      ledBlinkMode = NORMAL;
      lastBlinkSet = millis();
    }
    
    { // uart -> cdc
      static uint8_t tmp[1024];
      int i=0;
      while(xQueueReceive(uartCDCHandle, tmp+i,0)==pdTRUE) i++;
      
      // size of data received
      if(i!=0){
        ledBlinkMode = FAST;
        lastBlinkSet = millis();
        CDC_Transmit_FS(tmp, i);
      }
    }
    
    // cdc->uart
    if(uartTransmitDone){
      static uint8_t CDCuartBuffer[1024];
      int i=0;
      while(xQueueReceive(CDCuartHandle, CDCuartBuffer+i,0)==pdTRUE) i++;
      // size of data received
      if(i!=0){
        uartTransmitDone =0;
        ledBlinkMode = FAST;
        lastBlinkSet = millis();
      }
      HAL_UART_Transmit_DMA(&huart2, CDCuartBuffer, i);
    }

    {
      // CDC_Transmit_FS(uartCDCBuffer, 100);
      // uint8_t c[] = "\n\r";
      // CDC_Transmit_FS(&c, 2);
    }
    
    
    
    
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_startLedBlink */
/**
* @brief Function implementing the ledBlinkTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startLedBlink */
void startLedBlink(void const * argument)
{
  /* USER CODE BEGIN startLedBlink */
  /* Infinite loop */
  for(;;)
  {
    if(ledBlinkMode == FAST ){
      osDelay(100);
    }else if (ledBlinkMode == NORMAL){
      osDelay(1000);
    } else{
      HAL_GPIO_WritePin(GPIOC, SYS_LED_Pin, GPIO_PIN_SET);
      osDelay(10000);
    }
    HAL_GPIO_TogglePin(GPIOC, SYS_LED_Pin);
  }
  /* USER CODE END startLedBlink */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
