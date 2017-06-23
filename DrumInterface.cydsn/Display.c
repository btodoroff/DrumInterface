#include <stdlib.h>

/* Scheduler include files. */
//#include "project.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "u8g2.h"
#include "xformatc.h"
#include "SeqADC.h"
#include "USBSerial.h"



/* Demo program include files. */
//#include "partest.h"
#include "Display.h"

#define DISPLAYSTACK_SIZE		200
#define UISTACK_SIZE		    configMINIMAL_STACK_SIZE
    
/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vDisplayTask, pvParameters );
static portTASK_FUNCTION_PROTO( vUITask, pvParameters );


int QUAD_BtnCount= 0;
static struct UIEvent isrButtonEvent;
CY_ISR(isr_QUAD_SW_Count)
{
    QUAD_BtnCount+=1;
    xQueueSendToBackFromISR(UIEventQueue,&isrButtonEvent,NULL);
}

SemaphoreHandle_t DisplayMutex;
SemaphoreHandle_t DisplayUpdateSemaphore;
QueueHandle_t UIEventQueue;

/*-----------------------------------------------------------*/

uint8_t u8x8_byte_hw_SPI3(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t *data;
    static union {
        uint8 bytes[2];
        uint16 word;
    } out;
    switch(msg)
    {
        case U8X8_MSG_BYTE_SEND:
            data = (uint8_t *)arg_ptr;

            while( arg_int > 0 )
            {
                out.bytes[0] = *data;
                while(0u == (SPI_TX_STATUS_REG & SPI_STS_TX_FIFO_NOT_FULL)) vTaskDelay(1); //Yield until SPI is free. 1 Tick ~= 1 byte @ 10khz
                (void)SPI_WriteTxData(out.word); // No Error Checking..
  	            data++;
	            arg_int--;
            }
        break;
      
        case U8X8_MSG_BYTE_INIT: // Using the HW block you dont need to set 1/1
        break;
        case U8X8_MSG_BYTE_SET_DC:
            while(!(SPI_ReadTxStatus()&SPI_STS_SPI_IDLE));
            if(arg_int)
                out.bytes[1] = 0x01;
            else
                out.bytes[1] = 0x00;
        break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            //SPI_St
            //(void)I2C_MasterSendStart(u8x8_GetI2CAddress(u8x8)>>1,I2C_WRITE_XFER_MODE); // no Error Checking
         break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            //(void)I2C_MasterSendStop(); // no error checking
        break;
    
        default:
            return 0;
    }
    
    return 1;
}
uint8_t psoc_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;
    switch(msg)
    {

        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(arg_int / portTICK_PERIOD_MS);
        break;
        case U8X8_MSG_DELAY_10MICRO:
            CyDelayUs(10);
        break;
        case U8X8_MSG_DELAY_100NANO:
            CyDelayCycles((BCLK__BUS_CLK__HZ/1000000) * 100 - 16); //PSoC system reference guide says 8-23 extra cycles 
        break;
        case U8X8_MSG_GPIO_RESET:
            if(arg_int == 0)
                CyPins_ClearPin(DISP_RESET_0);
            else
                CyPins_SetPin(DISP_RESET_0);
        break;
        /* Using the HW SPI interface, so we don't need most of this.
         * If you want to use a software interface or have these pins then you 
         * need to read and write them
        case U8X8_MSG_GPIO_AND_DELAY_INIT: 
        case U8X8_MSG_GPIO_CS:
        case U8X8_MSG_GPIO_DC:
        case U8X8_MSG_GPIO_SPI_CLOCK:
        case U8X8_MSG_GPIO_SPI_DATA:		
        case U8X8_MSG_GPIO_D0:    
        case U8X8_MSG_GPIO_D1:			
        case U8X8_MSG_GPIO_D2:		
        case U8X8_MSG_GPIO_D3:		
        case U8X8_MSG_GPIO_D4:	
        case U8X8_MSG_GPIO_D5:	
        case U8X8_MSG_GPIO_D6:	
        case U8X8_MSG_GPIO_D7:
        case U8X8_MSG_GPIO_E: 	
        case U8X8_MSG_GPIO_I2C_CLOCK:
        case U8X8_MSG_GPIO_I2C_DATA:
        break;  */
    }
    return 1;
}


u8g2_t u8g2;
static unsigned int currentPage = DISP_INTRO;
void DisplayUpdatePage(unsigned int pageNum)
{
    currentPage = pageNum;
    xSemaphoreGive(DisplayUpdateSemaphore);
}

static char u8g2Buf[65];
static void u8g2Putchar(void *arg,char c)
{
    char ** s = (char **)arg;
    *(*s)++ = c;
}

void u8g2_xprintf(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y,const char *fmt,...)
{
    char *buf = u8g2Buf;
    va_list list;
    va_start(list,fmt);
    xvformat(u8g2Putchar,(void *)&buf,fmt,list);
    *buf = 0;
    u8g2_DrawStr(u8g2,x,y,u8g2Buf);
    va_end(list);
}


