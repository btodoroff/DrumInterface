#include <device.h>

/* RTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Common Demo includes. */
#include "flash.h"
#include "partest.h"
#include "xformatc.h"
#include "USBSerial.h"
#include "USBMidi.h"
#include "SeqADC.h"
#include "Display.h"

/*---------------------------------------------------------------------------*/

/* The time between cycles of the 'check' functionality (defined within the
tick hook. */
#define mainCHECK_DELAY						( ( TickType_t ) 5000 / portTICK_PERIOD_MS )
#define mainCOM_LED							( 3 )

/* The number of nano seconds between each processor clock. */
#define mainNS_PER_CLOCK ( ( unsigned long ) ( ( 1.0 / ( double ) configCPU_CLOCK_HZ ) * 1000000000.0 ) )

/* Task priorities. */
//#define mainQUEUE_POLL_PRIORITY			( tskIDLE_PRIORITY + 2 )
#define mainCHECK_TASK_PRIORITY				( tskIDLE_PRIORITY + 3 )
//#define mainSEM_TEST_PRIORITY				( tskIDLE_PRIORITY + 1 )
//#define mainBLOCK_Q_PRIORITY				( tskIDLE_PRIORITY + 2 )
//#define mainCREATOR_TASK_PRIORITY         ( tskIDLE_PRIORITY + 3 )
//#define mainINTEGER_TASK_PRIORITY         ( tskIDLE_PRIORITY )
//#define mainGEN_QUEUE_TASK_PRIORITY		( tskIDLE_PRIORITY )
//#define mainCOM_TEST_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define mainFLASH_TEST_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define mainUSBSERIAL_TASK_PRIORITY	        ( tskIDLE_PRIORITY + 2 )
#define mainUSBMIDI_TASK_PRIORITY	        ( tskIDLE_PRIORITY + 1 )
#define mainADC_TASK_PRIORITY               ( tskIDLE_PRIORITY + 1 )
#define mainDISPLAY_TASK_PRIORITY           ( tskIDLE_PRIORITY + 3 )
/*---------------------------------------------------------------------------*/

/*
 * Configures the timers and interrupts for the fast interrupt test as
 * described at the top of this file.
 */
extern void vSetupTimerTest( void );
/*---------------------------------------------------------------------------*/

/*
 * The Check task periodical interrogates each of the running tests to
 * ensure that they are still executing correctly.
 * If all the tests pass, then the LCD is updated with Pass, the number of
 * iterations and the Jitter time calculated but the Fast Interrupt Test.
 * If any one of the tests fail, it is indicated with an error code printed on
 * the display. This indicator won't disappear until the device is reset.
 */
void vCheckTask( void *pvParameters );

/*
 * Installs the RTOS interrupt handlers and starts the peripherals.
 */
static void prvHardwareSetup( void );

/*---------------------------------------------------------------------------*/

int main( void )
{
    /* Place your initialization/startup code here (e.g. MyInst_Start()) */
	prvHardwareSetup();
 
	vStartLEDFlashTasks( mainFLASH_TEST_TASK_PRIORITY );
    vStartUSBSerialTasks( mainUSBSERIAL_TASK_PRIORITY );
    vStartADCTasks( mainADC_TASK_PRIORITY);
    vStartUSBMidiTasks( mainUSBMIDI_TASK_PRIORITY);
    vStartDisplayTasks( mainDISPLAY_TASK_PRIORITY);
	//vAltStartComTestTasks( mainCOM_TEST_TASK_PRIORITY, 57600, mainCOM_LED );
	//vStartInterruptQueueTasks();

	/* Start the error checking task. */
  	( void ) xTaskCreate( vCheckTask, "Check", 1000, NULL, mainCHECK_TASK_PRIORITY, NULL );

	vTaskStartScheduler();
	/* Will only get here if there was insufficient memory to create the idle
    task.  The idle task is created within vTaskStartScheduler(). */

	/* Should never reach here as the kernel will now be running.  If
	vTaskStartScheduler() does return then it is very likely that there was
	insufficient (FreeRTOS) heap space available to create all the tasks,
	including the idle task that is created within vTaskStartScheduler() itself. */
	for( ;; );
}
/*---------------------------------------------------------------------------*/

void prvHardwareSetup( void )
{
/* Port layer functions that need to be copied into the vector table. */
extern void xPortPendSVHandler( void );
extern void xPortSysTickHandler( void );
extern void vPortSVCHandler( void );
extern cyisraddress CyRamVectors[];

	/* Install the OS Interrupt Handlers. */
	CyRamVectors[ 11 ] = ( cyisraddress ) vPortSVCHandler;
	CyRamVectors[ 14 ] = ( cyisraddress ) xPortPendSVHandler;
	CyRamVectors[ 15 ] = ( cyisraddress ) xPortSysTickHandler;

    
    USB_Start(0, USB_5V_OPERATION);

	/* Start the UART. */
	UART_1_Start();

	/* Initialise the LEDs. */
	vParTestInitialise();

	/* Start the PWM modules that drive the IntQueue tests. */
	//High_Frequency_PWM_0_Start();
	//High_Frequency_PWM_1_Start();

	/* Start the timers for the Jitter test. */
	//Timer_20KHz_Start();
	//Timer_48MHz_Start();
}
/*---------------------------------------------------------------------------*/

/*static void myPutchar(void *arg,char c)
{
    char ** s = (char **)arg;
    *(*s)++ = c;
}

static void xsprintf(char *buf,const char *fmt,...)
{
    va_list list;
    va_start(list,fmt);
    xvformat(myPutchar,(void *)&buf,fmt,list);
    *buf = 0;
    va_end(list);
}*/

void vCheckTask( void *pvParameters )
{
    TickType_t xDelay = 0;
    //unsigned short usErrorCode = 0;
    int lastBtnCount = 0;
    int lastQuadCount = 0;
    int tickCounter = 0;
    //extern unsigned short usMaxJitter;

	/* Intialise the sleeper. */
	xDelay = xTaskGetTickCount();

	for( ;; )
	{
		/* Perform this check every mainCHECK_DELAY milliseconds. */
		//vTaskDelayUntil( &xDelay, mainCHECK_DELAY );
        vTaskDelay(100);
        if(lastBtnCount != QUAD_BtnCount)
        {
            lastBtnCount = QUAD_BtnCount;
            if(lastBtnCount&0x01)
                DisplayUpdatePage(DISP_STATUS);
            else
                DisplayUpdatePage(DISP_TUNING);
        }
        if(lastQuadCount != Quad_GetCounter())
        {
            lastQuadCount = Quad_GetCounter();
            if(!(lastBtnCount&0x01))
                DisplayUpdatePage(DISP_TUNING);
        }
        

        /*xsprintf(strbuf,"Tick #%d\r\n",ulIteration);
            usbserial_putString(strbuf);
        if(ulIteration&0x1)
            usbmidi_noteOn(64,Trigger[2].lastSample>>4);
        else
            usbmidi_noteOff(64,Trigger[3].lastSample>>4);*/
        if(tickCounter++<50)
        {
            tickCounter = 0;
            SeqADCOutputSamples();
            usbserial_xprintf("Decoder: %d   Btn: %d\r\n",Quad_GetCounter(),QUAD_BtnCount);
        }        
	}
}
/*---------------------------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	/* The stack space has been execeeded for a task, considering allocating more. */
	taskDISABLE_INTERRUPTS();
}/*---------------------------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The heap space has been execeeded. */
	taskDISABLE_INTERRUPTS();
}
/*---------------------------------------------------------------------------*/
