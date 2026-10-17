// SETTINGS cover, list, picker, set time (SPEC §3.8.5, §2.5)
#include <stdio.h>
#include <string.h>

#include "../../config/timing.h"
#include "../../core/tz.h"
#include "../canvas.h"
#include "../fmt.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
constexpr int16_t kListTop = 10, kListBottom = 64;
constexpr uint8_t kRowsMax = 32;

struct Rows {
  ListRow r[kRowsMax];
  uint8_t n = 0;
  Rows() {
    int last = -1;
    for (int i = 0; i < (int)SettingId::Count; ++i) {
      const SettingDef& d = settingDef((SettingId)i);
      if ((int)d.section != last) { r[n++] = ListRow{true, d.section, (SettingId)i}; last = (int)d.section; }
      r[n++] = ListRow{false, d.section, (SettingId)i};
    }
  }
};
const Rows& rows() { static const Rows k; return k; }

int16_t settingValue(const ViewModel& vm, SettingId id) { return vm.settings ? vm.settings->get(id) : settingDef(id).def; }

char* upperCopy(char* b, size_t n, const char* s) { strncpy(b, s, n - 1); b[n - 1] = 0; toUpper(b); return b; }
}  // namespace

uint8_t listRowCount() { return rows().n; }
ListRow listRow(uint8_t i) { return rows().r[i < rows().n ? i : 0]; }
uint8_t listRowOf(SettingId id) {
  for (uint8_t i = 0; i < rows().n; ++i) if (!rows().r[i].header && rows().r[i].id == id) return i;
  return 0;
}
int16_t listRowHeight(uint8_t i) { return listRow(i).header ? 9 : 11; }

uint8_t listScroll(uint8_t top, SettingId cursor, int dir) {
  const uint8_t c = listRowOf(cursor);
  // the row we must see in full: the cursor, or the one after it when moving down (look-ahead) if it exists
  uint8_t need = c;
  if (dir > 0 && c + 1 < listRowCount()) need = (uint8_t)(c + 1);
  if (c < top) top = (c > 0 && listRow((uint8_t)(c - 1)).header) ? (uint8_t)(c - 1) : c;
  for (;;) {
    int16_t y = kListTop;
    for (uint8_t i = top; i <= need && i < listRowCount(); ++i) y = (int16_t)(y + listRowHeight(i));
    if (y <= kListBottom || top >= c) break;
    ++top;
  }
  return top;
}

bool listRowEditable(SettingId id, const FormatCtx& ctx) {
  const SettingKind k = settingDef(id).kind;
  return (k == SettingKind::Enum || k == SettingKind::Number) && editable(id, ctx.sht, ctx.mag) == NotEditable::No;
}

namespace {
// ---------------------------------------------------------------- Cover
class CoverScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState&, Bindings& b) const override {
    b.key[(int)Key::Up] = pageKey(ActionId::PagePrev);
    b.key[(int)Key::Down] = pageKey(ActionId::PageNext);
    b.key[(int)Key::Star] = act(ActionId::OpenList, IconId::Enter, S_L_OPEN, LabelMode::Static);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    char clk[8];
    statusClock(vm, clk, sizeof clk);
    const ChipSet chips = chipsFor(vm.bg, ScreenId::SettingsCover);
    widgets::statusLine(c, str(S_SET_TITLE), nullptr, clk, &chips);
    const SettingId ids[4] = {SettingId::TimeZone, SettingId::ScreenOff, SettingId::Heater, SettingId::Declination};
    char v[32];
    c.font(Font::Body);
    for (int i = 0; i < 4; ++i) {
      const int16_t ink = (int16_t)(12 + 10 * i);
      c.text(0, ink, str(settingLabel(ids[i])));
      formatSettingValue(ids[i], settingValue(vm, ids[i]), ValueStyle::Cover, vm.fmt, v, sizeof v);
      c.text(kContentX1, ink, v, Align::Right);
    }
    c.text(0, 56, str(S_SET_ITEMS));
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};

