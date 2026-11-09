#pragma once

#include <QString>

namespace kernel {

enum class State { Unsupported, Disabled, Enabled, Changing };

State state();
quint64 enableSeq();

/* The running scheduler as a program name, e.g. "scx_bpfland", empty when none */
QString running();

} // namespace kernel
