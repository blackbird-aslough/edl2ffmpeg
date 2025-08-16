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
	
	if (j.contains("fps")) {
		edl.fps = j["fps"];
	}
	if (j.contains("width")) {
		edl.width = j["width"];
	}
	if (j.contains("height")) {
		edl.height = j["height"];
	}
	
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
	
	if (j.contains("uri")) {
		source.uri = j["uri"];
	}
	if (j.contains("trackId")) {
		source.trackId = j["trackId"];
	}
	if (j.contains("in")) {
		source.in = j["in"];
	}
	if (j.contains("out")) {
		source.out = j["out"];
	}
	if (j.contains("width")) {
		source.width = j["width"];
	}
	if (j.contains("height")) {
		source.height = j["height"];
	}
	if (j.contains("fps")) {
		source.fps = j["fps"];
	}
	if (j.contains("rotation")) {
		source.rotation = j["rotation"];
	}
	if (j.contains("flip")) {
		source.flip = j["flip"];
	}
	
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
	
	if (j.contains("type")) {
		std::string typeStr = j["type"];
		if (typeStr == "video") {
			track.type = Track::Video;
		} else if (typeStr == "audio") {
			track.type = Track::Audio;
		} else if (typeStr == "subtitle") {
			track.type = Track::Subtitle;
		} else if (typeStr == "caption") {
			track.type = Track::Caption;
		}
	}
	
	if (j.contains("number")) {
		track.number = j["number"];
	}
	if (j.contains("subtype")) {
		track.subtype = j["subtype"];
	}
	if (j.contains("subnumber")) {
		track.subnumber = j["subnumber"];
	}
	
	return track;
}

Motion EDLParser::parseMotion(const nlohmann::json& j) {
	Motion motion;
	
	if (j.contains("panX")) {
		motion.panX = j["panX"];
	}
	if (j.contains("panY")) {
		motion.panY = j["panY"];
	}
	if (j.contains("zoomX")) {
		motion.zoomX = j["zoomX"];
	}
	if (j.contains("zoomY")) {
		motion.zoomY = j["zoomY"];
	}
	if (j.contains("rotation")) {
		motion.rotation = j["rotation"];
	}
	
	return motion;
}

Transition EDLParser::parseTransition(const nlohmann::json& j) {
	Transition transition;
	
	if (j.contains("type")) {
		transition.type = j["type"];
	}
	if (j.contains("duration")) {
		transition.duration = j["duration"];
	}
	
	return transition;
}

Clip EDLParser::parseClip(const nlohmann::json& j) {
	Clip clip;
	
	if (j.contains("in")) {
		clip.in = j["in"];
	}
	if (j.contains("out")) {
		clip.out = j["out"];
	}
	if (j.contains("track")) {
		clip.track = parseTrack(j["track"]);
	}
	if (j.contains("source")) {
		clip.source = parseSource(j["source"]);
	}
	if (j.contains("topFade")) {
		clip.topFade = j["topFade"];
	}
	if (j.contains("tailFade")) {
		clip.tailFade = j["tailFade"];
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