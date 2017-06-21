#ifndef DISPLAY_H
#define DISPLAY_H

#define DISP_INTRO      (0)
#define DISP_STATUS     (1)
#define DISP_TUNING     (2)
    
void vStartDisplayTasks( UBaseType_t uxPriority );
void DisplayUpdatePage(unsigned int pageNum);

#endif

