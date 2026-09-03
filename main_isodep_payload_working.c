#include <stdint.h>

/* =========================================================
 * STM32F446RE peripheral addresses
 * ========================================================= */

#define RCC_BASE        0x40023800UL
#define GPIOA_BASE      0x40020000UL
#define GPIOB_BASE      0x40020400UL
#define SPI1_BASE       0x40013000UL

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14))

#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint32_t *)(SPI1_BASE + 0x0C))


/* =========================================================
 * ST25R3916B register definitions used in this experiment
 * ========================================================= */

#define ST25_REG_OP_CONTROL          0x02U
#define ST25_REG_MODE                0x03U
#define ST25_REG_REGULATOR_CONTROL   0x2CU
#define ST25_REG_AUX_DISPLAY         0x31U
#define ST25_REG_IDENTITY            0x3FU
#define ST25_REG_AUX                 0x0AU
#define ST25_REG_IRQ_MASK_TIMER_NFC  0x17U
#define ST25_REG_IRQ_MASK_TARGET     0x19U
#define ST25_REG_IRQ_MAIN            0x1AU
#define ST25_REG_ISO14443A_NFC       0x05U
#define ST25_REG_IRQ_MASK_MAIN       0x16U
#define ST25_REG_IRQ_MASK_ERROR_WUP  0x18U
#define ST25_REG_FIFO_STATUS1        0x1EU
#define ST25_REG_NUM_TX_BYTES1       0x22U
#define ST25_REG_NUM_TX_BYTES2       0x23U

#define ST25_REGULATOR_RESULT_B      0x2CU
#define ST25_REG_FIELD_ON_GT_B       0x15U

#define ST25_CMD_SPACE_B             0xFBU
#define ST25_CMD_ADJUST_REGULATORS   0xD6U
#define ST25_CMD_INITIAL_RF_COLLISION 0xC8U
#define ST25_CMD_STOP                0xC2U
#define ST25_CMD_TRANSMIT_REQA       0xC6U
#define ST25_CMD_TRANSMIT_WITH_CRC   0xC4U
#define ST25_CMD_TRANSMIT_WITHOUT_CRC 0xC5U
#define ST25_CMD_RESET_RXGAIN        0xD5U
#define ST25_CMD_CLEAR_FIFO          0xDBU

#define ST25_REG_S_MASK              0x80U
#define ST25_OSC_OK_MASK             0x10U


/* =========================================================
 * Debug values visible through J-Link
 * ========================================================= */

volatile uint8_t regulator_result;
volatile uint8_t irq_dct_status;
volatile uint8_t irq_mask_timer;
volatile uint8_t irq_mask_target;
volatile uint8_t irq_regs[4];
volatile uint8_t field_irq[4];
volatile uint8_t field_op_control;
volatile uint8_t reqa_irq[4];
volatile uint8_t fifo_status[2];
volatile uint8_t atqa[2];
volatile uint8_t atqa_len;
volatile uint8_t anticoll_tx[2];
volatile uint8_t anticoll_irq[4];
volatile uint8_t anticoll_fifo_status[2];
volatile uint8_t uid_cl1[5];
volatile uint8_t select_cl1_tx[7];
volatile uint8_t select_cl1_irq[4];
volatile uint8_t select_cl1_fifo_status[2];
volatile uint8_t sak_cl1;
volatile uint8_t uid_cl2[5];
volatile uint8_t sak_cl2;
volatile uint8_t rats_tx[2];
volatile uint8_t ats[32];
volatile uint8_t ats_irq[4];
volatile uint8_t ats_fifo_status[2];
volatile uint8_t ats_len;
volatile uint8_t ats_tl;
volatile uint8_t ats_t0;
volatile uint8_t ats_ta1;
volatile uint8_t ats_tb1;
volatile uint8_t ats_tc1;
volatile uint8_t ats_hist_len;
volatile uint8_t ats_hist[16];
volatile uint8_t isodep_tx[32];
volatile uint8_t isodep_rx[32];
volatile uint8_t isodep_irq[4];
volatile uint8_t isodep_fifo_status[2];
volatile uint8_t isodep_rx_len;
volatile uint8_t isodep_tx_len;
volatile uint8_t select_cl2_tx[7];
volatile uint8_t select_cl2_irq[4];
volatile uint8_t select_cl2_fifo_status[2];
volatile uint8_t anticoll_cl2_tx[2];
volatile uint8_t anticoll_cl2_irq[4];
volatile uint8_t anticoll_cl2_fifo_status[2];
volatile uint8_t reg00;
volatile uint8_t reg01;
volatile uint8_t op_control;
volatile uint8_t mode;
volatile uint8_t aux_display;
volatile uint8_t identity;


