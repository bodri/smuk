#include "calibration_store.h"

#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_flash.h"
#include "stm32h5xx_hal_flash_ex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * STM32H503RB:
 *
 * 128 KiB Flash total
 * 2 banks x 64 KiB
 * 8 sectors/bank
 * 8 KiB/sector
 *
 * Linker FLASH length is reduced to 112 KiB, therefore:
 *
 * 0x08000000..0x0801BFFF  application
 * 0x0801C000..0x0801DFFF  calibration A
 * 0x0801E000..0x0801FFFF  calibration B
 */

#define CAL_SLOT_A_ADDR 0x0801C000UL
#define CAL_SLOT_B_ADDR 0x0801E000UL

#define CAL_SLOT_A_SECTOR FLASH_SECTOR_6
#define CAL_SLOT_B_SECTOR FLASH_SECTOR_7

#define CAL_FLASH_BANK FLASH_BANK_2

#define FLASH_QWORD_BYTES 16U

/* --------------------------------------------------------------------------
 * Flash helpers
 * -------------------------------------------------------------------------- */

static const smu_cal_record_t* slot_record(uint32_t address) {
    return (const smu_cal_record_t*)address;
}

static bool flash_erase_slot(uint32_t sector) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0xFFFFFFFFUL;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = CAL_FLASH_BANK;
    erase.Sector = sector;
    erase.NbSectors = 1U;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        return false;

    return true;
}

static bool flash_program_record(uint32_t address, const smu_cal_record_t* record) {
    /*
     * H503 user Flash programming is 128 bits at a time.
     *
     * HAL_FLASH_Program() receives a pointer to the 16-byte
     * source block through its DataAddress argument.
     */

    const uint8_t* src = (const uint8_t*)record;
    size_t remaining = sizeof(*record);

    /*
     * Aligned temporary quadword. Any bytes beyond the end of
     * the record remain erased (0xFF).
     */
    uint32_t qword[4];

    while (remaining > 0U) {
        memset(qword, 0xFF, sizeof(qword));

        size_t n = remaining;

        if (n > FLASH_QWORD_BYTES)
            n = FLASH_QWORD_BYTES;

        memcpy(qword, src, n);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, address, (uint32_t)qword) != HAL_OK) {
            return false;
        }

        address += FLASH_QWORD_BYTES;
        src += n;
        remaining -= n;
    }

    return true;
}

static bool flash_write_slot(uint32_t address, uint32_t sector, const smu_cal_record_t* record) {
    bool ok = false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    /*
     * Clear stale Flash error flags before starting.
     */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    if (!flash_erase_slot(sector))
        goto done;

    if (!flash_program_record(address, record))
        goto done;

    /*
     * Read-back verification includes magic/version/size/CRC.
     */
    if (!smu_cal_record_validate(slot_record(address)))
        goto done;

    /*
     * Byte-for-byte verification catches anything not covered
     * by the record validity test.
     */
    if (memcmp((const void*)address, record, sizeof(*record)) != 0) {
        goto done;
    }

    ok = true;

done:

    HAL_FLASH_Lock();

    return ok;
}

/* --------------------------------------------------------------------------
 * Public storage API
 * -------------------------------------------------------------------------- */

void smu_cal_store_init(void) {
    /*
     * Nothing to erase or initialize here.
     *
     * This is deliberately different from the old RAM backend:
     * calibration must survive reset and power cycling.
     */
}

smu_cal_load_result_t smu_cal_store_load(smu_cal_record_t* out) {
    if (out == NULL) {
        return SMU_CAL_LOAD_INVALID;
    }

    const smu_cal_record_t* a = slot_record(CAL_SLOT_A_ADDR);

    const smu_cal_record_t* b = slot_record(CAL_SLOT_B_ADDR);

    bool va = smu_cal_record_validate(a);
    bool vb = smu_cal_record_validate(b);

    if (va && vb) {
        if ((int32_t)(b->sequence - a->sequence) > 0)
            *out = *b;
        else
            *out = *a;

        return SMU_CAL_LOAD_OK;
    }

    if (va) {
        *out = *a;
        return SMU_CAL_LOAD_OK;
    }

    if (vb) {
        *out = *b;
        return SMU_CAL_LOAD_OK;
    }

    /*
     * No valid calibration exists yet.
     * Give the caller safe unity/default coefficients.
     */
    smu_cal_record_defaults(out);

    return SMU_CAL_LOAD_DEFAULTS;
}

bool smu_cal_store_save(const smu_cal_record_t* record) {
    if (record == NULL)
        return false;

    /*
     * Work on a local copy so caller's structure is untouched.
     */
    smu_cal_record_t candidate = *record;

    const smu_cal_record_t* a = slot_record(CAL_SLOT_A_ADDR);

    const smu_cal_record_t* b = slot_record(CAL_SLOT_B_ADDR);

    bool va = smu_cal_record_validate(a);
    bool vb = smu_cal_record_validate(b);

    uint32_t newest_sequence = 0U;

    if (va && vb) {
        if ((int32_t)(b->sequence - a->sequence) > 0)
            newest_sequence = b->sequence;
        else
            newest_sequence = a->sequence;
    } else if (va) {
        newest_sequence = a->sequence;
    } else if (vb) {
        newest_sequence = b->sequence;
    }

    candidate.sequence = newest_sequence + 1U;

    smu_cal_record_finalize(&candidate);
    if (!smu_cal_record_validate(&candidate))
        return false;

    /*
     * Always write the inactive/older slot.
     *
     * We never erase the newest valid calibration before the
     * replacement has been successfully written and verified.
     */
    if (!va) {
        return flash_write_slot(CAL_SLOT_A_ADDR, CAL_SLOT_A_SECTOR, &candidate);
    }

    if (!vb) {
        return flash_write_slot(CAL_SLOT_B_ADDR, CAL_SLOT_B_SECTOR, &candidate);
    }

    if ((int32_t)(b->sequence - a->sequence) > 0) {
        /* B newest -> replace A */
        return flash_write_slot(CAL_SLOT_A_ADDR, CAL_SLOT_A_SECTOR, &candidate);
    } else {
        /* A newest -> replace B */
        return flash_write_slot(CAL_SLOT_B_ADDR, CAL_SLOT_B_SECTOR, &candidate);
    }
}

void smu_cal_store_invalidate_all(void) {
    /*
     * Deliberately explicit destructive operation.
     */

    if (HAL_FLASH_Unlock() != HAL_OK)
        return;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    (void)flash_erase_slot(CAL_SLOT_A_SECTOR);
    (void)flash_erase_slot(CAL_SLOT_B_SECTOR);

    HAL_FLASH_Lock();
}
