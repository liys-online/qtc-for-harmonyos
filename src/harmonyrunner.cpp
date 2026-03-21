#include "harmonyrunner.h"

#include "ohosconstants.h"

#include <projectexplorer/runcontrol.h>

namespace Ohos::Internal {

void setupHarmonyRunWorker()
{
    static ProjectExplorer::ProcessRunnerFactory theHarmonyRunWorkerFactory(
        {Constants::HARMONY_RUNCONFIG_ID});
}

} // namespace Ohos::Internal
