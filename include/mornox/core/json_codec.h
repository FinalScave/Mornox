#pragma once

#include <string>
#include <string_view>

#include "mornox/core/result.h"
#include "mornox/core/value.h"

namespace mornox {

std::string ValueToJsonText(const Value& value);
Result<Value> ValueFromJsonText(std::string_view text);

}
