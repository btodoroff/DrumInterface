#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "xformatc.h"

/* Demo program include files. */
//#include "partest.h"
#include "USBSerial.h"

#define USBSERIALSTACK_SIZE		configMINIMAL_STACK_SIZE
const static TickType_t xDelay = 1 / portTICK_PERIOD_MS;
const static TickType_t mDelay = 5000 / portTICK_PERIOD_MS;
    
/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vUSBSerialTask, pvParameters );

SemaphoreHandle_t USBSerialMutex;

/*-----------------------------------------------------------*/

void usbserial_putString(const char msg[])
{
    if(0 == USBUART_GetConfiguration()) return;
    xSemaphoreTake(USBSerialMutex,portMAX_DELAY);
    if(0 == USBUART_CDCIsReady())
    {
        vTaskDelay(xDelay); //Wait 1ms for port to free up, then abandon
        if(0 == USBUART_CDCIsReady())
        {
            xSemaphoreGive(USBSerialMutex);
            return;
        }
    }
    USBUART_PutString(msg);
    xSemaphoreGive(USBSerialMutex);
}

static char usbserialBuf[128];
static void usbSerialPutchar(void *arg,char c)
{
    char ** s = (char **)arg;
    *(*s)++ = c;
}

void usbserial_xprintf(const char *fmt,...)
{
    if(0 == USBUART_GetConfiguration()) return;
    xSemaphoreTake(USBSerialMutex,portMAX_DELAY);
    if(0 == USBUART_CDCIsReady())
    {
        vTaskDelay(xDelay); //Wait 1ms for port to free up, then abandon
        if(0 == USBUART_CDCIsReady())
        {
            xSemaphoreGive(USBSerialMutex);
            return;
        }
    }
    char *buf = usbserialBuf;
    va_list list;
    va_start(list,fmt);
    xvformat(usbSerialPutchar,(void *)&buf,fmt,list);
    *buf = 0;
    USBUART_PutString(usbserialBuf);
    va_end(list);
    xSemaphoreGive(USBSerialMutex);
}

/*
void xsprintf(char *buf,const char *fmt,...)
{
    va_list list;
    va_start(list,fmt);
    xvformat(myPutchar,(void *)&buf,fmt,list);
    *buf = 0;
    va_end(list);
}*/

void vStartUSBSerialTasks( UBaseType_t uxPriority )
{
    /*Setup the mutex to control port access*/
    USBSerialMutex = xSemaphoreCreateMutex();
	/* Spawn the task. */
	xTaskCreate( vUSBSerialTask, "USBSerial", USBSERIALSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}

/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vUSBSerialTask, pvParameters )
{
   	/* The parameters are not used. */
	( void ) pvParameters;

    /* Start the USB_UART */
    while(0u == USBUART_GetConfiguration());
    USBUART_CDC_Init();

    
	for(;;)
	{
        /* Host can send double SET_INTERFACE request. */
        if (0u != USBUART_IsConfigurationChanged())
        {
            /* Initialize IN endpoints when device is configured. */
            if (0u != USBUART_GetConfiguration())
            {
                /* Enumeration is done, enable OUT endpoint to receive data 
                 * from host. */
                USBUART_CDC_Init();
            }
        }
        
        if(0 != USBUART_GetConfiguration())
        {
            /* Get and process inputs here */
        }
        vTaskDelay(xDelay);
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

