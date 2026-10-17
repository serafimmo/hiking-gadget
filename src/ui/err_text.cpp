#include "strings.h"

namespace hg {
namespace {
struct ErrText { uint16_t title, hint; };
// Indexed by ErrCode (§9.4); the static_assert keeps the table complete.
const ErrText kErrText[] = {
    {S_ERR_NONE_T, S_ERR_NONE_H},           {S_ERR_OLED_NONE_T, S_ERR_OLED_NONE_H}, {S_ERR_OLED_IO_T, S_ERR_OLED_IO_H},
    {S_ERR_SHT_NONE_T, S_ERR_SHT_NONE_H},   {S_ERR_SHT_UNK_T, S_ERR_SHT_UNK_H},     {S_ERR_SHT_CRC_T, S_ERR_SHT_CRC_H},
    {S_ERR_SHT_IO_T, S_ERR_SHT_IO_H},       {S_ERR_SHT3X_T, S_ERR_SHT3X_H},         {S_ERR_HEAT_HOT_T, S_ERR_HEAT_HOT_H},
    {S_ERR_MAG_NONE_T, S_ERR_MAG_NONE_H},   {S_ERR_MAG_UNK_T, S_ERR_MAG_UNK_H},     {S_ERR_MAG_ID_T, S_ERR_MAG_ID_H},
    {S_ERR_MAG_STUCK_T, S_ERR_MAG_STUCK_H}, {S_ERR_MAG_SAT_T, S_ERR_MAG_SAT_H},     {S_ERR_MAG_UNCAL_T, S_ERR_MAG_UNCAL_H},
    {S_ERR_MAG_CHIP_T, S_ERR_MAG_CHIP_H},   {S_ERR_MAG_NOST_T, S_ERR_MAG_NOST_H},   {S_ERR_I2C_STUCK_T, S_ERR_I2C_STUCK_H},
    {S_ERR_I2C_REC_T, S_ERR_I2C_REC_H},     {S_ERR_KEY_STUCK_T, S_ERR_KEY_STUCK_H}, {S_ERR_NVS_T, S_ERR_NVS_H},
    {S_ERR_CFG_RESET_T, S_ERR_CFG_RESET_H}, {S_ERR_CLK_LOST_T, S_ERR_CLK_LOST_H},   {S_ERR_SYNC_REJ_T, S_ERR_SYNC_REJ_H},
    {S_ERR_CRASH_T, S_ERR_CRASH_H},         {S_ERR_BROWNOUT_T, S_ERR_BROWNOUT_H},   {S_ERR_SLEEP_T, S_ERR_SLEEP_H},
    {S_ERR_RTC_LOST_T, S_ERR_RTC_LOST_H},
};
static_assert(sizeof kErrText / sizeof kErrText[0] == (size_t)ErrCode::Count, "one title/hint pair per ErrCode");
}  // namespace

uint16_t errTitle(ErrCode c) { return kErrText[(size_t)c < (size_t)ErrCode::Count ? (size_t)c : 0].title; }
uint16_t errHint(ErrCode c) { return kErrText[(size_t)c < (size_t)ErrCode::Count ? (size_t)c : 0].hint; }

}  // namespace hg
