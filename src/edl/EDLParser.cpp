#include "edl/EDLParser.h"
#include <fstream>
#include <stdexcept>

namespace edl {

EDL EDLParser::parse(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open EDL file: " + filename);
	}
	
	nlohmann::json j;
	try {
		file >> j;
	} catch (const nlohmann::json::parse_error& e) {
		throw std::runtime_error("Failed to parse EDL JSON: " + std::string(e.what()));
	}
	
	// Validate required fields
	if (!j.contains("tracks")) {
		throw std::runtime_error("EDL JSON missing required field: tracks");
	}
	if (!j.contains("sources")) {
		throw std::runtime_error("EDL JSON missing required field: sources");
	}
	
	return parseJSON(j);
}

EDL EDLParser::parseJSON(const nlohmann::json& j) {
	EDL edl;
	
	getIfExists(j, "fps", edl.fps);
	getIfExists(j, "width", edl.width);
	getIfExists(j, "height", edl.height);
	
	if (j.contains("clips")) {
		for (const auto& clipJson : j["clips"]) {
			edl.clips.push_back(parseClip(clipJson));
		}
	}
	
	return edl;
}

Source EDLParser::parseSource(const nlohmann::json& j) {
	// Check if this is an effect source or media source
	if (j.contains("type") && j["type"].is_string()) {
		// This is an effect source
		return parseEffectSource(j);
	} else {
		// This is a media source
		return parseMediaSource(j);
	}
}

MediaSource EDLParser::parseMediaSource(const nlohmann::json& j) {
	MediaSource source;
	
	getIfExists(j, "uri", source.uri);
	getIfExists(j, "trackId", source.trackId);
	getIfExists(j, "in", source.in);
	getIfExists(j, "out", source.out);
	getIfExists(j, "width", source.width);
	getIfExists(j, "height", source.height);
	getIfExists(j, "fps", source.fps);
	getIfExists(j, "rotation", source.rotation);
	getIfExists(j, "flip", source.flip);
	
	return source;
}

EffectSource EDLParser::parseEffectSource(const nlohmann::json& j) {
	EffectSource source;
	
	if (j.contains("type")) {
		source.type = j["type"];
	}
	if (j.contains("in")) {
		source.in = j["in"];
	}
	if (j.contains("out")) {
		source.out = j["out"];
	}
	if (j.contains("insideMaskFilters") && j["insideMaskFilters"].is_array()) {
		for (const auto& filterJson : j["insideMaskFilters"]) {
			source.insideMaskFilters.push_back(parseFilter(filterJson));
		}
	}
	if (j.contains("outsideMaskFilters") && j["outsideMaskFilters"].is_array()) {
		for (const auto& filterJson : j["outsideMaskFilters"]) {
			source.outsideMaskFilters.push_back(parseFilter(filterJson));
		}
	}
	if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
		for (const auto& cpJson : j["controlPoints"]) {
			source.controlPoints.push_back(parseShapeControlPoint(cpJson));
		}
	}
	if (j.contains("interpolation")) {
		source.interpolation = j["interpolation"];
	}
	
	return source;
}

Filter EDLParser::parseFilter(const nlohmann::json& j) {
	Filter filter;
	
	if (j.contains("type")) {
		filter.type = j["type"];
	}
	if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
		for (const auto& cpJson : j["controlPoints"]) {
			filter.controlPoints.push_back(parseFilterControlPoint(cpJson));
		}
	}
	
	return filter;
}

FilterControlPoint EDLParser::parseFilterControlPoint(const nlohmann::json& j) {
	FilterControlPoint cp;
	
	if (j.contains("point")) {
		cp.point = j["point"];
	}
	if (j.contains("linear") && j["linear"].is_array()) {
		for (const auto& linearJson : j["linear"]) {
			cp.linear.push_back(parseLinearMapping(linearJson));
		}
	}
	if (j.contains("bezier")) {
		BezierCurve bezier;
		if (j["bezier"].contains("srcTime")) {
			bezier.srcTime = j["bezier"]["srcTime"];
		}
		if (j["bezier"].contains("dstTime")) {
			bezier.dstTime = j["bezier"]["dstTime"];
		}
		cp.bezier = bezier;
	}
	
	return cp;
}

LinearMapping EDLParser::parseLinearMapping(const nlohmann::json& j) {
	LinearMapping mapping;
	
	if (j.contains("src")) {
		mapping.src = j["src"];
	}
	if (j.contains("dst")) {
		mapping.dst = j["dst"];
	}
	
	return mapping;
}

ShapeControlPoint EDLParser::parseShapeControlPoint(const nlohmann::json& j) {
	ShapeControlPoint cp;
	
	if (j.contains("point")) {
		cp.point = j["point"];
	}
	if (j.contains("panx")) {
		cp.panx = j["panx"];
	}
	if (j.contains("pany")) {
		cp.pany = j["pany"];
	}
	if (j.contains("zoomx")) {
		cp.zoomx = j["zoomx"];
	}
	if (j.contains("zoomy")) {
		cp.zoomy = j["zoomy"];
	}
	if (j.contains("rotate")) {
		cp.rotate = j["rotate"];
	}
	if (j.contains("shape")) {
		cp.shape = j["shape"];
	}
	
	return cp;
}

Track EDLParser::parseTrack(const nlohmann::json& j) {
	Track track;
	
	static const std::map<std::string, Track::Type> typeMap = {
		{"video", Track::Video},
		{"audio", Track::Audio},
		{"subtitle", Track::Subtitle},
		{"caption", Track::Caption}
	};
	
	getEnumIfExists(j, "type", track.type, typeMap);
	
	getIfExists(j, "number", track.number);
	getIfExists(j, "subtype", track.subtype);
	getIfExists(j, "subnumber", track.subnumber);
	
	return track;
}

Motion EDLParser::parseMotion(const nlohmann::json& j) {
	Motion motion;
	
	getIfExists(j, "panX", motion.panX);
	getIfExists(j, "panY", motion.panY);
	getIfExists(j, "zoomX", motion.zoomX);
	getIfExists(j, "zoomY", motion.zoomY);
	getIfExists(j, "rotation", motion.rotation);
	
	return motion;
}

Transition EDLParser::parseTransition(const nlohmann::json& j) {
	Transition transition;
	
	getIfExists(j, "type", transition.type);
	getIfExists(j, "duration", transition.duration);
	
	return transition;
}

Clip EDLParser::parseClip(const nlohmann::json& j) {
	Clip clip;
	
	getIfExists(j, "in", clip.in);
	getIfExists(j, "out", clip.out);
	getIfExists(j, "topFade", clip.topFade);
	getIfExists(j, "tailFade", clip.tailFade);
	
	if (j.contains("track")) {
		clip.track = parseTrack(j["track"]);
	}
	if (j.contains("source")) {
		clip.source = parseSource(j["source"]);
	}
	if (j.contains("motion")) {
		clip.motion = parseMotion(j["motion"]);
	}
	if (j.contains("transition")) {
		clip.transition = parseTransition(j["transition"]);
	}
	if (j.contains("effects") && j["effects"].is_array()) {
		for (const auto& effectJson : j["effects"]) {
			clip.effects.push_back(parseSimpleEffect(effectJson));
		}
	}
	
	return clip;
}

SimpleEffect EDLParser::parseSimpleEffect(const nlohmann::json& j) {
	SimpleEffect effect;
	
	if (j.contains("type")) {
		effect.type = j["type"];
	}
	if (j.contains("strength")) {
		effect.strength = j["strength"];
	}
	
	return effect;
}

} // namespace edl