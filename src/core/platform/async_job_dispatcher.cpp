#include "mornox/platform/async_job_dispatcher.h"

#include <utility>

#include "mornox/platform/async.h"

namespace mornox {

JobDispatcher AsyncJobDispatcher(AsyncRuntime& runtime) {
    return {
        .worker = [&runtime](JobTask task) {
            runtime.PostWorker(std::move(task));
        },
        .main = [&runtime](JobTask task) {
            runtime.PostMain(std::move(task));
        },
    };
}

}
