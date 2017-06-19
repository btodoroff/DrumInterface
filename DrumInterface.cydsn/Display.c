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

//uint8_t u8x8_byte_hw_SPI4(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
//{
//    #ifdef DEBUG_UART
//    char buff[20];
//    #endif
//    uint8_t *data;
//    static int DC = 1;
//    switch(msg)
//    {
//        case U8X8_MSG_BYTE_SEND:
//            data = (uint8_t *)arg_ptr;
//            while( arg_int > 0 )
//            {
//                #ifdef DEBUG_UART
//                sprintf(buff,"%x\n",*data);
//                UART_UartPutString(buff);
//                #endif
//                (void)SPI_WriteTxData(*data); // No Error Checking..
//  	            data++;
//	            arg_int--;
//            }
//            while((SPI_ReadTxStatus()&SPI_STS_SPI_IDLE));
//
//        break;
//      
//        case U8X8_MSG_BYTE_INIT: // Using the HW block you dont need to set 1/1
//        break;
//        case U8X8_MSG_BYTE_SET_DC:
//            if(arg_int != DC)
//            {
//                while(!(SPI_ReadTxStatus()&SPI_STS_SPI_IDLE));
//                DC = arg_int;
//                if(arg_int)
//                    CyPins_SetPin(DISP_D_NC_0);
//                else
//                    CyPins_ClearPin(DISP_D_NC_0);
//            }
//        break;
//        case U8X8_MSG_BYTE_START_TRANSFER:
//            //SPI_St
//            //(void)I2C_MasterSendStart(u8x8_GetI2CAddress(u8x8)>>1,I2C_WRITE_XFER_MODE); // no Error Checking
//         break;
//        case U8X8_MSG_BYTE_END_TRANSFER:
//            //(void)I2C_MasterSendStop(); // no error checking
//        break;
//    
//        default:
//            return 0;
//    }
//    
//    return 1;
//}

uint8_t u8x8_byte_hw_SPI3(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    #ifdef DEBUG_UART
    char buff[20];
    #endif
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
                #ifdef DEBUG_UART
                sprintf(buff,"%x\n",*data);
                UART_UartPutString(buff);
                #endif
                out.bytes[0] = *data;
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
        case U8X8_MSG_GPIO_AND_DELAY_INIT: // for now who cares?
        break;
        case U8X8_MSG_DELAY_MILLI:
            //CyDelay(arg_int);
            vTaskDelay(arg_int / portTICK_PERIOD_MS);
        break;
        case U8X8_MSG_DELAY_10MICRO:
            CyDelayUs(10);
        break;
        case U8X8_MSG_DELAY_100NANO:
            CyDelayCycles((BCLK__BUS_CLK__HZ/1000000) * 100 - 16); //PSoC system reference guide says ~16 extra cycles 
        break;
        case U8X8_MSG_GPIO_RESET:
            if(arg_int == 0)
                CyPins_ClearPin(DISP_RESET_0);
            else
                CyPins_SetPin(DISP_RESET_0);
        break;
        /*    - My Display has only I2C... which I have implemented in HW
         * If you want to use a software interface or have these pins then you 
         * need to read and write them
        case U8X8_MSG_GPIO_SPI_CLOCK:
        case U8X8_MSG_GPIO_SPI_DATA:
        case U8X8_MSG_GPIO_CS:
        case U8X8_MSG_GPIO_DC:
        case U8X8_MSG_GPIO_RESET:
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
void vStartDisplayTasks( UBaseType_t uxPriority )
{
    /*Setup the mutex to control port access*/
    DisplayMutex = xSemaphoreCreateMutex();
    DisplayUpdateSemaphore = xSemaphoreCreateBinary();
    
    SPI_Start();
//    //Set up charge pump
//    SPI_WriteByte(0x00ae); 
//    SPI_WriteByte(0x008d); 
//    SPI_WriteByte(0x0014); 
//    //Display On
//    SPI_WriteByte(0x00af); 
//    //All on regardless of RAM
//    SPI_WriteByte(0x00a5);
//    //All on RAM
//    SPI_WriteByte(0x00a4);
//    //All on regardless of RAM
//    //SPI_WriteByte(0x00a5);
    
    
    
    
    
    
    //usbserial_putString("Calling Setup\r\n");
    u8g2_Setup_ssd1306_128x64_noname_1(&u8g2, U8G2_R0, u8x8_byte_hw_SPI3, psoc_gpio_and_delay_cb);
    //u8x8_Setup(&u8x8, u8x8_d_ssd1306_128x64_noname, u8x8_cad_ssd13xx_i2c, u8x8_byte_hw_i2c, psoc_gpio_and_delay_cb);

    
    //usbserial_putString("Calling Init\r\n");
    u8g2_InitDisplay(&u8g2);  
    u8g2_SetPowerSave(&u8g2,0);


    //usbserial_putString("Calling ClearDisplay\n");        
    u8g2_ClearDisplay(&u8g2);    
    
    u8g2_FirstPage(&u8g2);
    do
    {
        u8g2_SetFontMode(&u8g2, 1);	// Transparent
        u8g2_SetFontDirection(&u8g2, 0);
        /*u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
        u8g2_DrawStr(&u8g2, 0, 30, "U");
        
        u8g2_SetFontDirection(&u8g2, 1);
        u8g2_SetFont(&u8g2, u8g2_font_inb30_mn);
        u8g2_DrawStr(&u8g2, 21,8,"8");
            
        u8g2_SetFontDirection(&u8g2, 0);
        u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
        u8g2_DrawStr(&u8g2, 51,30,"g");
        u8g2_DrawStr(&u8g2, 67,30,"\xb2");*/
        
        //u8g2_DrawHLine(&u8g2, 2, 35, 47);
        //u8g2_DrawHLine(&u8g2, 3, 36, 47);
        //u8g2_DrawVLine(&u8g2, 45, 32, 12);
        //u8g2_DrawVLine(&u8g2, 46, 33, 12); 
      
        u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
        u8g2_DrawStr(&u8g2, 1,54,"TODOROFF");
    } while( u8g2_NextPage(&u8g2) );
    
    xSemaphoreGive(DisplayUpdateSemaphore);
	/* Spawn the task. */
	xTaskCreate( vDisplayTask, "Display",DISPLAYSTACK_SIZE, NULL, uxPriority, ( TaskHandle_t * ) NULL );

}

