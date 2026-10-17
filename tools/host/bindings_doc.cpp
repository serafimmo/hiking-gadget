#include "bindings_doc.h"

#include <cstring>

#include "fixtures.h"
#include "ui/strings.h"

namespace hg {
namespace {
const char* actionName(ActionId a, DocLang lang) {
  static const char* const en[] = {
      "-", "previous page", "next page", "details on/off", "open list", "cursor up", "cursor down", "back",
      "open / edit / act", "item above", "item below", "cancel", "field +1", "field -1", "previous field / cancel",
      "next field", "previous page", "next page", "back", "OK", "dismiss alert", "hint",
      "display off", "power off", "start", "pause", "resume", "lap", "reset", "next preset", "start", "pause",
      "resume", "+1 min", "cancel", "new (select)", "again (same preset)", "mark target", "clear target",
      "calibrate", "re-probe", "cancel calibration", "save calibration", "retry", "done", "save",
      "set the time", "reset settings", "reboot", "heater test", "rescan", "reset min/max"};
  static const char* const ru[] = {
      "-", "пред. страница", "след. страница", "подробности вкл/выкл", "открыть список", "курсор вверх",
      "курсор вниз", "назад", "открыть / изменить / выполнить", "значение выше", "значение ниже", "отмена",
      "поле +1", "поле -1", "пред. поле / отмена", "след. поле", "пред. страница", "след. страница", "назад", "OK",
      "снять сигнал", "подсказка", "погасить экран", "выключить", "старт", "пауза", "продолжить", "круг", "сброс",
      "след. пресет", "старт", "пауза", "продолжить", "+1 мин", "отменить таймер", "новый (выбор)",
      "ещё раз (тот же пресет)", "запомнить цель", "сбросить цель", "калибровать", "искать датчик снова",
      "прервать калибровку", "сохранить калибровку", "повторить", "готово", "сохранить", "установить время",
      "сбросить настройки", "перезагрузить", "тест нагревателя", "пересканировать шины", "сбросить min/max"};
  static_assert(sizeof en / sizeof en[0] == (size_t)ActionId::Count, "one English name per ActionId");
  static_assert(sizeof ru / sizeof ru[0] == (size_t)ActionId::Count, "one Russian name per ActionId");
  if ((size_t)a >= (size_t)ActionId::Count) return "?";
  return lang == DocLang::Ru ? ru[(size_t)a] : en[(size_t)a];
}

// Russian descriptions of the states that reach the table (the first fixture of each distinct key row).
const char* stateRu(const char* fixture) {
  static const char* const k[][2] = {
      {"home_synced", "Home (главный экран)"},
      {"sw_idle", "Секундомер на нуле"},
      {"sw_running", "Секундомер идёт"},
      {"sw_paused", "Секундомер на паузе"},
      {"tm_select", "Таймер: выбор времени"},
      {"tm_running", "Таймер идёт"},
      {"tm_paused_on", "Таймер на паузе"},
      {"tm_done", "Таймер: время вышло"},
      {"alert_a", "Сигнал таймера (поверх любого экрана)"},
      {"cmp_ok", "Компас"},
      {"cmp_target", "Компас, цель запомнена"},
      {"cmp_uncal", "Компас без калибровки"},
      {"cmp_none", "Компас: датчик не найден"},
      {"set_cover", "Settings: обложка"},
      {"set_list_top", "Settings: список"},
      {"set_list_system_hold", "Settings: пункт, срабатывающий по удержанию (Power off, Reboot, Reset)"},
      {"set_list_sht3x", "Settings: пункт, недоступный с этим датчиком (нагреватель на SHT3x)"},
      {"pick_tz", "Выбор значения настройки"},
      {"pick_heater_na", "Выбор значения: нагреватель недоступен (SHT3x)"},
      {"set_time_hh", "Установка времени: поля часов, минут, дня, месяца"},
      {"set_time_year", "Установка времени: последнее поле (год)"},
      {"cal_start", "Калибровка компаса: идёт сбор"},
      {"cal_ready", "Калибровка компаса: можно сохранить"},
      {"cal_good", "Калибровка компаса: успешно"},
      {"cal_failed", "Калибровка компаса: не удалась"},
      {"diag_sensors", "Diagnostics > Sensors"},
      {"diag_i2c", "Diagnostics > I2C"},
      {"diag_buttons", "Diagnostics > Buttons (все кнопки перехвачены для проверки)"},
      {"diag_mag", "Diagnostics > Magnetometer"},
      {"diag_system", "Diagnostics > System"},
      {"card_check_hw", "Карточка с ошибкой (CHECK HARDWARE, KEY STUCK и др.)"},
      {"power_off", "Выключение (карточка перед сном)"},
  };
  for (const auto& e : k)
    if (strcmp(e[0], fixture) == 0) return e[1];
  return nullptr;
}

void cellEn(char* b, size_t n, const Binding& kb) {
  if (kb.shortAct == ActionId::None && kb.longAct == ActionId::None) { snprintf(b, n, "-"); return; }
  const KeyConfig kc = keyConfigFor(kb);
  char s[96] = "";
  if (kb.shortAct != ActionId::None && kb.shortAct != ActionId::ShowHint)
    snprintf(s, sizeof s, "%s %s%s", actionName(kb.shortAct, DocLang::En), kc.shortTrigger == Trigger::OnDown ? "D" : "C",
             kb.repeat ? " R" : "");
  else if (kb.shortAct == ActionId::ShowHint)
    snprintf(s, sizeof s, "hint \"%s\" C", str(kb.hintStr));
  if (kb.longAct != ActionId::None) {
    char l[64];
    snprintf(l, sizeof l, "%s**%s H%.1f**", s[0] ? " · " : "", actionName(kb.longAct, DocLang::En), kb.holdMs / 1000.0);
    strncat(s, l, sizeof s - strlen(s) - 1);
  }
  if (kb.labelStr) {
    char l[32];
    snprintf(l, sizeof l, " [%s%s]", str(kb.labelStr), kb.labelMode == LabelMode::PillOnHold ? " pill" : "");
    strncat(s, l, sizeof s - strlen(s) - 1);
  }
  snprintf(b, n, "%s", s);
}

void cellRu(char* b, size_t n, const Binding& kb) {
  if (kb.shortAct == ActionId::None && kb.longAct == ActionId::None) { snprintf(b, n, "—"); return; }
  const KeyConfig kc = keyConfigFor(kb);
  char s[320] = "";
  if (kb.shortAct != ActionId::None && kb.shortAct != ActionId::ShowHint)
    snprintf(s, sizeof s, "%s%s%s", actionName(kb.shortAct, DocLang::Ru),
             kc.shortTrigger == Trigger::OnDown ? "" : " (при отпускании)", kb.repeat ? ", с повтором" : "");
  else if (kb.shortAct == ActionId::ShowHint)
    snprintf(s, sizeof s, "коротко: подсказка «%s»", str(kb.hintStr));
  if (kb.longAct != ActionId::None) {
    char sec[16], l[160];
    snprintf(sec, sizeof sec, "%.1f", kb.holdMs / 1000.0);
    if (char* dot = strchr(sec, '.')) *dot = ',';
    snprintf(l, sizeof l, "%s**удерживать %s с: %s**", s[0] ? "; " : "", sec, actionName(kb.longAct, DocLang::Ru));
    strncat(s, l, sizeof s - strlen(s) - 1);
  }
  if (kb.labelStr) {
    char l[64];
    snprintf(l, sizeof l, " [%s%s]", str(kb.labelStr), kb.labelMode == LabelMode::PillOnHold ? ", при удержании" : "");
    strncat(s, l, sizeof s - strlen(s) - 1);
  }
  snprintf(b, n, "%s", s);
}

void header(FILE* out, DocLang lang) {
  if (lang == DocLang::En) {
    fprintf(out, "# Hiking Gadget v2 - keys per screen state\n\n");
    fprintf(out, "Generated by `hg_render --bindings` from the same `Bindings` the firmware dispatches (SPEC §2.4). "
                 "D = on Down, C = on click, H = hold seconds, R = repeat; [LABEL] = text next to the keycap.\n\n");
    fprintf(out, "| State (fixture) | ^ | v | # | * |\n|---|---|---|---|---|\n");
    return;
  }
  fprintf(out, "# Hiking Gadget v2 — что делают кнопки на каждом экране\n\n");
  fprintf(out, "Таблица сгенерирована командой `hg_render --bindings --lang ru` из тех же привязок, которые исполняет "
               "прошивка, поэтому всегда совпадает с ней. Руками не править: `tools/build.sh docs`. "
               "Английская версия: [BUTTONS.md](BUTTONS.md).\n\n");
  fprintf(out, "Как читать:\n\n"
               "- действие срабатывает в момент нажатия; «(при отпускании)» — только после отпускания, потому что у "
               "этой кнопки есть ещё действие по удержанию;\n"
               "- **жирным** — действие по удержанию и сколько секунд держать;\n"
               "- «с повтором» — при удержании действие повторяется и ускоряется;\n"
               "- [ТЕКСТ] — надпись рядом с кнопкой на экране; «при удержании» — надпись появляется, пока кнопку "
               "держат;\n"
               "- «—» — кнопка на этом экране ничего не делает.\n\n");
  fprintf(out, "Общие правила:\n\n"
               "- Пока экран погашен или притушен, первое нажатие любой кнопки только включает экран; больше это "
               "нажатие ничего не делает.\n"
               "- Сигнал таймера снимается любой кнопкой; это нажатие тоже больше ничего не делает.\n"
               "- Кнопка, зажатая дольше 10 с, отключается до отпускания и показывает карточку KEY STUCK; "
               "остальные кнопки работают.\n\n");
  fprintf(out, "| Экран | ^ | v | # | * |\n|---|---|---|---|---|\n");
}
}  // namespace

int writeBindingsDoc(FILE* out, DocLang lang) {
  header(out, lang);
  static char seen[128][700];
  int nSeen = 0, missing = 0;
  for (int i = 0; i < fixtureCount(); ++i) {
    FixtureCtx f = baseWorld();
    fixtureAt(i).fn(f);
    RenderOut o{};
    renderFixture(f, o);
    // Rows are de-duplicated on the English cells, so both files list the same states.
    char c[4][160], row[700];
    for (int k = 0; k < 4; ++k) cellEn(c[k], sizeof c[k], o.bindings.key[k]);
    if (o.bindings.captureAll) for (int k = 0; k < 4; ++k) snprintf(c[k], sizeof c[k], "captured (exit: hold 2 s, or 10 s idle)");
    snprintf(row, sizeof row, "%s|%s|%s|%s", c[0], c[1], c[2], c[3]);
    bool dup = false;
    for (int s = 0; s < nSeen; ++s) if (strcmp(seen[s], row) == 0) dup = true;
    if (dup || nSeen >= 128) continue;
    snprintf(seen[nSeen++], sizeof seen[0], "%s", row);
    if (lang == DocLang::En) {
      fprintf(out, "| `%s` | %s | %s | %s | %s |\n", fixtureAt(i).name, c[0], c[1], c[2], c[3]);
      continue;
    }
    char r[4][400];
    for (int k = 0; k < 4; ++k) cellRu(r[k], sizeof r[k], o.bindings.key[k]);
    if (o.bindings.captureAll)
      for (int k = 0; k < 4; ++k) snprintf(r[k], sizeof r[k], "перехвачена для проверки (выход: удерживать 2 с или не трогать 10 с)");
    const char* name = stateRu(fixtureAt(i).name);
    if (!name) {
      ++missing;
      fprintf(stderr, "bindings --lang ru: no Russian description for fixture '%s' (add it to stateRu)\n", fixtureAt(i).name);
    }
    fprintf(out, "| %s | %s | %s | %s | %s |\n", name ? name : fixtureAt(i).name, r[0], r[1], r[2], r[3]);
  }
  return missing;
}

}  // namespace hg
