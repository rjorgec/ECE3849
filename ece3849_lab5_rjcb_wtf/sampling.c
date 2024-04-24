/*
 * sampling.c
 * created 3/21/2024 by Ricardo Croes-Ball and Will-i-am Folan
 * >:(
 */
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "sysctl_pll.h"
#include "driverlib/adc.h"
#include "inc/tm4c1294ncpdt.h"
#include "buttons.h"
#include "sampling.h"
#include "Crystalfontz128x128_ST7735.h"

#include "driverlib/udma.h"
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/gates/GateHwi.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>
#pragma DATA_ALIGN(gDMAControlTable, 1024) // address alignment required
tDMAControlTable gDMAControlTable[64]; // uDMA control table (global)

// latest sample index
//volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1;
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile uint32_t gADCErrors = 0; // number of missed ADC deadlines
volatile bool TrigFound= false;


int32_t getADCBufferIndex(void);

void ADCInit(void){
//all the ADC1 stuff
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0); // GPIO setup for analog input AIN3
  // initialize ADC peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
  // ADC clock
    uint32_t pll_frequency = SysCtlFrequencyGet(CRYSTAL_FREQUENCY);
    uint32_t pll_divisor = (pll_frequency - 1) / (16 * ADC_SAMPLING_RATE) + 1; // round up
  ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
  ADCClockConfigSet(ADC1_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
  // choose ADC1 sequence 0; disable before configuring
  ADCSequenceDisable(ADC1_BASE, 0);

  ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger

  // in the 0th step, sample channel 3 (AIN3)
  // enable interrupt, and make it the end of sequence
  ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END); //Not sure if CH3 is correct, IF SOMETHING DOESNT WORK LOOK HERE

  // enable the sequence. it is now sampling
  ADCSequenceEnable(ADC1_BASE, 0);

  //DMA ENABLE
  ADCSequenceDMAEnable(ADC1_BASE, 0); // enable DMA for ADC1 sequence 0
  ADCIntEnableEx(ADC1_BASE, ADC_INT_DMA_SS0); // enable ADC1 sequence 0 DMA interrupt  //IDFK IF THIS IS RIGHT

  // enable sequence 0 interrupt in the ADC1 peripheral
//  ADCIntEnable(ADC1_BASE, 0);
//  IntPrioritySet(INT_ADC1SS0, 0x00); // set ADC1 sequence 0 interrupt priority
  // enable ADC1 sequence 0 interrupt in int. controller
//  IntEnable(INT_ADC1SS0);
}


int RisingTrigger(void){ // search for rising edge trigger
    // Step 1
    TrigFound = true;
    int x = getADCBufferIndex() - LCD_HORIZONTAL_MAX / 2; // half screen width; don’t use a magic number
    // Step 2
    int x_stop = x - ADC_BUFFER_SIZE / 2;
    for (; x > x_stop; x--) {
    if (gADCBuffer[ADC_BUFFER_WRAP(x)] >= ADC_OFFSET && gADCBuffer[ADC_BUFFER_WRAP(x - 1)] < ADC_OFFSET)
        break;
    }
    // Step 3
    if (x == x_stop) // for loop ran to the end
        x = getADCBufferIndex() - LCD_HORIZONTAL_MAX / 2; // reset x back to how it was initialized
    TrigFound = false;
    return x;
}

int FallingTrigger(void){ // search for rising edge trigger
    // Step 1
    TrigFound = true;
    int x = getADCBufferIndex() - LCD_HORIZONTAL_MAX / 2; // half screen width; don’t use a magic number
    // Step 2
    int x_stop = x - ADC_BUFFER_SIZE / 2;
    for (; x > x_stop; x--) {
    if (gADCBuffer[ADC_BUFFER_WRAP(x)] < ADC_OFFSET && gADCBuffer[ADC_BUFFER_WRAP(x - 1)] >= ADC_OFFSET)
        break;
    }
    // Step 3
    if (x == x_stop) // for loop ran to the end
        x = getADCBufferIndex() - LCD_HORIZONTAL_MAX / 2; // reset x back to how it was initialized
        TrigFound = false;
    return x;
}


//void ADC_ISR(void){
//    // clear ADC1 sequence0 interrupt flag in the ADCISC register
//    ADC1_ISC_R = ADC_ISC_IN0;
//    // check for ADC FIFO overflow
//    if(ADC1_OSTAT_R & ADC_OSTAT_OV0) {
//        gADCErrors++; // count errors
//        ADC1_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
//    }
//    gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1);
//    // read sample from the ADC1 sequence 0 FIFO
//    gADCBuffer[gADCBufferIndex] = ADC1_SSFIFO0_R;
////    return gADCBuffer;
//}

// is DMA occurring in the primary channel?
volatile bool gDMAPrimary = true;

void ADC_ISR(void) // DMA
{
    ADCIntClearEx(ADC1_BASE, ADC_INT_DMA_SS0); // clear the ADC1 sequence 0 DMA interrupt flag
    // Check the primary DMA channel for end of transfer, and
    // restart if needed.
    if (uDMAChannelModeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT) == UDMA_MODE_STOP) {
        // restart the primary channel (same as setup)
        uDMAChannelTransferSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_PRI_SELECT, UDMA_MODE_PINGPONG, (void*)&ADC1_SSFIFO0_R, (void*)&gADCBuffer[0], ADC_BUFFER_SIZE/2);
        // DMA is currently occurring in the alternate buffer
        gDMAPrimary = false;
    }
    // Check the alternate DMA channel for end of transfer, and
    // restart if needed.
    // Also set the gDMAPrimary global.
    if(uDMAChannelModeGet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT) == UDMA_MODE_STOP){
            uDMAChannelTransferSet(UDMA_SEC_CHANNEL_ADC10 | UDMA_ALT_SELECT, UDMA_MODE_PINGPONG, (void*)&ADC1_SSFIFO0_R, (void*)&gADCBuffer[ADC_BUFFER_SIZE/2], ADC_BUFFER_SIZE/2);
            gDMAPrimary = true;
        }

    // The DMA channel may be disabled if the CPU is paused by the debugger
    if (!uDMAChannelIsEnabled(UDMA_SEC_CHANNEL_ADC10)) {
        uDMAChannelEnable(UDMA_SEC_CHANNEL_ADC10);
    // re-enable the DMA channel
    }
}
uint16_t ADCSAMPLER(int i){
    return gADCBuffer[i];
}

int32_t getADCBufferIndex(void)
{
    int32_t index;

    IArg keyGateHwi0;
    keyGateHwi0 = GateHwi_enter(gateHwi0);

    if (gDMAPrimary) { // DMA is currently in the primary channel
        index = ADC_BUFFER_SIZE/2 - 1 -
        uDMAChannelSizeGet(UDMA_SEC_CHANNEL_ADC10 |
        UDMA_PRI_SELECT);
    } else { // DMA is currently in the alternate channel
        index = ADC_BUFFER_SIZE - 1 -
        uDMAChannelSizeGet(UDMA_SEC_CHANNEL_ADC10 |
        UDMA_ALT_SELECT);
    }

    GateHwi_leave(gateHwi0, keyGateHwi0);
    return index;
}
