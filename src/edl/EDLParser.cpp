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
	file >> j;
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
	Source source;
	
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
	
	return clip;
}

} // namespace edl