#include <stdlib.h>

/* Scheduler include files. */
//#include "project.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "USBSerial.h"
#include "u8g2.h"

/* Demo program include files. */
//#include "partest.h"
#include "Display.h"

#define DISPLAYSTACK_SIZE		configMINIMAL_STACK_SIZE
    
/* The task that is created three times. */
static portTASK_FUNCTION_PROTO( vDisplayTask, pvParameters );

SemaphoreHandle_t DisplayMutex;
SemaphoreHandle_t DisplayUpdateSemaphore;

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

void drawPage(unsigned int pageNum)
{
    SPI_WriteByte(0x00A1); //Kludge to keep segment mapping from reseting.
    u8g2_FirstPage(&u8g2);
    do
    {
        u8g2_SetFontPosTop(&u8g2);
        u8g2_SetFontMode(&u8g2, 1);	// Transparent
        u8g2_SetFontDirection(&u8g2, 0);
        u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
        switch(pageNum)
        {
            case DISP_INTRO:
                u8g2_DrawStr(&u8g2, 1,1,"Drum Interface");
                u8g2_DrawStr(&u8g2, 1,10,"Version 0.0.0");
                u8g2_DrawStr(&u8g2, 1,30,"Loading...");
            break;
            case DISP_STATUS:
                u8g2_DrawStr(&u8g2, 1,1,"Drum Interface");
                u8g2_DrawStr(&u8g2, 1,10,"Version 0.0.0");
                u8g2_DrawStr(&u8g2, 1,30,"Rockin'");
            break;
            case DISP_TUNING:
            break;
        }
    } while( u8g2_NextPage(&u8g2) );
}

void vStartDisplayTasks( UBaseType_t uxPriority )
{
    /*Setup the mutex to control port access*/
    DisplayMutex = xSemaphoreCreateMutex();
    DisplayUpdateSemaphore = xSemaphoreCreateBinary();
    SPI_Start();
    
	/* Spawn the task. */
	xTaskCreate( vDisplayTask, "Display",DISPLAYSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}

/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vDisplayTask, pvParameters )
{
   	/* The parameters are not used. */
	( void ) pvParameters;
    
    u8g2_Setup_ssd1306_128x64_noname_1(&u8g2, U8G2_R0, u8x8_byte_hw_SPI3, psoc_gpio_and_delay_cb);
    u8g2_InitDisplay(&u8g2);  
    u8g2_ClearDisplay(&u8g2);    
    vTaskDelay(10);
    DisplayUpdatePage(DISP_STATUS);
    drawPage(currentPage);
    drawPage(currentPage);
    u8g2_SetPowerSave(&u8g2,0);

	for(;;)
	{
        xSemaphoreTake(DisplayUpdateSemaphore,portMAX_DELAY);
        drawPage(currentPage);
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

