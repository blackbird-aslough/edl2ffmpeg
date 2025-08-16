#pragma once

#include "EDLTypes.h"
#include <nlohmann/json.hpp>
#include <string>
#include <map>

namespace edl {

class EDLParser {
public:
	static EDL parse(const std::string& filename);
	static EDL parseJSON(const nlohmann::json& j);
	
private:
	// Helper template to safely get JSON values
	template<typename T>
	static void getIfExists(const nlohmann::json& j, const std::string& key, T& value) {
		if (j.contains(key)) {
			value = j[key].get<T>();
		}
	}
	
	// Helper for string to enum mappings
	template<typename EnumType>
	static void getEnumIfExists(const nlohmann::json& j, const std::string& key, 
		EnumType& value, const std::map<std::string, EnumType>& mapping) {
		if (j.contains(key)) {
			std::string str = j[key];
			auto it = mapping.find(str);
			if (it != mapping.end()) {
				value = it->second;
			}
		}
	}
	
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