#pragma once

#include "mornox/execution/job_service.h"

namespace mornox {

class AsyncRuntime;

JobDispatcher AsyncJobDispatcher(AsyncRuntime& runtime);

}