// ---------------------------------------------------------------- List
class ListScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState& ui, Bindings& b) const override {
    Binding up = pageKey(ActionId::ListUp), dn = pageKey(ActionId::ListDown);
    up.repeat = dn.repeat = true;
    b.key[(int)Key::Up] = up;
    b.key[(int)Key::Down] = dn;
    b.chevrons = true;
    b.key[(int)Key::Hash] = act(ActionId::ListBack, IconId::Back);
    const SettingId id = ui.listCursor;
    switch (settingDef(id).kind) {
      case SettingKind::Enum:
      case SettingKind::Number:
        if (listRowEditable(id, vm.fmt)) b.key[(int)Key::Star] = act(ActionId::ListActivate, IconId::Edit);
        break;
      case SettingKind::Action:
        if (id != SettingId::Calibrate || editable(id, vm.fmt.sht, vm.fmt.mag) == NotEditable::No)
          b.key[(int)Key::Star] = act(ActionId::ListActivate, IconId::Enter);
        break;
      case SettingKind::ActionHold:
        b.key[(int)Key::Star] = holdAct(ActionId::ShowHint, ActionId::ListActivate, kHoldCriticalMs, IconId::Power,
                                        IconId::None, S_L_HOLD, LabelMode::PillOnHold,
                                        id == SettingId::PowerOff ? S_T_HOLD_POWER : S_T_HOLD_RESET);
        break;
      default:
        break;
    }
  }
  void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs) const override {
    char pos[8];
    snprintf(pos, sizeof pos, "%d/%d", (int)ui.listCursor + 1, (int)SettingId::Count);
    c.scope(DrawScope::Status);
    c.font(Font::Micro);
    c.text(0, kStatusInk, str(S_SET_TITLE));
    c.num(kContentX1, kStatusInk, pos, Align::Right);
    widgets::rule(c, kStatusRuleY);
    c.scope(DrawScope::Content);
    int16_t y = kListTop;
    uint8_t shown = 0;
    for (uint8_t i = ui.listTop; i < listRowCount(); ++i) {
      const int16_t h = listRowHeight(i);
      if (y + h > kListBottom) break;
      const ListRow r = listRow(i);
      if (r.header) {
        char t[16];
        upperCopy(t, sizeof t, str(sectionTitle(r.section)));
        c.font(Font::Micro);
        const int16_t w = c.text(1, (int16_t)(y + 2), t);
        c.dottedH((int16_t)(w + 4), (int16_t)(y + 4), (int16_t)(112 - (w + 4) + 1));
      } else {
        const bool sel = r.id == ui.listCursor;
        if (sel) c.rbox(Rect{0, y, 114, 11}, 2);
        c.color(sel ? 0 : 1);
        c.font(Font::Body);
        c.text(3, (int16_t)(y + 2), str(settingLabel(r.id)));
        const SettingKind k = settingDef(r.id).kind;
        char v[32];
        formatSettingValue(r.id, settingValue(vm, r.id), ValueStyle::List, vm.fmt, v, sizeof v);
        if (k == SettingKind::Action) {
          c.icon(106, (int16_t)(y + 2), IconId::Next);
          if (v[0]) c.text(103, (int16_t)(y + 2), v, Align::Right);
        } else if (v[0]) {
          c.text(111, (int16_t)(y + 2), v, Align::Right);
        }
        c.color(1);
      }
      y = (int16_t)(y + h);
      ++shown;
    }
    // scrollbar: dotted track x 116, y 10..63; thumb box(115, y, 2, h)
    c.dottedV(116, kListTop, kListBottom - kListTop);
    const int total = listRowCount();
    int16_t th = (int16_t)(54 * shown / total);
    if (th < 6) th = 6;
    const int16_t ty = (int16_t)(kListTop + (54 - th) * ui.listTop / (total - shown > 0 ? total - shown : 1));
    c.box(Rect{115, ty, 2, th});
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};

// ---------------------------------------------------------------- Picker
int16_t pickerNeighbour(SettingId id, int16_t v, int dir) {   // dir -1 = shown above, +1 = shown below
  const SettingDef& d = settingDef(id);
  if (d.kind == SettingKind::Number) return (int16_t)(v + (dir < 0 ? d.step : -d.step));   // larger value above
  return (int16_t)(v + dir);                                                                // previous option above
}

class PickerScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState& ui, Bindings& b) const override {
    const SettingDef& d = settingDef(ui.listCursor);
    Binding up = pageKey(ActionId::PickUp), dn = pageKey(ActionId::PickDown);
    up.repeat = dn.repeat = true;
    up.accel = dn.accel = d.accel;
    b.key[(int)Key::Up] = up;
    b.key[(int)Key::Down] = dn;
    b.chevrons = true;
    b.key[(int)Key::Hash] = act(ActionId::PickCancel, IconId::X);
    if (listRowEditable(ui.listCursor, vm.fmt)) b.key[(int)Key::Star] = act(ActionId::SettingSave, IconId::Check);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs) const override {
    const SettingId id = ui.listCursor;
    const SettingDef& d = settingDef(id);
    char t[40], pos[8];
    upperCopy(t, sizeof t, str(settingLabel(id)));
    snprintf(pos, sizeof pos, "%d/%d", (int)id + 1, (int)SettingId::Count);
    c.scope(DrawScope::Status);
    c.font(Font::Micro);
    c.text(0, kStatusInk, t);
    c.num(kContentX1, kStatusInk, pos, Align::Right);
    widgets::rule(c, kStatusRuleY);
    c.scope(DrawScope::Content);
    const int16_t v = ui.pending;
    const bool editableNow = listRowEditable(id, vm.fmt);
    const ValueStyle nst = id == SettingId::TimeZone ? ValueStyle::Neighbour : ValueStyle::Picker;
    c.font(Font::Body);
    if (editableNow) {
      const int16_t above = pickerNeighbour(id, v, -1), below = pickerNeighbour(id, v, +1);
      if (isValid(id, above)) { formatSettingValue(id, above, nst, vm.fmt, t, sizeof t); c.text(58, 11, t, Align::Center); }
      if (isValid(id, below)) { formatSettingValue(id, below, nst, vm.fmt, t, sizeof t); c.text(58, 52, t, Align::Center); }
    }
    c.rframe(Rect{0, 19, 116, 30}, 3);
    formatSettingValue(id, v, ValueStyle::Picker, vm.fmt, t, sizeof t);
    if (d.kind == SettingKind::Number) {
      c.font(Font::Mid);
      c.num(58, 22, t, Align::Center);
    } else {
      c.font(Font::Value);
      if (c.width(t) > 110) c.font(Font::Label);   // generic width rule (YEKATERINBURG)
      c.text(58, 23, t, Align::Center);
    }
    formatSettingSubLabel(id, v, vm.fmt, t, sizeof t);
    c.font(Font::Body);
    if (t[0]) c.text(58, 39, t, Align::Center);
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState& ui) const override {
    return ui.listCursor == SettingId::Mount || ui.listCursor == SettingId::TempOffset ? 200 : 0;   // live previews
  }
  uint8_t sensorDemand(const ViewModel&, const UiState& ui) const override {
    return ui.listCursor == SettingId::Mount ? 1 : 0;
  }
};