/* =========================================================
 * Basic delay
 * ========================================================= */

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm volatile ("nop");
    }
}


/* =========================================================
 * SPI transfer
 * ========================================================= */

static uint8_t spi_xfer(uint8_t data)
{
    /* Wait until TX buffer is empty */
    while (!(SPI1_SR & (1U << 1))) {
    }

    *(volatile uint8_t *)&SPI1_DR = data;

    /* Wait until one byte has been received */
    while (!(SPI1_SR & (1U << 0))) {
    }

    return *(volatile uint8_t *)&SPI1_DR;
}


/* =========================================================
 * Space-A register access
 * ========================================================= */

static uint8_t st25_read_reg(uint8_t reg)
{
    uint8_t value;

    /* CS low */
    GPIOB_ODR &= ~(1U << 6);

    /* Register read = 01xxxxxx */
    spi_xfer(0x40U | (reg & 0x3FU));

    /* Dummy byte clocks out register value */
    value = spi_xfer(0x00U);

    while (SPI1_SR & (1U << 7)) {
    }

    /* CS high */
    GPIOB_ODR |= (1U << 6);

    return value;
}

static void st25_read_regs(uint8_t reg, uint8_t *values, uint8_t length)
{
    uint8_t i;

    GPIOB_ODR &= ~(1U << 6);
    spi_xfer(0x40U | (reg & 0x3FU));

    for (i = 0U; i < length; i++)
        values[i] = spi_xfer(0x00U);

    while (SPI1_SR & (1U << 7)) {}
    GPIOB_ODR |= (1U << 6);
}

static void st25_read_fifo(uint8_t *values, uint8_t length)
{
    uint8_t i;

    /* Select ST25R3916B: PB6 / SEN goes LOW. */
    GPIOB_ODR &= ~(1U << 6);

    /* Dedicated FIFO READ command. */
    spi_xfer(0x9FU);

    /* Each dummy byte clocks one FIFO byte back on MISO. */
    for (i = 0U; i < length; i++)
    {
        values[i] = spi_xfer(0x00U);
    }

    /* Wait until SPI hardware is no longer busy. */
    while (SPI1_SR & (1U << 7))
    {
    }

    /* Release ST25R3916B: PB6 / SEN goes HIGH. */
    GPIOB_ODR |= (1U << 6);
}

static void st25_write_fifo(const uint8_t *values, uint8_t length)
{
    uint8_t i;

    /* CS low */
    GPIOB_ODR &= ~(1U << 6);

    /* FIFO LOAD command */
    spi_xfer(0x80U);

    for (i = 0U; i < length; i++)
    {
        spi_xfer(values[i]);
    }

    while (SPI1_SR & (1U << 7))
    {
    }

    /* CS high */
    GPIOB_ODR |= (1U << 6);
}

static void st25_write_reg(uint8_t reg, uint8_t value)
{
    /* CS low */
    GPIOB_ODR &= ~(1U << 6);

    spi_xfer(reg & 0x3FU);
    spi_xfer(value);

    while (SPI1_SR & (1U << 7)) {
    }

    /* CS high */
    GPIOB_ODR |= (1U << 6);
}


static void st25_modify_reg(uint8_t reg,
                            uint8_t mask,
                            uint8_t value)
{
    uint8_t old_value;
    uint8_t new_value;

    old_value = st25_read_reg(reg);

    new_value =
        (uint8_t)((old_value & (uint8_t)~mask) |
                  (value & mask));

    if (new_value != old_value) {
        st25_write_reg(reg, new_value);
    }
}

static void st25_set_num_tx_bits(uint16_t bits)
{
    uint16_t full_bytes;
    uint8_t last_bits;

    full_bytes = bits >> 3;
    last_bits = (uint8_t)(bits & 0x07U);

    st25_write_reg(
        ST25_REG_NUM_TX_BYTES1,
        (uint8_t)(full_bytes >> 5)
    );

    st25_write_reg(
        ST25_REG_NUM_TX_BYTES2,
        (uint8_t)(((full_bytes & 0x1FU) << 3) | last_bits)
    );
}

