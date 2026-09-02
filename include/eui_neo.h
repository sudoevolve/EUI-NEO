#pragma once

#include "eui/dsl_app.h"
#include "eui/dsl.h"
#include "eui/image.h"
#include "eui/json.h"
#include "eui/network.h"
#include "eui/platform.h"
#include "eui/signal.h"
#include "eui/types.h"

#include "components/components.h"

// The xmake-repo app_runner variant supplies the platform loop from the
// framework package. Defining main here keeps the entry point in the user's
// translation unit, so static linkers do not need whole-archive flags.
#if defined(EUI_APP_RUNNER)
int eui_app_run();
int main() {
    return eui_app_run();
}
#endif