// ---------------------------------------------------------------- Set time
class SetTimeScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState& ui, Bindings& b) const override {
    Binding up = pageKey(ActionId::TimeInc), dn = pageKey(ActionId::TimeDec);
    up.repeat = dn.repeat = true;
    up.accel = dn.accel = ui.timeField == 4;   // the year
    b.key[(int)Key::Up] = up;
    b.key[(int)Key::Down] = dn;
    b.chevrons = true;
    b.key[(int)Key::Hash] = act(ActionId::TimeFieldPrev, IconId::Back);
    b.key[(int)Key::Star] = ui.timeField >= 4 ? act(ActionId::SetTimeCommit, IconId::Check)
                                               : act(ActionId::TimeFieldNext, IconId::Next);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs) const override {
    char t[24], pos[8], zone[8];
    snprintf(zone, sizeof zone, "%s", vm.clock.zone);
    snprintf(t, sizeof t, str(S_SET_TIME_TITLE), zone);
    snprintf(pos, sizeof pos, "%u/5", ui.timeField + 1u);
    c.scope(DrawScope::Status);
    c.font(Font::Micro);
    c.text(0, kStatusInk, t);
    c.num(kContentX1, kStatusInk, pos, Align::Right);
    widgets::rule(c, kStatusRuleY);
    c.scope(DrawScope::Content);
    const Civil& e = ui.timeEdit;
    char hh[4], mm[4];
    snprintf(hh, sizeof hh, "%02u", e.hour % 100u);
    snprintf(mm, sizeof mm, "%02u", e.minute % 100u);
    c.font(Font::BigS);
    const int16_t wh = c.numWidth(hh), wc = c.numWidth(":");
    const int16_t xh = 22, xc = (int16_t)(xh + wh), xm = (int16_t)(xc + wc);
    for (int f = 0; f < 2; ++f) {
      const int16_t x = f ? xm : xh;
      const char* s = f ? mm : hh;
      if (ui.timeField == f) {
        c.rbox(Rect{(int16_t)(x - 2), 11, (int16_t)(wh + 4), 28}, 2);
        c.color(0);
      }
      c.num(x, 13, s);
      c.color(1);
    }
    c.num(xc, 13, ":");
    // date line: "SUN 11.10.2026", centred at 58, the active field inverted
    char dd[4], mo[4], yy[8], wd[8];
    snprintf(dd, sizeof dd, "%02u", e.day % 100u);
    snprintf(mo, sizeof mo, "%02u", e.month % 100u);
    snprintf(yy, sizeof yy, "%04d", (int)e.year);
    snprintf(wd, sizeof wd, "%s ", weekdayAbbr(e.weekday));
    c.font(Font::Label);
    const char* parts[6] = {wd, dd, ".", mo, ".", yy};
    int16_t total = 0;
    for (const char* p : parts) total = (int16_t)(total + c.numWidth(p));
    int16_t x = (int16_t)(58 - total / 2);
    for (int i = 0; i < 6; ++i) {
      const int16_t w = c.numWidth(parts[i]);
      const bool active = (i == 1 && ui.timeField == 2) || (i == 3 && ui.timeField == 3) || (i == 5 && ui.timeField == 4);
      if (active) { c.rbox(Rect{(int16_t)(x - 1), 43, (int16_t)(w + 2), 11}, 1); c.color(0); }
      c.num(x, 45, parts[i]);
      c.color(1);
      x = (int16_t)(x + w);
    }
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};

const CoverScreen kCover;
const ListScreen kList;
const PickerScreen kPicker;
const SetTimeScreen kSetTime;
}  // namespace

const Screen& settingsCoverScreen() { return kCover; }
const Screen& settingsListScreen() { return kList; }
const Screen& pickerScreen() { return kPicker; }
const Screen& setTimeScreen() { return kSetTime; }

}  // namespace hg
