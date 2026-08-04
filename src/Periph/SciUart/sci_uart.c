#include "sci_uart.h"

#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>

/* Set by Core/Clock's Clock_Init(): ICLK /1, PCLKB /2 from a 48 MHz HOCO. */
#define SCI_UART_PCLKB_HZ (24000000U)

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

    sciUart_ConfigurePins(instance);

    /* Release this channel's module stop. */
    R_SYSTEM->PRCR = (uint16_t) (SCI_UART_PRCR_KEY | SCI_UART_PRCR_PRC1);
    R_MSTP->MSTPCRB &= ~(1UL << instance->mstpBit);
    R_SYSTEM->PRCR = (uint16_t) SCI_UART_PRCR_KEY;

    peripheral->SCR = 0; /* Stop TX/RX while the mode/baud settings change. */

    peripheral->SMR_b.CM   = 0; /* Asynchronous mode. */
    peripheral->SMR_b.CHR  = 0; /* 8-bit data. */
    peripheral->SMR_b.PE   = 0; /* No parity. */
    peripheral->SMR_b.STOP = 0; /* 1 stop bit. */
    peripheral->SMR_b.CKS  = 0; /* PCLKB / 1 base clock. */

    peripheral->SEMR_b.BGDM  = 1; /* Baud-rate generator double-speed mode. */
    peripheral->SEMR_b.ABCSE = 0;
    peripheral->SEMR_b.ABCS  = 0;
    peripheral->SEMR_b.BRME  = 0; /* No bit-rate modulation needed: with
                                     BGDM=1, CKS=0 and a 24 MHz PCLKB,
                                     115200 baud lands within ~0.2% of
                                     nominal. */

    /* BRR = PCLKB / (16 * baudRate) - 1, valid for BGDM=1, ABCS=0, CKS=0
       (RA hardware manual, SCI asynchronous bit rate formula). */
    brr = (SCI_UART_PCLKB_HZ / (16U * baudRate)) - 1U;
    peripheral->BRR = (uint8_t) brr;

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
