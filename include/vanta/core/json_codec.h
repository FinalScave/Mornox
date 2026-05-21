#pragma once

#include <string>
#include <string_view>

#include "vanta/core/result.h"
#include "vanta/core/value.h"

namespace vanta {

std::string ValueToJsonText(const Value& value);
Result<Value> ValueFromJsonText(std::string_view text);

}
