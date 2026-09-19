#pragma once
#include "rr64_prediction_resources.hpp"
#include "rr64_prediction_context.hpp"
namespace rr64::prediction {
// Isolated worker only. The caller supplies a disposable historical image and
// a valid private stack. An unrepresented live task cannot be restarted here.
enum class ResourceDrain { Idle, UnrepresentedTask, Invalid, Blocked, Reached };
// An optional observed task inventory limits background work at a historical
// streaming boundary. Completions still come from the original private worker.
ResourceDrain drain_resource_worker(Resources&,const CpuContext&,const ResourceInventory* target=nullptr);
}
