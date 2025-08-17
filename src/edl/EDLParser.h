#pragma once

#include "EDLTypes.h"
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <set>

namespace edl {

class EDLParser {
public:
	static EDL parse(const std::string& filename);
	static EDL parseJSON(const nlohmann::json& j);
	
private:
	// Helper methods for validation and extraction
	template<typename T>
	static void getIfExists(const nlohmann::json& j, const std::string& key, T& value) {
		if (j.contains(key) && !j[key].is_null()) {
			value = j[key].get<T>();
		}
	}
	
	template<typename EnumT>
	static void getEnumIfExists(const nlohmann::json& j, const std::string& key, 
								EnumT& value, const std::map<std::string, EnumT>& mapping) {
		if (j.contains(key) && j[key].is_string()) {
			auto strValue = j[key].get<std::string>();
			auto it = mapping.find(strValue);
			if (it != mapping.end()) {
				value = it->second;
			}
		}
	}
	
	// Validation helpers
	static bool hasNonNullKey(const nlohmann::json& j, const std::string& key);
	static void ensureOnlyKeys(const nlohmann::json& j, const std::string& objectName, 
							   const std::set<std::string>& allowedKeys);
	static std::string getUniqueNonNullKey(const nlohmann::json& j, const std::string& objectName,
										   const std::set<std::string>& exclusiveKeys);
	
	// Required field extractors with validation
	static std::string getString(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static double getDouble(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static double getNonNegativeDouble(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static int getInteger(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static int getPositiveInteger(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static bool getBoolean(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static nlohmann::json getObject(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	static nlohmann::json getArray(const nlohmann::json& j, const std::string& objectName, const std::string& key);
	
	// Interval validation
	static std::pair<double, double> getInterval(const nlohmann::json& j, const std::string& objectName);
	
	// Parsing methods for different structures
	static Source parseSource(const nlohmann::json& j, const Track& track);
	static MediaSource parseMediaSource(const nlohmann::json& j);
	static GenerateSource parseGenerateSource(const nlohmann::json& j);
	static LocationSource parseLocationSource(const nlohmann::json& j);
	static EffectSource parseEffectSource(const nlohmann::json& j);
	static TransformSource parseTransformSource(const nlohmann::json& j);
	static SubtitleSource parseSubtitleSource(const nlohmann::json& j);
	static ShapeControlPoint parseShapeControlPoint(const nlohmann::json& j);
	static Track parseTrack(const nlohmann::json& j);
	static Motion parseMotion(const nlohmann::json& j);
	static Transition parseTransition(const nlohmann::json& j);
	static TextFormat parseTextFormat(const nlohmann::json& j);
	static Clip parseClip(const nlohmann::json& j);
	
	// Track alignment and organization
	static void alignTracksWithNullClips(EDL& edl);
	static std::string getTrackKey(const Track& track);
	static void validateUnsupportedFeatures(const nlohmann::json& j);
	
	// Feature support checking
	static void checkUnsupportedClipFeatures(const nlohmann::json& clip);
	static void checkUnsupportedSourceFeatures(const nlohmann::json& source);
};;

} // namespace edl