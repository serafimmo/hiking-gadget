#pragma once
// docs/BUTTONS.md and docs/BUTTONS_RU.md generator: the key table of every fixture state, from the same Bindings
// the device runs. The Russian texts are documentation-only strings here; the device UI stays ASCII English.
#include <cstdio>

namespace hg {
enum class DocLang { En, Ru };
// Returns the number of printed states without a Russian description (always 0 for En); non-zero fails the build.
int writeBindingsDoc(FILE* out, DocLang lang);
}  // namespace hg
