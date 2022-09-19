#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(const std::string_view text) {
    std::vector<std::string_view> output;
    std::string_view inp  = text;
    std::string_view delims = " ";

    while (inp.begin() != inp.end()){
        const auto second = inp.find(delims);
        if(second == 0) {
          inp.remove_prefix(second + 1);
          continue;
        }
        output.emplace_back(inp.substr(0, second));
        inp.remove_prefix(second + 1);
        if (second == std::string_view::npos) break;
    }
    return output;
}
