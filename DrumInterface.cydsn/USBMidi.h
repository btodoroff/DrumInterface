#ifndef USBMIDI_H
#define USBMIDI_H

void vStartUSBMidiTasks( UBaseType_t uxPriority );
void usbmidi_noteOn(uint8 note, uint8 velocity);
void usbmidi_noteOff(uint8 note, uint8 velocity);

#endif

