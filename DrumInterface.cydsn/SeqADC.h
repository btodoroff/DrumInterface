#ifndef SEQADC_H
#define SEQADC_H

#include "queue.h"
#define ADCNUM_CHANNELS     16

void vStartADCTasks( UBaseType_t uxPriority );
void SeqADCOutputSamples();
struct TriggerInfo {
    int lastSample;         //Last value read from ADC channel
    int lastVelocity;       //Last velocity reported for trigger
    int thresholdHigh;      //ADC min level to trigger
    int thresholdLow;       //ADC max level to release
    int active;             //0 = not waiting for release and retrigger
    int retriggerDelay;     //How many sample periods to mask out after trigger
    int retriggerCountdown; //How long left till ready for retrigger
};
#define ADCEVENT_HIT (1)
struct HitInfo {
    unsigned char triggerNumber;
    unsigned char velocity;
};
#define ADCEVENT_CTRL (2)
struct CtrlChange {
    unsigned char ctrlNumber;
    unsigned char newValue;
};
struct ADCEvent {
    unsigned char type;
    union {
        struct HitInfo hit;
        struct CtrlChange ctrl;
    } data;  
};
extern struct TriggerInfo Trigger[ADCNUM_CHANNELS];
extern QueueHandle_t ADCEventQueue;
#endif

