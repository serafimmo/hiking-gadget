#include "strings.h"

namespace hg {
namespace {
struct StrRec { const char* en; StrRole role; const char* worst; };
const StrRec kStr[S_COUNT] = {
#define HG_STR(id, role, en, worst) {en, StrRole::role, worst},
    {"", StrRole::Free, ""},
#include "strings_core.h"
#include "strings_screens_a.h"
#include "strings_screens_b.h"
#undef HG_STR
};
}  // namespace

const char* str(uint16_t id) { return id < S_COUNT ? kStr[id].en : ""; }
StrRole strRole(uint16_t id) { return id < S_COUNT ? kStr[id].role : StrRole::Free; }
const char* strWorstArg(uint16_t id) { return id < S_COUNT ? kStr[id].worst : ""; }

}  // namespace hg