/* =========================================================
 * Space-B register access
 *
 * Important:
 * 0xFB + register command + data remain under ONE CS-low
 * transaction.
 * ========================================================= */

static uint8_t st25_read_reg_b(uint8_t reg)
{
    uint8_t value;

    /* CS low */
    GPIOB_ODR &= ~(1U << 6);

    /* Select Space B */
    spi_xfer(ST25_CMD_SPACE_B);

    /* Read register */
    spi_xfer(0x40U | (reg & 0x3FU));

    /* Clock out returned value */
    value = spi_xfer(0x00U);

    while (SPI1_SR & (1U << 7)) {
    }

    /* CS high */
    GPIOB_ODR |= (1U << 6);

    return value;
}


static void st25_write_reg_b(uint8_t reg, uint8_t value)
{
    /* CS low */
    GPIOB_ODR &= ~(1U << 6);

    /* Select Space B */
    spi_xfer(ST25_CMD_SPACE_B);

    /* Register write */
    spi_xfer(reg & 0x3FU);
    spi_xfer(value);

    while (SPI1_SR & (1U << 7)) {
    }

    /* CS high */
    GPIOB_ODR |= (1U << 6);
}


static void st25_modify_reg_b(uint8_t reg,
                              uint8_t mask,
                              uint8_t value)
{
    uint8_t old_value;
    uint8_t new_value;

    old_value = st25_read_reg_b(reg);

    new_value =
        (uint8_t)((old_value & (uint8_t)~mask) |
                  (value & mask));

    if (new_value != old_value) {
        st25_write_reg_b(reg, new_value);
    }
}


/* =========================================================
 * ST25R3916B direct command
 * ========================================================= */

static void st25_direct_cmd(uint8_t cmd)
{
    /* CS low */
    GPIOB_ODR &= ~(1U << 6);

    spi_xfer(cmd | 0xC0U);

    while (SPI1_SR & (1U << 7)) {
    }

    /* CS high */
    GPIOB_ODR |= (1U << 6);
}


/* =========================================================
 * Regulator adjustment
 *
 * Mirrors the sequence we traced in ST's driver:
 *
 *   set reg_s
 *   clear reg_s
 *   execute ADJUST_REGULATORS
 *   wait for completion
 *   read regulator result from Space B
 *
 * We currently use a conservative delay instead of DCT IRQ.
 * ========================================================= */

static uint8_t st25_adjust_regulators(void)
{
    uint8_t result;

    st25_modify_reg(
        ST25_REG_REGULATOR_CONTROL,
        ST25_REG_S_MASK,
        ST25_REG_S_MASK
    );

    st25_modify_reg(
        ST25_REG_REGULATOR_CONTROL,
        ST25_REG_S_MASK,
        0x00U
    );

    st25_direct_cmd(ST25_CMD_ADJUST_REGULATORS);

    /*
     * ST specifies a maximum adjustment duration around 5 ms.
     * This loop is deliberately conservative for the current
     * bare-metal experiment.
     */
    delay(30000);

    result = st25_read_reg_b(ST25_REGULATOR_RESULT_B);

    return result;
}

static void st25_prepare_reqa(void)
{
    uint8_t clear_irq[4];

    /* Stop current activity and reset RX gain, matching RFAL preparation. */
    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_RESET_RXGAIN);

    /* Clear pending hardware IRQs. */
    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);
    
    /* Enable FWL + RXS + RXE + TXE + COL.
     * Mask semantics: 0 = enabled.
     */
    st25_modify_reg(ST25_REG_IRQ_MASK_MAIN, 0x7CU, 0x00U);

    /* Enable NRE. */
    st25_modify_reg(ST25_REG_IRQ_MASK_TIMER_NFC, 0x40U, 0x00U);

    /* Enable CRC + PAR + ERR2 + ERR1. */
    st25_modify_reg(ST25_REG_IRQ_MASK_ERROR_WUP, 0xF0U, 0x00U);

    /* Normal ISO14443A parity/NFC framing. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0xE0U, 0x00U);

    /* Clear nbtx bits before REQA. */
    st25_write_reg(ST25_REG_NUM_TX_BYTES2, 0x00U);
}

