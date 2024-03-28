
#ifndef SAMPLING_H_
#define SAMPLING_H_

#define ADC_BUFFER_SIZE 2048 // size must be a power of 2
// index wrapping macro
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1))
// latest sample index
extern volatile int32_t gADCBufferIndex;
extern volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
extern volatile uint32_t gADCErrors; // number of missed ADC deadlines

#endif
