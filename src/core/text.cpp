#include "mornox/core/text.h"

#include <algorithm>

namespace mornox {

std::size_t OffsetForPosition(const std::string& text, TextPosition position) {
    std::size_t offset = 0;
    int current_line = 0;
    while (offset < text.size() && current_line < position.line) {
        if (text[offset] == '\n') {
            ++current_line;
        }
        ++offset;
    }

    const std::size_t line_start = offset;
    while (offset < text.size() && text[offset] != '\n' && offset - line_start < static_cast<std::size_t>(std::max(0, position.character))) {
        ++offset;
    }
    return offset;
}

std::string ApplyTextEdit(const std::string& text, const TextEdit& edit) {
    const std::size_t start = OffsetForPosition(text, edit.range.start);
    const std::size_t end = OffsetForPosition(text, edit.range.end);
    std::string result;
    result.reserve(text.size() + edit.replacement_text.size());
    result.append(text.substr(0, start));
    result.append(edit.replacement_text);
    result.append(text.substr(std::min(end, text.size())));
    return result;
}

}
