#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "xformatc.h"
#include "USBSerial.h"

/* Demo program include files. */
//#include "partest.h"
#include "USBMidi.h"

#define USBMIDISTACK_SIZE		configMINIMAL_STACK_SIZE
const static TickType_t xDelay = 1 / portTICK_PERIOD_MS;
const static TickType_t mDelay = 5000 / portTICK_PERIOD_MS;
    
/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vUSBMidiTask, pvParameters );

SemaphoreHandle_t USBMidiMutex;

/*-----------------------------------------------------------*/


void usbmidi_noteOn(uint8 note, uint8 velocity)
{
    uint8 onMsg[] = {0x9A,note,velocity};
    if(0 == USBUART_GetConfiguration()) return;
    xSemaphoreTake(USBMidiMutex,portMAX_DELAY);
    USBUART_PutUsbMidiIn(3,onMsg,USBUART_MIDI_CABLE_00);
    xSemaphoreGive(USBMidiMutex);
}

void usbmidi_noteOff(uint8 note, uint8 velocity)
{
    uint8 offMsg[] = {0x8A,note,velocity};
    if(0 == USBUART_GetConfiguration()) return;
    xSemaphoreTake(USBMidiMutex,portMAX_DELAY);
    USBUART_PutUsbMidiIn(3,offMsg,USBUART_MIDI_CABLE_00);
    xSemaphoreGive(USBMidiMutex);
}


void vStartUSBMidiTasks( UBaseType_t uxPriority )
{
    /*Setup the mutex to control port access*/
    USBMidiMutex = xSemaphoreCreateMutex();
	/* Spawn the task. */
	xTaskCreate( vUSBMidiTask, "USBMidi", USBMIDISTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}

void USBUART_callbackLocalMidiEvent(uint8 cable, uint8* midiMsg)
{
    usbserial_xprintf("MIDI Event:%02X\r\n",midiMsg[0]);
    //usbserial_putString(strBuf);
    return;
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vUSBMidiTask, pvParameters )
{
   	/* The parameters are not used. */
	( void ) pvParameters;

    /* Start the USB_UART */
    /* Start USBFS operation with 5-V operation. */
    while(0u == USBUART_GetConfiguration());
    USBUART_MIDI_Init();

    
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
                USBUART_MIDI_Init();
            }
        }
        
        if(0 != USBUART_GetConfiguration())
        {
            /* Get and process inputs here */
            USBUART_MIDI_IN_Service();
            USBUART_MIDI_OUT_Service();
            
        }
        vTaskDelay(1);
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

