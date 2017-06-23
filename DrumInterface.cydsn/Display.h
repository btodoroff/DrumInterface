#ifndef DISPLAY_H
#define DISPLAY_H

#define DISP_INTRO      (0)
#define DISP_STATUS     (1)
#define DISP_TUNING     (2)
    
extern int QUAD_BtnCount;

#define UIEVENT_BTN (1)
#define UIEVENT_QUAD (2)
#define UIEVENT_TRIG (3)

struct UIEvent {
    unsigned char type;
    union {
        struct  {
            unsigned char number;
            unsigned char release;
        } Button;
        struct  {
            int newValue;
            int delta;
        } QUADChange;
        struct  {
            int number;
        } Trigger;
    } data;  
};
extern QueueHandle_t UIEventQueue;
void vStartDisplayTasks( UBaseType_t uxPriority );

#endif