static void st25_parse_ats(void)
{
    uint8_t index;
    uint8_t i;

    ats_tl = 0U;
    ats_t0 = 0U;
    ats_ta1 = 0U;
    ats_tb1 = 0U;
    ats_tc1 = 0U;
    ats_hist_len = 0U;

    for (i = 0U; i < sizeof(ats_hist); i++)
    {
        ats_hist[i] = 0U;
    }

    if (ats_len < 2U)
    {
        return;
    }

    ats_tl = ats[0];
    ats_t0 = ats[1];
    index = 2U;

    if ((ats_t0 & 0x10U) != 0U)
    {
        ats_ta1 = ats[index++];
    }

    if ((ats_t0 & 0x20U) != 0U)
    {
        ats_tb1 = ats[index++];
    }

    if ((ats_t0 & 0x40U) != 0U)
    {
        ats_tc1 = ats[index++];
    }

    while ((index < ats_tl) && (ats_hist_len < sizeof(ats_hist)))
    {
        ats_hist[ats_hist_len++] = ats[index++];
    }
}

static void st25_isodep_test(void)
{
    uint8_t clear_irq[4];

    isodep_tx[0] = 0x02U;
    isodep_tx[1] = 0x00U;
    isodep_tx_len = 2U;

    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_CLEAR_FIFO);

    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x00U);

    st25_set_num_tx_bits((uint16_t)isodep_tx_len * 8U);

    st25_write_fifo((uint8_t *)isodep_tx, isodep_tx_len);

    st25_direct_cmd(ST25_CMD_TRANSMIT_WITH_CRC);

    delay(60000U);

    st25_read_regs(
        ST25_REG_IRQ_MAIN,
        (uint8_t *)isodep_irq,
        4U
    );

    st25_read_regs(
        ST25_REG_FIFO_STATUS1,
        (uint8_t *)isodep_fifo_status,
        2U
    );

    isodep_rx_len = isodep_fifo_status[0];

    if (isodep_rx_len > sizeof(isodep_rx))
    {
        isodep_rx_len = sizeof(isodep_rx);
    }

    if (isodep_rx_len > 0U)
    {
        st25_read_fifo((uint8_t *)isodep_rx, isodep_rx_len);
    }
}

static void st25_rats(void)
{
    uint8_t clear_irq[4];

    rats_tx[0] = 0xE0U;
    rats_tx[1] = 0x80U;

    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_CLEAR_FIFO);

    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    /* Standard ISO14443A framing. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x00U);

    /* RATS = 2 bytes before automatic CRC. */
    st25_set_num_tx_bits(16U);

    st25_write_fifo((uint8_t *)rats_tx, 2U);

    st25_direct_cmd(ST25_CMD_TRANSMIT_WITH_CRC);

    delay(60000U);

    st25_read_regs(
        ST25_REG_IRQ_MAIN,
        (uint8_t *)ats_irq,
        4U
    );

    st25_read_regs(
        ST25_REG_FIFO_STATUS1,
        (uint8_t *)ats_fifo_status,
        2U
    );

    ats_len = ats_fifo_status[0];

    if (ats_len > sizeof(ats))
    {
        ats_len = sizeof(ats);
    }

    if (ats_len > 0U)
    {
        st25_read_fifo((uint8_t *)ats, ats_len);

        st25_parse_ats();
        st25_isodep_test();
    }
}

static void st25_select_cl2(void)
{
    uint8_t clear_irq[4];

    select_cl2_tx[0] = 0x95U;
    select_cl2_tx[1] = 0x70U;
    select_cl2_tx[2] = uid_cl2[0];
    select_cl2_tx[3] = uid_cl2[1];
    select_cl2_tx[4] = uid_cl2[2];
    select_cl2_tx[5] = uid_cl2[3];
    select_cl2_tx[6] = uid_cl2[4];

    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_CLEAR_FIFO);

    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    /* SELECT is a standard ISO14443A frame. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x00U);

    /* 7 bytes before automatic CRC. */
    st25_set_num_tx_bits(56U);

    st25_write_fifo((uint8_t *)select_cl2_tx, 7U);

    st25_direct_cmd(ST25_CMD_TRANSMIT_WITH_CRC);

    delay(60000U);

    st25_read_regs(
        ST25_REG_IRQ_MAIN,
        (uint8_t *)select_cl2_irq,
        4U
    );

    st25_read_regs(
        ST25_REG_FIFO_STATUS1,
        (uint8_t *)select_cl2_fifo_status,
        2U
    );

    st25_read_fifo((uint8_t *)&sak_cl2, 1U);

    st25_rats();

}