static struct {
    enum UIState {
        NONE,
        STATUS,
        TUNING_SELECT,
        TUNING_EDIT
    } state;
    int menuItem;
    unsigned char curTrigger;
} UI;


void vStartDisplayTasks( UBaseType_t uxPriority )
{
    /*Setup the mutex to control port access*/
    DisplayMutex = xSemaphoreCreateMutex();
    DisplayUpdateSemaphore = xSemaphoreCreateBinary();
    UIEventQueue = xQueueCreate( 8, sizeof(struct UIEvent));

    SPI_Start();
    Quad_Start();
    Quad_SetCounter(0);
    isrButtonEvent.type = UIEVENT_BTN;
    isrButtonEvent.data.Button.number = 0;
    isrButtonEvent.data.Button.release = 1;
    isr_QUAD_SW_ClearPending();
    isr_QUAD_SW_StartEx(isr_QUAD_SW_Count);
    UI.state = NONE;
    UI.menuItem = 0;
    UI.curTrigger = 0;
    
	/* Spawn the task. */
	xTaskCreate( vDisplayTask, "Display",DISPLAYSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );
	xTaskCreate( vUITask, "UI",DISPLAYSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );
}



void drawPage(unsigned int pageNum)
{
    SPI_WriteByte(0x00A1); //Kludge to keep segment mapping from reseting.
    u8g2_FirstPage(&u8g2);
    do
    {
        u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
        u8g2_SetFontPosTop(&u8g2);
        u8g2_SetFontDirection(&u8g2, 0);
        u8g2_SetFontMode(&u8g2, 1);	// Transparent
        switch(pageNum)
        {
            case DISP_INTRO:
                u8g2_DrawStr(&u8g2, 1,0,"Drum Interface");
                u8g2_DrawStr(&u8g2, 1,10,"Version 0.0.0");
                u8g2_DrawStr(&u8g2, 1,30,"Loading...");
            break;
            case DISP_STATUS:
                u8g2_DrawStr(&u8g2, 1,0,"Drum Interface");
                u8g2_DrawStr(&u8g2, 1,10,"Version 0.0.0");
                u8g2_DrawStr(&u8g2, 1,30,"Rockin'");
            break;
            case DISP_TUNING:
                if(UI.state == TUNING_SELECT) {
                    u8g2_SetDrawColor(&u8g2,1); /* color 1 for the box */
                    u8g2_xprintf(&u8g2, 70,55,"Vel:  %3d",Trigger[UI.curTrigger].lastVelocity);
                    u8g2_DrawBox(&u8g2,0,UI.menuItem*10, 60, 10);
                    u8g2_SetDrawColor(&u8g2,UI.menuItem!=0);
                    u8g2_xprintf(&u8g2, 1,0,"Note: %3d",Trigger[UI.curTrigger].midiNote);
                    u8g2_SetDrawColor(&u8g2,UI.menuItem!=1);
                    u8g2_xprintf(&u8g2, 1,10,"Delay:%3d",Trigger[UI.curTrigger].retriggerDelay);
                    u8g2_SetDrawColor(&u8g2,UI.menuItem!=2);
                    u8g2_xprintf(&u8g2, 1,20,"Thr: %4d",Trigger[UI.curTrigger].thresholdHigh);
                    u8g2_SetDrawColor(&u8g2,UI.menuItem!=3);
                    u8g2_xprintf(&u8g2, 1,30,"Rel:  %3d",Trigger[UI.curTrigger].thresholdLow);
                    u8g2_SetFont(&u8g2, u8g2_font_fur42_tn);
                    u8g2_SetFontPosTop(&u8g2);
                    u8g2_SetDrawColor(&u8g2,1);
                    u8g2_xprintf(&u8g2, 60,1,"%02d",UI.curTrigger);
                } else {
                    const int boxOffset[] = { 37,37,25,33 };
                    u8g2_SetDrawColor(&u8g2,1); /* color 1 for the box */
                    u8g2_DrawBox(&u8g2,boxOffset[UI.menuItem],UI.menuItem*10, 60-boxOffset[UI.menuItem], 10);
                    u8g2_xprintf(&u8g2, 70,55,"Vel:  %3d",Trigger[UI.curTrigger].lastVelocity);
                    u8g2_xprintf(&u8g2, 1,0,"Note: %3d",Trigger[UI.curTrigger].midiNote);
                    u8g2_xprintf(&u8g2, 1,10,"Delay:%3d",Trigger[UI.curTrigger].retriggerDelay);
                    u8g2_xprintf(&u8g2, 1,20,"Thr: %4d",Trigger[UI.curTrigger].thresholdHigh);
                    u8g2_xprintf(&u8g2, 1,30,"Rel:  %3d",Trigger[UI.curTrigger].thresholdLow);
                    u8g2_SetDrawColor(&u8g2,0); 
                    switch(UI.menuItem) {
                        case 0:
                            u8g2_xprintf(&u8g2, 1,0,"      %3d",Trigger[UI.curTrigger].midiNote);
                            break;
                        case 1:
                            u8g2_xprintf(&u8g2, 1,10,"      %3d",Trigger[UI.curTrigger].retriggerDelay);
                            break;
                        case 2:
                            u8g2_xprintf(&u8g2, 1,20,"     %4d",Trigger[UI.curTrigger].thresholdHigh);
                            break;
                        case 3:
                            u8g2_xprintf(&u8g2, 1,30,"      %3d",Trigger[UI.curTrigger].thresholdLow);
                            break;
                    }
                    u8g2_SetFont(&u8g2, u8g2_font_fur42_tn);
                    u8g2_SetFontPosTop(&u8g2);
                    u8g2_SetDrawColor(&u8g2,1);
                    u8g2_xprintf(&u8g2, 60,1,"%02d",UI.curTrigger);
                }
            break;
        }
    } while( u8g2_NextPage(&u8g2) );
}


