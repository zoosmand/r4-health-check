#include "sci_uart.h"

#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>

/* The SCI baud-rate generator is clocked at 48 MHz on the UNO R4 WiFi.
   This was also verified on hardware: calculating BRR from 24 MHz produced
   exactly twice the requested line rate. */
#define SCI_UART_CLOCK_HZ (48000000U)

/* PRCR unlock key. PRC1 gates the module-stop registers (MSTPCRB). */
#define SCI_UART_PRCR_KEY  (0xA500U)
#define SCI_UART_PRCR_PRC1 (1U << 1)

static void sciUart_ConfigurePins(const SciUart_Instance * instance)
{
    /* Unlock the PFS registers: clear BOWI, then set PFSWE. */
    R_PMISC->PWPR = 0;
    R_PMISC->PWPR = 1U << 6;

    /* Per the RA hardware manual, PSEL must be written while PMR is still 0;
       only then is PMR set to 1 to switch the pin over to the peripheral. */
    R_PFS->PORT[instance->portNum].PIN[instance->txPinNum].PmnPFS_b.PSEL = instance->pselGroup;
    R_PFS->PORT[instance->portNum].PIN[instance->txPinNum].PmnPFS_b.PMR = 1;

    R_PFS->PORT[instance->portNum].PIN[instance->rxPinNum].PmnPFS_b.PSEL = instance->pselGroup;
    R_PFS->PORT[instance->portNum].PIN[instance->rxPinNum].PmnPFS_b.PMR = 1;
}

void SciUart_Init(const SciUart_Instance * instance, uint32_t baudRate)
{
    R_SCI0_Type * peripheral = instance->peripheral;
    uint32_t brr;
    volatile uint32_t settlingDelay;

    sciUart_ConfigurePins(instance);

    /* Release this channel's module stop. */
    R_SYSTEM->PRCR = (uint16_t) (SCI_UART_PRCR_KEY | SCI_UART_PRCR_PRC1);
    R_MSTP->MSTPCRB &= ~(1UL << instance->mstpBit);
    R_SYSTEM->PRCR = (uint16_t) SCI_UART_PRCR_KEY;

    peripheral->SCR = 0; /* Stop TX/RX while the mode/baud settings change. */

    /* The resident bootloader jumps to the application without resetting the
       MCU. Do not rely on SCI reset values: a previous program may have left
       FIFO mode, inverted data, LSB-first transfer, or a non-8-bit character
       length selected. */
    peripheral->FCR = 0;
    peripheral->SCMR_b.SMIF = 0;
    peripheral->SCMR_b.SINV = 0;
    peripheral->SCMR_b.SDIR = 0;
    peripheral->SCMR_b.CHR1 = 1;

    peripheral->SMR_b.CM   = 0; /* Asynchronous mode. */
    peripheral->SMR_b.CHR  = 0; /* 8-bit data. */
    peripheral->SMR_b.MP   = 0; /* Disable multi-processor mode. */
    peripheral->SMR_b.PE   = 0; /* No parity. */
    peripheral->SMR_b.STOP = 0; /* 1 stop bit. */
    peripheral->SMR_b.CKS  = 0; /* PCLKB / 1 base clock. */

    peripheral->SEMR_b.BGDM  = 1; /* Baud-rate generator double-speed mode. */
    peripheral->SEMR_b.ABCSE = 0;
    peripheral->SEMR_b.ABCS  = 0;
    peripheral->SEMR_b.NFEN  = 0;
    peripheral->SEMR_b.ACS0  = 0;
    peripheral->SEMR_b.BRME  = 0; /* No bit-rate modulation needed: with
                                     BGDM=1 and CKS=0,
                                     115200 baud lands within ~0.2% of
                                     nominal. */

    /* BRR = SCI clock / (16 * baudRate) - 1, valid for BGDM=1, ABCS=0, CKS=0
       (RA hardware manual, SCI asynchronous bit rate formula). */
    brr = (SCI_UART_CLOCK_HZ / (16U * baudRate)) - 1U;
    peripheral->BRR = (uint8_t) brr;

    /* The hardware requires at least one bit interval between changing BRR
       and enabling transmission/reception. This loop is deliberately longer
       than that interval at the configured 48 MHz ICLK. */
    for (settlingDelay = 0; settlingDelay < 512U; settlingDelay++) {
        __NOP();
    }

    peripheral->SCR_b.TE = 1;
    peripheral->SCR_b.RE = 1;
}

void SciUart_WriteByte(const SciUart_Instance * instance, uint8_t data)
{
    while (instance->peripheral->SSR_b.TDRE == 0) {}
    instance->peripheral->TDR = data;
}

void SciUart_WriteString(const SciUart_Instance * instance, const char * text)
{
    while (*text != '\0') {
        SciUart_WriteByte(instance, (uint8_t) *text);
        text++;
    }
}

bool SciUart_TryReadByte(const SciUart_Instance * instance, uint8_t * outData)
{
    if (instance->peripheral->SSR_b.RDRF == 0) {
        return false;
    }

    *outData = instance->peripheral->RDR;
    return true;
}
