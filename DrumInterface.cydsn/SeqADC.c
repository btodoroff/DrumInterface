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
//#define ADCSTACK_SIZE		configMINIMAL_STACK_SIZE
//const static TickType_t xDelay = 1 / portTICK_PERIOD_MS;
//const static TickType_t mDelay = 5000 / portTICK_PERIOD_MS;
    
/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vADCTask, pvParameters );
struct TriggerInfo Trigger[ADCNUM_CHANNELS];
 // xQueueBuffer will hold the queue structure.
//StaticQueue_t xQueueBuffer;
//uint8_t ucQueueStorage[ 16 * sizeof(struct ADCEvent) ];
QueueHandle_t ADCEventQueue;
/*-----------------------------------------------------------*/

void vStartADCTasks( UBaseType_t uxPriority )
{
    //xQueueCreateStatic( 16, sizeof(struct ADCEvent), &( ucQueueStorage[ 0 ] ),&xQueueBuffer);
    ADCEventQueue = xQueueCreate( 16, sizeof(struct ADCEvent));

	/* Spawn the task. */
	xTaskCreate( vADCTask, "ADC", ADCSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}


void SeqADCOutputSamples()
{
    int i;
    for(i=0;i<ADCNUM_CHANNELS;i++)
    {
        usbserial_xprintf("Channel %2d = %6d\r\n",i,Trigger[i].lastSample); 
    }
}

void resetTrigger(int i)
{
    return;
    /*switch(i)
    {
        case 0:
            CyPins_ClearPin(TRIGGER_0);
            CyPins_SetPin(TRIGGER_0);
            break;
        case 1:
            CyPins_ClearPin(TRIGGER_1);
            CyPins_SetPin(TRIGGER_1);
            break;
        case 2:
            CyPins_ClearPin(TRIGGER_2);
            CyPins_SetPin(TRIGGER_2);
            break;
        case 3:
            CyPins_ClearPin(TRIGGER_3);
            CyPins_SetPin(TRIGGER_3);
            break;
        case 4:
            CyPins_ClearPin(TRIGGER_4);
            CyPins_SetPin(TRIGGER_4);
            break;
        case 5:
            CyPins_ClearPin(TRIGGER_5);
            CyPins_SetPin(TRIGGER_5);
            break;
        case 6:
            CyPins_ClearPin(TRIGGER_6);
            CyPins_SetPin(TRIGGER_6);
            break;
        case 7:
            CyPins_ClearPin(TRIGGER_7);
            CyPins_SetPin(TRIGGER_7);
            break;
        case 8:
            CyPins_ClearPin(TRIGGER_8);
            CyPins_SetPin(TRIGGER_8);
            break;
        case 9:
            CyPins_ClearPin(TRIGGER_9);
            CyPins_SetPin(TRIGGER_9);
            break;
        case 10:
            CyPins_ClearPin(TRIGGER_10);
            CyPins_SetPin(TRIGGER_10);
            break;
        case 11:
            CyPins_ClearPin(TRIGGER_11);
            CyPins_SetPin(TRIGGER_11);
            break;
        case 12:
            CyPins_ClearPin(TRIGGER_12);
            CyPins_SetPin(TRIGGER_12);
            break;
        case 13:
            CyPins_ClearPin(TRIGGER_13);
            CyPins_SetPin(TRIGGER_13);
            break;
        case 14:
            CyPins_ClearPin(TRIGGER_14);
            CyPins_SetPin(TRIGGER_14);
            break;
        case 15:
            CyPins_ClearPin(TRIGGER_15);
            CyPins_SetPin(TRIGGER_15);
            break;    }*/
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vADCTask, pvParameters )
{
    TickType_t xSampleRate, xLastSampleTime;
   	/* The parameters are not used. */
	( void ) pvParameters;
    int i = 0;
    int thisSample;
    static struct ADCEvent event;
    
    xSampleRate = 1 / portTICK_PERIOD_MS;
    for(i=0;i<ADCNUM_CHANNELS;i++)
    {
        Trigger[i].retriggerCountdown = 0;
        Trigger[i].retriggerDelay = 5;
        Trigger[i].active = 0;
        Trigger[i].thresholdHigh = 2000;
        Trigger[i].thresholdLow = 5;
    }
    /* Start the ADC */
    ADC_Start();
    ADC_StartConvert();
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);

    
    xLastSampleTime = xTaskGetTickCount();	
	for(;;)
	{
        ADC_StartConvert();
        ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
        for(i=0;i<ADCNUM_CHANNELS;i++)
        {
            thisSample = ADC_GetResult16(i);
            if(!Trigger[i].active && thisSample >= Trigger[i].thresholdHigh && thisSample < Trigger[i].lastSample)
            {
                //Not already active, 
                //Sample is high enough to trigger,
                //Signal is falling, so we've past peak and can measuer velocity
                Trigger[i].active = 1;
                Trigger[i].retriggerCountdown = Trigger[i].retriggerDelay;
                event.type = ADCEVENT_HIT;
                event.data.hit.triggerNumber = i;
                event.data.hit.velocity = thisSample >> 4;
                Trigger[i].lastVelocity = event.data.hit.velocity;
                if(errQUEUE_FULL == xQueueSend(ADCEventQueue, &event, 0))
                    usbserial_putString("Hit queue overflow\r\n");
                resetTrigger(i);

            }
            else if(Trigger[i].active && thisSample >= Trigger[i].thresholdLow)
            {
                //We are wating for the signal to stay low and avoid retriggers
                resetTrigger(i);
                Trigger[i].retriggerCountdown = Trigger[i].retriggerDelay;
                usbserial_xprintf("%d @ %d\r\n",Trigger[i].retriggerCountdown,thisSample);

            }
            else if(Trigger[i].active && --Trigger[i].retriggerCountdown <= 0) //sample < thresholdLow
            {
                //Ok, we can start looking for another hit!
                Trigger[i].active = 0;
                usbserial_putString("Release\r\n");
            }
            else if(Trigger[i].active)
            {
                usbserial_xprintf("%d @ %4d\r\n",Trigger[i].retriggerCountdown,thisSample);
            }
            Trigger[i].lastSample = thisSample;
        }
        vTaskDelayUntil( &xLastSampleTime, xSampleRate );
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

