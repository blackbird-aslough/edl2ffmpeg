#pragma once

#include "EDLTypes.h"
#include <nlohmann/json.hpp>
#include <string>

namespace edl {

class EDLParser {
public:
	static EDL parse(const std::string& filename);
	static EDL parseJSON(const nlohmann::json& j);
	
private:
	static Source parseSource(const nlohmann::json& j);
	static MediaSource parseMediaSource(const nlohmann::json& j);
	static EffectSource parseEffectSource(const nlohmann::json& j);
	static Filter parseFilter(const nlohmann::json& j);
	static FilterControlPoint parseFilterControlPoint(const nlohmann::json& j);
	static LinearMapping parseLinearMapping(const nlohmann::json& j);
	static ShapeControlPoint parseShapeControlPoint(const nlohmann::json& j);
	static Track parseTrack(const nlohmann::json& j);
	static Motion parseMotion(const nlohmann::json& j);
	static Transition parseTransition(const nlohmann::json& j);
	static SimpleEffect parseSimpleEffect(const nlohmann::json& j);
	static Clip parseClip(const nlohmann::json& j);
};

} // namespace edl