/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vDisplayTask, pvParameters )
{
   	/* The parameters are not used. */
	( void ) pvParameters;

    //u8x8_t u8x8;



    xSemaphoreTake(DisplayUpdateSemaphore,portMAX_DELAY);
    
    u8g2_FirstPage(&u8g2);
    do
    {
        u8g2_SetFontMode(&u8g2, 1);	// Transparent
        u8g2_SetFontDirection(&u8g2, 0);
        /*u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
        u8g2_DrawStr(&u8g2, 0, 30, "U");
        
        u8g2_SetFontDirection(&u8g2, 1);
        u8g2_SetFont(&u8g2, u8g2_font_inb30_mn);
        u8g2_DrawStr(&u8g2, 21,8,"8");
            
        u8g2_SetFontDirection(&u8g2, 0);
        u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
        u8g2_DrawStr(&u8g2, 51,30,"g");
        u8g2_DrawStr(&u8g2, 67,30,"\xb2");*/
        
        //u8g2_DrawHLine(&u8g2, 2, 35, 47);
        //u8g2_DrawHLine(&u8g2, 3, 36, 47);
        //u8g2_DrawVLine(&u8g2, 45, 32, 12);
        //u8g2_DrawVLine(&u8g2, 46, 33, 12); 
      
        u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
        u8g2_DrawStr(&u8g2, 1,54,"github.com/olikraus/u8g2");
    } while( u8g2_NextPage(&u8g2) );
       
    xSemaphoreTake(DisplayUpdateSemaphore,portMAX_DELAY);
  
	for(;;)
	{
        u8g2_FirstPage(&u8g2);
        do
        {
            u8g2_SetFontMode(&u8g2, 1);	// Transparent
            u8g2_SetFontDirection(&u8g2, 0);
            /*u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
            u8g2_DrawStr(&u8g2, 0, 30, "U");
            
            u8g2_SetFontDirection(&u8g2, 1);
            u8g2_SetFont(&u8g2, u8g2_font_inb30_mn);
            u8g2_DrawStr(&u8g2, 21,8,"8");
                
            u8g2_SetFontDirection(&u8g2, 0);
            u8g2_SetFont(&u8g2, u8g2_font_inb24_mf);
            u8g2_DrawStr(&u8g2, 51,30,"g");
            u8g2_DrawStr(&u8g2, 67,30,"\xb2");*/
            
            //u8g2_DrawHLine(&u8g2, 2, 35, 47);
            //u8g2_DrawHLine(&u8g2, 3, 36, 47);
            //u8g2_DrawVLine(&u8g2, 45, 32, 12);
            //u8g2_DrawVLine(&u8g2, 46, 33, 12); 
          
            u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
            u8g2_DrawStr(&u8g2, 1,10,"Semaphore");
        } while( u8g2_NextPage(&u8g2) );        
        vTaskDelay(100);
	}
} /*lint !e715 !e818 !e830 Function definition must be standard for task creation. */

