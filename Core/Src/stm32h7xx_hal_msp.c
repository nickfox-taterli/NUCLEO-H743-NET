/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

/**
 * Initializes the Global MSP.
 */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
}

/**
 * @brief RTC MSP Initialization
 * This function configures the hardware resources used in this example
 * @param hrtc: RTC handle pointer
 * @retval None
 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    for (;;)
      ;
  }

  /* Peripheral clock enable */
  __HAL_RCC_RTC_ENABLE();
}

/**
  * @brief RNG MSP Initialization
  *        This function configures the hardware resources used in this application:
  *           - Peripheral's clock enable
  * @param hrng: RNG handle pointer
  * @retval None
  */
void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

  /*Select PLL output as RNG clock source */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RNG;
  PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_PLL;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

  /* RNG Peripheral clock enable */
  __RNG_CLK_ENABLE();

}

/**
 * @brief RTC MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param hrtc: RTC handle pointer
 * @retval None
 */
void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc)
{
  __HAL_RCC_RTC_DISABLE();
}

void HAL_Delay(uint32_t Delay)
{
  vTaskDelay(Delay);
}

uint32_t HAL_GetTick(void)
{
  return xTaskGetTickCount();
}
