/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "stm32h7xx_hal.h"

#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/api.h"

#include "lwip/apps/sntp.h"

#include "app_ethernet.h"
#include "ethernetif.h"

#include <string.h>
#include <time.h>

extern void SSL_Client(void *argument);

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#if LWIP_DHCP
#define MAX_DHCP_TRIES 4

// Fallback

/*Static IP ADDRESS*/
#define IP_ADDR0 ((uint8_t)192U)
#define IP_ADDR1 ((uint8_t)168U)
#define IP_ADDR2 ((uint8_t)31U)
#define IP_ADDR3 ((uint8_t)10U)

/*NETMASK*/
#define NETMASK_ADDR0 ((uint8_t)255U)
#define NETMASK_ADDR1 ((uint8_t)255U)
#define NETMASK_ADDR2 ((uint8_t)255U)
#define NETMASK_ADDR3 ((uint8_t)0U)

/*Gateway Address*/
#define GW_ADDR0 ((uint8_t)192U)
#define GW_ADDR1 ((uint8_t)168U)
#define GW_ADDR2 ((uint8_t)31U)
#define GW_ADDR3 ((uint8_t)1U)

__IO uint8_t eth_dhcp_state = DHCP_OFF;
#endif

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/**
 * @brief  Notify the User about the network interface config status
 * @param  netif: the network interface
 * @retval None
 */
void ethernet_link_status_updated(struct netif *netif)
{
  if (netif_is_up(netif))
  {
#if LWIP_DHCP
    /* Update DHCP state machine */
    eth_dhcp_state = DHCP_START;
#endif /* LWIP_DHCP */
  }
  else
  {
#if LWIP_DHCP
    /* Update DHCP state machine */
    eth_dhcp_state = DHCP_LINK_DOWN;
#endif /* LWIP_DHCP */
  }
}

#if LWIP_DHCP
/**
 * @brief  DHCP Process
 * @param  argument: network interface
 * @retval None
 */
void lwip_dhcp_thread(void *argument)
{
  struct netif *netif = (struct netif *)argument;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
  struct dhcp *dhcp;

  for (;;)
  {
    switch (eth_dhcp_state)
    {
    case DHCP_START:
    {
      ip_addr_set_zero_ip4(&netif->ip_addr);
      ip_addr_set_zero_ip4(&netif->netmask);
      ip_addr_set_zero_ip4(&netif->gw);
      eth_dhcp_state = DHCP_WAIT_ADDRESS;

      dhcp_start(netif);
    }
    break;
    case DHCP_WAIT_ADDRESS:
    {
      if (dhcp_supplied_address(netif))
      {
        eth_dhcp_state = DHCP_ADDRESS_ASSIGNED;

        /* Start NTP */
        xTaskCreate(lwip_ntp_thread, "ntp_thread", configMINIMAL_STACK_SIZE * 2, netif, tskIDLE_PRIORITY + 2, NULL);
        xTaskCreate(SSL_Client, "SSL_Client", configMINIMAL_STACK_SIZE * 15, netif, tskIDLE_PRIORITY + 2, NULL);
        
      }
      else
      {
        dhcp = (struct dhcp *)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

        /* DHCP timeout */
        if (dhcp->tries > MAX_DHCP_TRIES)
        {
          eth_dhcp_state = DHCP_TIMEOUT;

          /* Static address used */
          IP_ADDR4(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
          IP_ADDR4(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
          IP_ADDR4(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
          netif_set_addr(netif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), ip_2_ip4(&gw));
        }
      }
    }
    break;
    case DHCP_LINK_DOWN:
    {
      eth_dhcp_state = DHCP_OFF;
    }
    break;
    default:
      break;
    }

    /* wait 500 ms */
    vTaskDelay(500);
  }
}
#endif /* LWIP_DHCP */

/**
 * @brief  NTP Process
 * @param  argument: network interface
 * @retval None
 */
void lwip_ntp_thread(void *argument)
{
  ip_addr_t addr;

  IP4_ADDR(&addr, 157, 119, 101, 100); /* xTom */

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_init();

  sntp_setserver(0, &addr);

  while (1)
  {
    vTaskDelete(NULL);
  }
}

extern RTC_HandleTypeDef hrtc;
void sntp_set_system_time(time_t sntp_time)
{
  struct tm *tm;
  RTC_DateTypeDef RTC_DateStructure;
  RTC_TimeTypeDef RTC_TimeStructure;

  tm = gmtime(&sntp_time);

  RTC_DateStructure.Year = tm->tm_year + 1900 - 2000;
  RTC_DateStructure.Month = tm->tm_mon + 1;
  RTC_DateStructure.Date = tm->tm_mday;
  RTC_DateStructure.WeekDay = tm->tm_wday;

  RTC_TimeStructure.Hours = tm->tm_hour + 8; /* UTC + 8 */
  RTC_TimeStructure.Minutes = tm->tm_min;
  RTC_TimeStructure.Seconds = tm->tm_sec;
  RTC_TimeStructure.TimeFormat = RTC_HOURFORMAT12_AM;
  RTC_TimeStructure.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  RTC_TimeStructure.StoreOperation = RTC_STOREOPERATION_RESET;

  HAL_RTC_SetDate(&hrtc, &RTC_DateStructure, RTC_FORMAT_BIN);
  HAL_RTC_SetTime(&hrtc, &RTC_TimeStructure, RTC_FORMAT_BIN);
}

time_t rtc_get_system_time(time_t * timer){
  struct tm tm;
  RTC_DateTypeDef RTC_DateStructure;
  RTC_TimeTypeDef RTC_TimeStructure;
  
  if(timer != NULL){
    return time(timer);
  }
  
  HAL_RTC_GetTime(&hrtc, &RTC_TimeStructure, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &RTC_DateStructure, RTC_FORMAT_BIN);
  
  tm.tm_hour = RTC_TimeStructure.Hours;
  tm.tm_min = RTC_TimeStructure.Minutes;
  tm.tm_sec = RTC_TimeStructure.Seconds;
 
  tm.tm_year = RTC_DateStructure.Year + 100;
  tm.tm_mon = RTC_DateStructure.Month;
  tm.tm_mday = RTC_DateStructure.Date;
  tm.tm_wday = RTC_DateStructure.WeekDay;
  tm.tm_yday = 0; /* Can't Easy Calc,waste time,so ignore it. */
  
  return mktime(&tm);
}