static void st25_anticollision_cl2(void);
static void st25_rats(void);
static void st25_parse_ats(void);
static void st25_isodep_test(void);
static void st25_select_cl1(void)
{
    uint8_t clear_irq[4];

    select_cl1_tx[0] = 0x93U;
    select_cl1_tx[1] = 0x70U;
    select_cl1_tx[2] = uid_cl1[0];
    select_cl1_tx[3] = uid_cl1[1];
    select_cl1_tx[4] = uid_cl1[2];
    select_cl1_tx[5] = uid_cl1[3];
    select_cl1_tx[6] = uid_cl1[4];

    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_CLEAR_FIFO);

    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    /* SELECT is a standard ISO14443A frame. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x00U);

    /* 7 bytes before automatic CRC. */
    st25_set_num_tx_bits(56U);

    st25_write_fifo((uint8_t *)select_cl1_tx, 7U);

    st25_direct_cmd(ST25_CMD_TRANSMIT_WITH_CRC);

    delay(60000U);

    st25_read_regs(
        ST25_REG_IRQ_MAIN,
        (uint8_t *)select_cl1_irq,
        4U
    );

    st25_read_regs(
        ST25_REG_FIFO_STATUS1,
        (uint8_t *)select_cl1_fifo_status,
        2U
    );

    st25_read_fifo((uint8_t *)&sak_cl1, 1U);
    st25_anticollision_cl2();
}

static void st25_anticollision_cl2(void)
{
    uint8_t clear_irq[4];

    anticoll_cl2_tx[0] = 0x95U;
    anticoll_cl2_tx[1] = 0x20U;

    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_CLEAR_FIFO);

    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    /* ISO14443A anticollision reception mode. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x01U);

    /* 0x95 0x20 = 16 transmitted bits. */
    st25_set_num_tx_bits(16U);

    st25_write_fifo((uint8_t *)anticoll_cl2_tx, 2U);

    st25_direct_cmd(ST25_CMD_TRANSMIT_WITHOUT_CRC);

    delay(60000U);

    st25_read_regs(
        ST25_REG_IRQ_MAIN,
        (uint8_t *)anticoll_cl2_irq,
        4U
    );

    st25_read_regs(
        ST25_REG_FIFO_STATUS1,
        (uint8_t *)anticoll_cl2_fifo_status,
        2U
    );

    st25_read_fifo((uint8_t *)uid_cl2, 5U);
    st25_select_cl2();

    /* Return to normal ISO14443A framing. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x00U);
}

static void st25_anticollision_cl1(void)
{
    uint8_t clear_irq[4];

    anticoll_tx[0] = 0x93U;
    anticoll_tx[1] = 0x20U;

    st25_direct_cmd(ST25_CMD_STOP);
    st25_direct_cmd(ST25_CMD_CLEAR_FIFO);

    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    /* ISO14443A anticollision reception mode. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x01U);

    /* 0x93 0x20 = 16 transmitted bits. */
    st25_set_num_tx_bits(16U);

    st25_write_fifo((uint8_t *)anticoll_tx, 2U);

    st25_direct_cmd(ST25_CMD_TRANSMIT_WITHOUT_CRC);

    delay(60000U);

    st25_read_regs(
        ST25_REG_IRQ_MAIN,
        (uint8_t *)anticoll_irq,
        4U
    );

    st25_read_regs(
        ST25_REG_FIFO_STATUS1,
        (uint8_t *)anticoll_fifo_status,
        2U
    );

    st25_read_fifo((uint8_t *)uid_cl1, 5U);
    st25_select_cl1();

    /* Return to normal ISO14443A framing. */
    st25_modify_reg(ST25_REG_ISO14443A_NFC, 0x01U, 0x00U);
}

