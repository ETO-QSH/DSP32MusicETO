/* ============================================================================
 *  key.c -- on-board 3x3 matrix keypad (debounce + press edge)
 * ========================================================================== */
#include "DSP2833x_Device.h"
#include "key.h"
#include "DSP2833x_Device.h"
#include "delay.h"

#define KEY_ROW1_PIN    12U     /* A 缁勶紝杈撳叆 */
#define KEY_ROW2_PIN    13U
#define KEY_ROW3_PIN    14U
#define KEY_COL1_PIN    48U     /* B 缁勶紝杈撳嚭 */
#define KEY_COL2_PIN    49U
#define KEY_COL3_PIN    50U

#define KEY_MUX1_ROWS   (A_MUX1(KEY_ROW1_PIN) | A_MUX1(KEY_ROW2_PIN) | A_MUX1(KEY_ROW3_PIN))
#define KEY_DAT_ROWS    (A_BIT(KEY_ROW1_PIN) | A_BIT(KEY_ROW2_PIN) | A_BIT(KEY_ROW3_PIN))
#define KEY_MUX2_COLS   (B_MUX2(KEY_COL1_PIN) | B_MUX2(KEY_COL2_PIN) | B_MUX2(KEY_COL3_PIN))
#define KEY_DAT_COLS    (B_BIT(KEY_COL1_PIN) | B_BIT(KEY_COL2_PIN) | B_BIT(KEY_COL3_PIN))

/* key scan state */
static U16 key_last_raw = 0U;
static U16 key_stable   = 0U;
static U16 key_latched  = 0U;

void key_init(void)
{
    EALLOW;
    /* rows: GPIO (input) + pull-up enabled */
    GpioCtrlRegs.GPAMUX1.all &= ~KEY_MUX1_ROWS;
    GpioCtrlRegs.GPADIR.all  &= ~KEY_DAT_ROWS;
    GpioCtrlRegs.GPAPUD.all  &= ~KEY_DAT_ROWS;
    /* cols: GPIO (output), driven low one at a time */
    GpioCtrlRegs.GPBMUX2.all &= ~KEY_MUX2_COLS;
    GpioCtrlRegs.GPBDIR.all  |=  KEY_DAT_COLS;
    GpioCtrlRegs.GPBPUD.all  &= ~KEY_DAT_COLS;
    EDIS;

    GpioDataRegs.GPBSET.all = KEY_DAT_COLS;              /* all cols high */
}

U16 key_scan(void)
{
    U32 rows;
    U16 col;
    U16 key = 0U;

    for (col = 0U; col < 3U; col++) {
        GpioDataRegs.GPBSET.all   = KEY_DAT_COLS;                                /* all cols high */
        GpioDataRegs.GPBCLEAR.all = B_BIT((U16)(KEY_COL1_PIN + col));            /* drive this col low */
        delay_us(30U);                                          /* 绛夎绾跨ǔ瀹?*/
        rows = GpioDataRegs.GPADAT.all;

        if ((rows & A_BIT(KEY_ROW1_PIN)) == 0UL) {
            key = (U16)(col + 1U);                    /* K1/K2/K3 */
            break;
        }
        if ((rows & A_BIT(KEY_ROW2_PIN)) == 0UL) {
            key = (U16)(col + 4U);                    /* K4/K5/K6 */
            break;
        }
        if ((rows & A_BIT(KEY_ROW3_PIN)) == 0UL) {
            key = (U16)(col + 7U);                    /* K7/K8/K9 */
            break;
        }
    }
    GpioDataRegs.GPBSET.all = KEY_DAT_COLS;                                      /* all cols high */
    return key;
}

U16 key_task(void)
{
    U16 now = key_scan();
    U16 event = 0U;

    if (now == key_last_raw) {
        if (key_stable < 3U) {
            key_stable++;
        }
    } else {
        key_last_raw = now;
        key_stable = 0U;
    }

    /* needs periodic calls: twice in a row with >= 2 scans of debounce */
    if ((key_stable >= 2U) && (now != 0U) && (now != key_latched)) {
        key_latched = now;
        event = now;
    }
    if (now == 0U) {
        key_latched = 0U;               /* 鏉惧紑鍚庡彲鍐嶆瑙﹀彂 */
    }
    return event;
}
