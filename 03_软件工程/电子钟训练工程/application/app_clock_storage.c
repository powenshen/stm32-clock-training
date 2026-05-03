/*
 * Persistent settings for the clock application.
 * Current version stores alarm time, alarm enable state, and mute state in backup registers.
 */
#include "app_clock_storage.h"

#include "drv_backup.h"

#define APP_CLOCK_STORAGE_MARKER_REGISTER    2U
#define APP_CLOCK_STORAGE_VERSION_REGISTER   3U
#define APP_CLOCK_STORAGE_DATA_REGISTER      4U
#define APP_CLOCK_STORAGE_CHECKSUM_REGISTER  5U

#define APP_CLOCK_STORAGE_MARKER             0x434BU
#define APP_CLOCK_STORAGE_VERSION            0x0001U

/**
 * @brief  将需要持久化的参数打包为 16 位数据
 * @param  clock: 电子钟状态对象指针
 * @retval 打包后的参数数据
 */
static uint16_t AppClockStorage_BuildPackedData(const ClockContext_t *clock)
{
    uint16_t packed_data;  /* 打包后的参数值 */

    packed_data = (uint16_t)(clock->alarm_time.hour & 0x1FU);
    packed_data |= (uint16_t)((clock->alarm_time.minute & 0x3FU) << 5U);
    packed_data |= (uint16_t)((clock->alarm_enabled != 0U) << 11U);
    packed_data |= (uint16_t)((clock->mute_enabled != 0U) << 12U);
    return packed_data;
}

/**
 * @brief  生成参数数据校验值
 * @param  marker: 参数标记值
 * @param  version: 参数版本号
 * @param  packed_data: 打包后的参数数据
 * @retval 16 位校验值
 */
static uint16_t AppClockStorage_BuildChecksum(uint16_t marker, uint16_t version, uint16_t packed_data)
{
    return (uint16_t)(0xA55AU ^ marker ^ version ^ packed_data);
}

/**
 * @brief  初始化电子钟参数存储模块
 * @param  无
 * @retval 无
 */
void AppClockStorage_Init(void)
{
    Drv_Backup_Init();
}

/**
 * @brief  从后备寄存器加载电子钟参数
 * @param  clock: 电子钟状态对象指针
 * @retval 1 表示加载成功，0 表示没有有效参数
 */
uint8_t AppClockStorage_Load(ClockContext_t *clock)
{
    uint16_t marker;       /* 读出的参数标记 */
    uint16_t version;      /* 读出的参数版本号 */
    uint16_t packed_data;  /* 读出的打包参数数据 */
    uint16_t checksum;     /* 读出的校验值 */

    marker = Drv_Backup_Read(APP_CLOCK_STORAGE_MARKER_REGISTER);
    version = Drv_Backup_Read(APP_CLOCK_STORAGE_VERSION_REGISTER);
    packed_data = Drv_Backup_Read(APP_CLOCK_STORAGE_DATA_REGISTER);
    checksum = Drv_Backup_Read(APP_CLOCK_STORAGE_CHECKSUM_REGISTER);

    if ((marker != APP_CLOCK_STORAGE_MARKER) ||
        (version != APP_CLOCK_STORAGE_VERSION) ||
        (checksum != AppClockStorage_BuildChecksum(marker, version, packed_data)))
    {
        return 0U;
    }

    clock->alarm_time.hour = (uint8_t)(packed_data & 0x1FU);
    clock->alarm_time.minute = (uint8_t)((packed_data >> 5U) & 0x3FU);
    clock->alarm_time.second = 0U;
    clock->alarm_enabled = (uint8_t)((packed_data >> 11U) & 0x01U);
    clock->mute_enabled = (uint8_t)((packed_data >> 12U) & 0x01U);
    return 1U;
}

/**
 * @brief  将电子钟参数保存到后备寄存器
 * @param  clock: 电子钟状态对象指针
 * @retval 无
 */
void AppClockStorage_Save(const ClockContext_t *clock)
{
    uint16_t packed_data;  /* 待保存的打包参数数据 */
    uint16_t checksum;     /* 待保存的校验值 */

    packed_data = AppClockStorage_BuildPackedData(clock);
    checksum = AppClockStorage_BuildChecksum(APP_CLOCK_STORAGE_MARKER,
                                             APP_CLOCK_STORAGE_VERSION,
                                             packed_data);

    Drv_Backup_Write(APP_CLOCK_STORAGE_MARKER_REGISTER, APP_CLOCK_STORAGE_MARKER);
    Drv_Backup_Write(APP_CLOCK_STORAGE_VERSION_REGISTER, APP_CLOCK_STORAGE_VERSION);
    Drv_Backup_Write(APP_CLOCK_STORAGE_DATA_REGISTER, packed_data);
    Drv_Backup_Write(APP_CLOCK_STORAGE_CHECKSUM_REGISTER, checksum);
}