static void st25_field_on_test(void)
{
    uint8_t clear_irq[4];

    /* Clear any pending hardware IRQs. */
    st25_read_regs(ST25_REG_IRQ_MAIN, clear_irq, 4U);

    /* Enable CAC + CAT in IRQ_TIMER_NFC:
     * CAC = bit2
     * CAT = bit1
     * 0 = enabled
     */
    st25_modify_reg(ST25_REG_IRQ_MASK_TIMER_NFC, 0x06U, 0x00U);

    /* Enable APON in IRQ_TARGET:
     * APON = bit5
     * 0 = enabled
     */
    st25_modify_reg(ST25_REG_IRQ_MASK_TARGET, 0x20U, 0x00U);

    /* Official RFAL field-on preparation. */
    st25_write_reg_b(ST25_REG_FIELD_ON_GT_B, 0x00U);
    st25_modify_reg(ST25_REG_OP_CONTROL, 0x03U, 0x01U);
    st25_modify_reg(ST25_REG_AUX, 0x03U, 0x00U);

    /* Initial RF Collision Avoidance. */
    st25_direct_cmd(ST25_CMD_INITIAL_RF_COLLISION);

    /* Conservative wait; official RFAL timeout is 10 ms. */
    delay(60000U);

    /* Capture CAC / CAT / APON result for J-Link. */
    st25_read_regs(ST25_REG_IRQ_MAIN, (uint8_t *)field_irq, 4U);

    /* Restore automatic external-field detector. */
    st25_modify_reg(ST25_REG_OP_CONTROL, 0x03U, 0x03U);

    field_op_control = st25_read_reg(ST25_REG_OP_CONTROL);
    st25_prepare_reqa();

    /* Enable receiver before REQA response. */
    st25_modify_reg(ST25_REG_OP_CONTROL, 0x40U, 0x40U);

    st25_direct_cmd(ST25_CMD_TRANSMIT_REQA);
    
    /* Allow REQA transmission and possible ATQA reception to complete. */
    delay(60000U);

    /* Capture REQA/RX interrupt status. */
    st25_read_regs(ST25_REG_IRQ_MAIN, (uint8_t *)reqa_irq, 4U);
    st25_read_regs(ST25_REG_FIFO_STATUS1, (uint8_t *)fifo_status, 2U);
    atqa_len = fifo_status[0];
    st25_read_fifo((uint8_t *)atqa, 2U);
    st25_anticollision_cl1();

    st25_write_reg(ST25_REG_OP_CONTROL, 0x80U);
}

/* =========================================================
 * Hardware initialization
 * ========================================================= */

static void hardware_init(void)
{
    /* Enable GPIOA + GPIOB clocks */
    RCC_AHB1ENR |= (1U << 0) | (1U << 1);

    /* Enable SPI1 clock */
    RCC_APB2ENR |= (1U << 12);


    /* -----------------------------------------------------
     * PB6 = software-controlled chip select
     * ----------------------------------------------------- */

    GPIOB_MODER &= ~(3U << (6 * 2));
    GPIOB_MODER |=  (1U << (6 * 2));

    /* CS idle high */
    GPIOB_ODR |= (1U << 6);


    /* -----------------------------------------------------
     * PA5 = SPI1 SCLK
     * PA6 = SPI1 MISO
     * PA7 = SPI1 MOSI
     * Alternate Function 5
     * ----------------------------------------------------- */

    GPIOA_MODER &= ~(
        (3U << (5 * 2)) |
        (3U << (6 * 2)) |
        (3U << (7 * 2))
    );

    GPIOA_MODER |= (
        (2U << (5 * 2)) |
        (2U << (6 * 2)) |
        (2U << (7 * 2))
    );

    GPIOA_AFRL &= ~(
        (0xFU << (5 * 4)) |
        (0xFU << (6 * 4)) |
        (0xFU << (7 * 4))
    );

    GPIOA_AFRL |= (
        (5U << (5 * 4)) |
        (5U << (6 * 4)) |
        (5U << (7 * 4))
    );


    /* -----------------------------------------------------
     * SPI1
     *
     * Master
     * Baud divider /16
     * CPOL = 0
     * CPHA = 1
     * Software slave management
     * SPI enabled
     * ----------------------------------------------------- */

    SPI1_CR1 =
        (1U << 2) |       /* MSTR */
        (3U << 3) |       /* BR = /16 */
        (1U << 8) |       /* SSI */
        (1U << 9) |       /* SSM */
        (1U << 0) |       /* CPHA = 1 */
        (1U << 6);        /* SPE */
}


/* =========================================================
 * Main
 * ========================================================= */

