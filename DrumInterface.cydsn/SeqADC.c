#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "SeqADC.h"
#include "USBSerial.h"

//#include "xformatc.h"

/* Demo program include files. */
//#include "partest.h"
#include "ADC.h"

#define ADCSTACK_SIZE		configMINIMAL_STACK_SIZE
//const static TickType_t xDelay = 1 / portTICK_PERIOD_MS;
//const static TickType_t mDelay = 5000 / portTICK_PERIOD_MS;
    
/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vADCTask, pvParameters );
int lastSample[ADCNUM_CHANNELS];
/*-----------------------------------------------------------*/

void vStartADCTasks( UBaseType_t uxPriority )
{

	/* Spawn the task. */
	xTaskCreate( vADCTask, "ADC", ADCSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}


void SeqADCOutputSamples()
{
    int i;
    for(i=0;i<ADCNUM_CHANNELS;i++)
    {
        lastSample[i] = ADC_GetResult16(i);
        usbserial_xprintf("Channel %2d = %6d\r\n",i,lastSample[i]); 
    }
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vADCTask, pvParameters )
{
    TickType_t xSampleRate, xLastSampleTime;
   	/* The parameters are not used. */
	( void ) pvParameters;
    int i = 0;
    
    xSampleRate = 1 / portTICK_PERIOD_MS;
    /* Start the ADC */
    ADC_Start();
    ADC_StartConvert();
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);

    
    xLastSampleTime = xTaskGetTickCount();	
	for(;;)
	{
        for(i=0;i<ADCNUM_CHANNELS;i++)
        {
            lastSample[i] = ADC_GetResult16(i);
        }
        ADC_StartConvert();
        vTaskDelayUntil( &xLastSampleTime, xSampleRate );
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

