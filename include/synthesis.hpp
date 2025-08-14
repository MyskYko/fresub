#pragma once

#include "fresub_aig.hpp"
#include "window.hpp"
#include "resub.hpp"

namespace fresub {

// Forward declaration of synthesis function
int synthesize_with_divisors(AIG& aig, const Window& window, 
                            const std::vector<int>& selected_divisors,
                            const TruthTable& target_function);

} // namespace fresub