int main(void)
{
    hardware_init();

    st25_modify_reg(0x00U, 0x07U, 0x07U);
    st25_modify_reg(0x01U, 0x18U, 0x18U);
    st25_modify_reg(0x01U, 0x20U, 0x20U);
    st25_modify_reg(0x28U, 0x0FU, 0x00U);
    st25_modify_reg_b(0x2AU, 0x80U, 0x80U);
    st25_modify_reg(0x2AU, 0x70U, 0x10U);
    st25_modify_reg(0x2AU, 0x0FU, 0x01U);
    st25_modify_reg(0x2BU, 0x70U, 0x00U);
    st25_modify_reg(0x2BU, 0x0FU, 0x00U);
    st25_modify_reg_b(0x28U, 0x20U, 0x00U);
    st25_modify_reg_b(0x28U, 0x10U, 0x10U);
    st25_modify_reg(0x08U, 0xF0U, 0x50U);
    st25_modify_reg(0x29U, 0xFFU, 0x2FU);
    st25_modify_reg_b(0x05U, 0x40U, 0x40U);
    st25_modify_reg(0x26U, 0xFFU, 0x40U);
    st25_modify_reg(0x27U, 0xFFU, 0x58U);
    st25_modify_reg_b(0x28U, 0x04U, 0x04U);
    st25_modify_reg_b(0x2EU, 0x01U, 0x01U);
    st25_modify_reg_b(0x28U, 0x80U, 0x80U);
    st25_modify_reg_b(0x2EU, 0x08U, 0x08U);
    st25_modify_reg(0x05U, 0x1EU, 0x00U);
    st25_modify_reg_b(0x34U, 0x0FU, 0x01U);
    st25_modify_reg_b(0x36U, 0xF0U, 0x70U);
    st25_modify_reg_b(0x36U, 0x0FU, 0x09U);
    st25_modify_reg_b(0x37U, 0x0FU, 0x07U);
    st25_modify_reg(0x03U, 0x04U, 0x00U);
    st25_modify_reg(0x28U, 0xF0U, 0xF0U);
    st25_modify_reg_b(0x2FU, 0x20U, 0x00U);
    st25_modify_reg_b(0x2FU, 0x10U, 0x10U);
    st25_modify_reg_b(0x2FU, 0x0FU, 0x08U);
    st25_modify_reg(0x0BU, 0xFFU, 0x08U);
    st25_modify_reg(0x0CU, 0xFFU, 0xEDU);
    st25_modify_reg(0x0DU, 0xFFU, 0x00U);
    st25_modify_reg(0x0EU, 0xFFU, 0x00U);
    st25_modify_reg_b(0x0CU, 0xFFU, 0x51U);
    st25_modify_reg_b(0x0DU, 0xFFU, 0x00U);
    st25_modify_reg(0x17U, 0x80U, 0x00U);
    irq_mask_timer = st25_read_reg(0x17U);
    irq_mask_target = st25_read_reg(0x19U);
    regulator_result = st25_adjust_regulators();
    st25_read_regs(0x1AU, (uint8_t *)irq_regs, 4U);

     while (1) {

        /*
         * Put ST25R3916B into Ready mode:
         * en = 1
         * tx_en = 0
         * rx_en = 0
         *
         * RF FIELD REMAINS OFF.
         */
        st25_write_reg(ST25_REG_OP_CONTROL, 0x80U);


        /*
         * Wait until crystal oscillator is stable.
         * AUX_DISPLAY bit4 = osc_ok
         */
        do {
            aux_display =
                st25_read_reg(ST25_REG_AUX_DISPLAY);

        } while ((aux_display & ST25_OSC_OK_MASK) == 0U);


        /*
         * ISO14443A initiator mode.
         *
         * Mode register 0x03 = 0x08
         */
        st25_write_reg(ST25_REG_MODE, 0x08U);


        /*
         * Perform regulator adjustment and read
         * Space-B regulator result.
         */
        regulator_result = st25_adjust_regulators();
        st25_field_on_test();

        /*
         * Capture known registers in SRAM for J-Link.
         */
        reg00 =
            st25_read_reg(0x00U);

        reg01 =
            st25_read_reg(0x01U);

        op_control =
            st25_read_reg(ST25_REG_OP_CONTROL);

        mode =
            st25_read_reg(ST25_REG_MODE);

        aux_display =
            st25_read_reg(ST25_REG_AUX_DISPLAY);

        identity =
            st25_read_reg(ST25_REG_IDENTITY);


        delay(800000);
    }
}
