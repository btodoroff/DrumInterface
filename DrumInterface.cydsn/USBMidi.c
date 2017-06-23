#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "xformatc.h"
#include "USBSerial.h"
#include "SeqADC.h"

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
    uint8 onMsg[] = {0x99,note,velocity};
    if(0 == USB_GetConfiguration()) return;
    xSemaphoreTake(USBMidiMutex,portMAX_DELAY);
    USB_PutUsbMidiIn(3,onMsg,USB_MIDI_CABLE_00);
    xSemaphoreGive(USBMidiMutex);
}

void usbmidi_noteOff(uint8 note, uint8 velocity)
{
    uint8 offMsg[] = {0x89,note,velocity};
    if(0 == USB_GetConfiguration()) return;
    xSemaphoreTake(USBMidiMutex,portMAX_DELAY);
    USB_PutUsbMidiIn(3,offMsg,USB_MIDI_CABLE_00);
    xSemaphoreGive(USBMidiMutex);
}


void vStartUSBMidiTasks( UBaseType_t uxPriority )
{
    /*Setup the mutex to control port access*/
    USBMidiMutex = xSemaphoreCreateMutex();

	/* Spawn the task. */
	xTaskCreate( vUSBMidiTask, "USBMidi", USBMIDISTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}

void USB_callbackLocalMidiEvent(uint8 cable, uint8* midiMsg)
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
    static struct ADCEvent queueEvent;

    /* Start the USB_UART */
    /* Start USBFS operation with 5-V operation. */
    while(0u == USB_GetConfiguration());
    USB_MIDI_Init();

    
	for(;;)
	{
        /* Host can send double SET_INTERFACE request. */
        if (0u != USB_IsConfigurationChanged())
        {
            /* Initialize IN endpoints when device is configured. */
            if (0u != USB_GetConfiguration())
            {
                /* Enumeration is done, enable OUT endpoint to receive data 
                 * from host. */
                USB_MIDI_Init();
            }
        }
        
        if(0 != USB_GetConfiguration())
        {
            if(pdPASS == xQueueReceive(ADCEventQueue,&queueEvent,0))
            {
                if(queueEvent.type == ADCEVENT_HIT)
                {
                    //usbserial_xprintf("Trigger:%02d @ %03d\r\n",queueEvent.data.hit.triggerNumber,queueEvent.data.hit.velocity);
                    usbmidi_noteOn(Trigger[queueEvent.data.hit.triggerNumber].midiNote,queueEvent.data.hit.velocity);
                }
                else if(queueEvent.type == ADCEVENT_RELEASE)
                {
                    //usbserial_xprintf("Trigger:%02d @ %03d\r\n",queueEvent.data.hit.triggerNumber,queueEvent.data.hit.velocity);
                    usbmidi_noteOff(Trigger[queueEvent.data.hit.triggerNumber].midiNote,0);
                }
            }
            /* Get and process inputs here */
            USB_MIDI_IN_Service();
            USB_MIDI_OUT_Service();
            
        }
        vTaskDelay(1);
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

