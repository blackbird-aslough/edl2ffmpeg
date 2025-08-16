#include "utils/Logger.h"

namespace utils {

std::atomic<Logger::Level> Logger::currentLevel{Logger::INFO};

} // namespace utils