static portTASK_FUNCTION( vUITask, pvParameters )
{
    static struct UIEvent queueEvent;
    int8 lastQuadCount = 0;
    Quad_SetCounter(0);

   	/* The parameters are not used. */
	( void ) pvParameters;
    
	for(;;)
	{
        if(pdPASS == xQueueReceive(UIEventQueue,&queueEvent,50))
        {
            switch(UI.state) {
                case TUNING_SELECT:
                    switch(queueEvent.type) {
                        case UIEVENT_QUAD:
                            UI.menuItem = (UI.menuItem + queueEvent.data.QUADChange.delta + 4) % 4;
                            DisplayUpdatePage(DISP_TUNING);
                            break;
                        case UIEVENT_TRIG:
                            UI.curTrigger = queueEvent.data.Trigger.number;
                            DisplayUpdatePage(DISP_TUNING);
                            break;
                        case UIEVENT_BTN:
                            UI.state = TUNING_EDIT;
                            DisplayUpdatePage(DISP_TUNING);
                            break;
                    }
                    break;
                case TUNING_EDIT:
                    switch(queueEvent.type) {
                        case UIEVENT_QUAD:
                            switch(UI.menuItem) {
                                case 0:
                                    Trigger[UI.curTrigger].midiNote += queueEvent.data.QUADChange.delta;
                                    break;
                                case 1:
                                    Trigger[UI.curTrigger].retriggerDelay += queueEvent.data.QUADChange.delta;
                                    break;
                                case 2:
                                    Trigger[UI.curTrigger].thresholdHigh += queueEvent.data.QUADChange.delta*10;
                                    break;
                                case 3:
                                    Trigger[UI.curTrigger].thresholdLow += queueEvent.data.QUADChange.delta*10;
                                    break;
                            }
                            DisplayUpdatePage(DISP_TUNING);
                            break;
                        case UIEVENT_TRIG:
                            if(UI.curTrigger == queueEvent.data.Trigger.number) {
                                DisplayUpdatePage(DISP_TUNING);
                            }
                            else
                            {
                                UI.curTrigger = queueEvent.data.Trigger.number;
                                UI.state = TUNING_SELECT;
                                DisplayUpdatePage(DISP_TUNING);
                            }
                            break;
                        case UIEVENT_BTN:
                            UI.state = TUNING_SELECT;
                            DisplayUpdatePage(DISP_TUNING);
                            break;
                    }
                    break;
                case STATUS:
                    switch(queueEvent.type) {
                        case UIEVENT_BTN:
                            UI.state = TUNING_SELECT;
                            DisplayUpdatePage(DISP_TUNING);
                            break;
                    }
                    break;
                default:
                    UI.state=STATUS;
                    DisplayUpdatePage(DISP_STATUS);
                    break;
            }
        }
        if(Quad_GetCounter() != lastQuadCount)
        {
            queueEvent.type = UIEVENT_QUAD;
            queueEvent.data.QUADChange.newValue = Quad_GetCounter();
            queueEvent.data.QUADChange.delta = queueEvent.data.QUADChange.newValue - lastQuadCount;
            if(pdTRUE == xQueueSend(UIEventQueue,&queueEvent,0)) lastQuadCount = queueEvent.data.QUADChange.newValue;
        }
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */


/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vDisplayTask, pvParameters )
{
   	/* The parameters are not used. */
	( void ) pvParameters;
    
    u8g2_Setup_ssd1306_128x64_noname_1(&u8g2, U8G2_R0, u8x8_byte_hw_SPI3, psoc_gpio_and_delay_cb);
    u8g2_InitDisplay(&u8g2);  
    u8g2_ClearDisplay(&u8g2);    
    //vTaskDelay(10);
    drawPage(DISP_INTRO);
    u8g2_SetPowerSave(&u8g2,0);
    xQueueSendToBack(UIEventQueue,&u8g2,0);
	for(;;)
	{
        xSemaphoreTake(DisplayUpdateSemaphore,portMAX_DELAY);
        drawPage(currentPage);
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

