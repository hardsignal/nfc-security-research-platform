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

#define ST25_REGULATOR_RESULT_B      0x2CU

#define ST25_CMD_SPACE_B             0xFBU
#define ST25_CMD_ADJUST_REGULATORS   0xD6U

#define ST25_REG_S_MASK              0x80U
#define ST25_OSC_OK_MASK             0x10U


/* =========================================================
 * Debug values visible through J-Link
 * ========================================================= */

volatile uint8_t regulator_result;
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
    st25_modify_reg_b(0x2FU, 0x20U, 0x00U);
    st25_modify_reg_b(0x2FU, 0x10U, 0x10U);
    st25_modify_reg_b(0x2FU, 0x0FU, 0x08U);
    st25_modify_reg(0x0BU, 0xFFU, 0x08U);
    st25_modify_reg(0x0CU, 0xFFU, 0xEDU);
    st25_modify_reg(0x0DU, 0xFFU, 0x00U);
    st25_modify_reg(0x0EU, 0xFFU, 0x00U);
    st25_modify_reg_b(0x0CU, 0xFFU, 0x51U);
    st25_modify_reg_b(0x0DU, 0xFFU, 0x00U);

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
