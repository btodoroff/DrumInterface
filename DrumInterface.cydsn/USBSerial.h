#ifndef USBSERIAL_H
#define USBSERIAL_H

void vStartUSBSerialTasks( UBaseType_t uxPriority );
void usbserial_putString(const char msg[]);
void usbserial_xprintf(const char *fmt,...);
void serial_xprintf(const char *fmt,...);

#endif

