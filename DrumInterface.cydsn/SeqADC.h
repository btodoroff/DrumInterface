#ifndef SEQADC_H
#define SEQADC_H
    
#define ADCNUM_CHANNELS     4

void vStartADCTasks( UBaseType_t uxPriority );
void SeqADCOutputSamples();
extern int lastSample[ADCNUM_CHANNELS];
#endif

