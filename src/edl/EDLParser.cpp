#include "edl/EDLParser.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace edl {

// ============================================================================
// Public interface
// ============================================================================

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
	
	return parseJSON(j);
}

EDL EDLParser::parseJSON(const nlohmann::json& j) {
	EDL edl;
	
	// Check for unsupported features first
	validateUnsupportedFeatures(j);
	
	// Parse global settings
	getIfExists(j, "fps", edl.fps);
	getIfExists(j, "width", edl.width);
	getIfExists(j, "height", edl.height);
	
	// Validate FPS
	if (edl.fps <= 0) {
		throw InvalidEdlException("FPS must be positive: " + std::to_string(edl.fps));
	}
	
	// Parse clips
	if (j.contains("clips")) {
		for (const auto& clipJson : j["clips"]) {
			Clip clip = parseClip(clipJson);
			
			// Skip caption tracks (like reference parser)
			if (clip.track.type == Track::Caption) {
				continue;
			}
			
			edl.clips.push_back(clip);
		}
	}
	
	// Organize clips into tracks and apply alignment
	alignTracksWithNullClips(edl);
	
	return edl;
}

// ============================================================================
// Validation helpers
// ============================================================================

bool EDLParser::hasNonNullKey(const nlohmann::json& j, const std::string& key) {
	return j.contains(key) && !j[key].is_null();
}

void EDLParser::ensureOnlyKeys(const nlohmann::json& j, const std::string& objectName, 
							   const std::set<std::string>& allowedKeys) {
	std::ostringstream badKeys;
	for (const auto& [key, value] : j.items()) {
		if (!allowedKeys.count(key)) {
			badKeys << " " << key;
		}
	}
	if (!badKeys.str().empty()) {
		throw InvalidEdlException(objectName + " contains unsupported keys:" + badKeys.str());
	}
}

std::string EDLParser::getUniqueNonNullKey(const nlohmann::json& j, const std::string& objectName,
										   const std::set<std::string>& exclusiveKeys) {
	std::set<std::string> foundKeys;
	for (const auto& key : exclusiveKeys) {
		if (hasNonNullKey(j, key)) {
			foundKeys.insert(key);
		}
	}
	
	if (foundKeys.size() == 1) {
		return *foundKeys.begin();
	}
	
	std::ostringstream errorMessage;
	if (foundKeys.empty()) {
		errorMessage << objectName << " must contain one of the keys:";
		for (const auto& key : exclusiveKeys) {
			errorMessage << " " << key;
		}
	} else {
		errorMessage << objectName << " can only contain one of the keys:";
		for (const auto& key : foundKeys) {
			errorMessage << " " << key;
		}
	}
	throw InvalidEdlException(errorMessage.str());
}

// ============================================================================
// Required field extractors
// ============================================================================

std::string EDLParser::getString(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	if (!j.contains(key)) {
		throw InvalidEdlException(objectName + " must have " + key);
	}
	if (!j[key].is_string()) {
		throw InvalidEdlException(key + " must be a string in " + objectName);
	}
	return j[key].get<std::string>();
}

double EDLParser::getDouble(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	if (!j.contains(key)) {
		throw InvalidEdlException(objectName + " must have " + key);
	}
	if (!j[key].is_number()) {
		throw InvalidEdlException(key + " must be a number in " + objectName);
	}
	return j[key].get<double>();
}

double EDLParser::getNonNegativeDouble(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	double val = getDouble(j, objectName, key);
	if (val < 0) {
		throw InvalidEdlException(key + " must be non-negative in " + objectName + ": " + std::to_string(val));
	}
	return val;
}

int EDLParser::getInteger(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	if (!j.contains(key)) {
		throw InvalidEdlException(objectName + " must have " + key);
	}
	if (!j[key].is_number_integer()) {
		throw InvalidEdlException(key + " must be an integer in " + objectName);
	}
	return j[key].get<int>();
}

int EDLParser::getPositiveInteger(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	int val = getInteger(j, objectName, key);
	if (val <= 0) {
		throw InvalidEdlException(key + " must be positive in " + objectName + ": " + std::to_string(val));
	}
	return val;
}

bool EDLParser::getBoolean(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	if (!j.contains(key)) {
		throw InvalidEdlException(objectName + " must have " + key);
	}
	if (!j[key].is_boolean()) {
		throw InvalidEdlException(key + " must be a boolean in " + objectName);
	}
	return j[key].get<bool>();
}

nlohmann::json EDLParser::getObject(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	if (!j.contains(key)) {
		throw InvalidEdlException(objectName + " must have " + key);
	}
	if (!j[key].is_object()) {
		throw InvalidEdlException(key + " must be an object in " + objectName);
	}
	return j[key];
}

nlohmann::json EDLParser::getArray(const nlohmann::json& j, const std::string& objectName, const std::string& key) {
	if (!j.contains(key)) {
		throw InvalidEdlException(objectName + " must have " + key);
	}
	if (!j[key].is_array()) {
		throw InvalidEdlException(key + " must be an array in " + objectName);
	}
	return j[key];
}

std::pair<double, double> EDLParser::getInterval(const nlohmann::json& j, const std::string& objectName) {
	double in = getNonNegativeDouble(j, objectName, "in");
	double out = getNonNegativeDouble(j, objectName, "out");
	if (in >= out) {
		throw InvalidEdlException("In point must be before out point in " + objectName + 
								 ": in=" + std::to_string(in) + ", out=" + std::to_string(out));
	}
	return {in, out};
}

// ============================================================================
// Source parsing
// ============================================================================

Source EDLParser::parseSource(const nlohmann::json& j, const Track& track) {
	// Determine source type based on keys present
	if (track.subtype == "effects") {
		return parseEffectSource(j);
	} else if (track.subtype == "transform" || track.subtype == "colour" || 
			  track.subtype == "pan" || track.subtype == "level") {
		return parseTransformSource(j);
	} else if (track.type == Track::Subtitle || track.type == Track::Burnin) {
		return parseSubtitleSource(j);
	} else if (hasNonNullKey(j, "generate")) {
		return parseGenerateSource(j);
	} else if (hasNonNullKey(j, "location")) {
		return parseLocationSource(j);
	} else if (hasNonNullKey(j, "uri")) {
		return parseMediaSource(j);
	} else {
		throw InvalidEdlException("Unknown source type");
	}
}

MediaSource EDLParser::parseMediaSource(const nlohmann::json& j) {
	MediaSource source;
	
	source.uri = getString(j, "source", "uri");
	auto [in, out] = getInterval(j, "source");
	source.in = in;
	source.out = out;
	
	// Optional fields
	getIfExists(j, "trackId", source.trackId);
	getIfExists(j, "width", source.width);
	getIfExists(j, "height", source.height);
	getIfExists(j, "fps", source.fps);
	getIfExists(j, "speed", source.speed);
	getIfExists(j, "gamma", source.gamma);
	getIfExists(j, "audiomix", source.audiomix);
	
	// Check for unsupported features
	checkUnsupportedSourceFeatures(j);
	
	return source;
}

GenerateSource EDLParser::parseGenerateSource(const nlohmann::json& j) {
	GenerateSource source;
	
	auto generate = getObject(j, "source", "generate");
	auto generateType = getString(generate, "generate", "type");
	
	// Parse type
	if (generateType == "black") {
		source.type = GenerateSource::Black;
	} else if (generateType == "colour") {
		source.type = GenerateSource::Colour;
	} else if (generateType == "testpattern") {
		source.type = GenerateSource::TestPattern;
	} else if (generateType == "demo") {
		source.type = GenerateSource::Demo;
	} else {
		throw InvalidEdlException("Unsupported generate type: " + generateType + 
								 ". Only 'black' is currently supported.");
	}
	
	// For now, only support black
	if (source.type != GenerateSource::Black) {
		throw InvalidEdlException("Generate type '" + generateType + 
								 "' is not yet supported. Only 'black' is currently supported.");
	}
	
	auto [in, out] = getInterval(j, "source");
	source.in = in;
	source.out = out;
	
	// Generated sources require width and height
	source.width = getPositiveInteger(j, "source", "width");
	source.height = getPositiveInteger(j, "source", "height");
	
	// Store any extra parameters for future use
	for (const auto& [key, value] : generate.items()) {
		if (key != "type") {
			if (value.is_number_integer()) {
				source.parameters[key] = value.get<int>();
			} else if (value.is_number_float()) {
				source.parameters[key] = value.get<float>();
			} else if (value.is_string()) {
				source.parameters[key] = value.get<std::string>();
			}
		}
	}
	
	return source;
}

LocationSource EDLParser::parseLocationSource(const nlohmann::json& j) {
	// Location sources are not supported
	throw InvalidEdlException("Location sources are not supported");
}

EffectSource EDLParser::parseEffectSource(const nlohmann::json& j) {
	EffectSource source;
	
	source.type = getString(j, "source", "type");
	auto [in, out] = getInterval(j, "source");
	source.in = in;
	source.out = out;
	
	// Store effect-specific data
	// Common fields include: value (for brightness/contrast), filters, controlPoints
	if (hasNonNullKey(j, "value")) {
		source.data["value"] = getDouble(j, "source", "value");
	}
	
	if (hasNonNullKey(j, "filters")) {
		// For now, store filters as JSON string
		source.data["filters_json"] = j["filters"].dump();
	}
	
	if (hasNonNullKey(j, "insideMaskFilters")) {
		// For now, store as JSON string
		source.data["insideMaskFilters_json"] = j["insideMaskFilters"].dump();
	}
	
	if (hasNonNullKey(j, "outsideMaskFilters")) {
		// For now, store as JSON string
		source.data["outsideMaskFilters_json"] = j["outsideMaskFilters"].dump();
	}
	
	if (hasNonNullKey(j, "controlPoints")) {
		// For now, store as JSON string
		source.data["controlPoints_json"] = j["controlPoints"].dump();
	}
	
	// Store any other fields for future use
	for (const auto& [key, value] : j.items()) {
		if (key != "in" && key != "out" && key != "type" && 
			key != "value" && key != "filters" && key != "controlPoints" &&
			key != "insideMaskFilters" && key != "outsideMaskFilters") {
			// Store as JSON string for flexibility
			source.data[key + "_json"] = value.dump();
		}
	}
	
	return source;
}

TransformSource EDLParser::parseTransformSource(const nlohmann::json& j) {
	TransformSource source;
	
	auto [in, out] = getInterval(j, "source");
	source.in = in;
	source.out = out;
	
	// Parse control points if present
	if (hasNonNullKey(j, "controlPoints")) {
		auto controlPoints = getArray(j, "source", "controlPoints");
		for (const auto& cp : controlPoints) {
			source.controlPoints.push_back(parseShapeControlPoint(cp));
		}
	}
	
	return source;
}

SubtitleSource EDLParser::parseSubtitleSource(const nlohmann::json& j) {
	SubtitleSource source;
	
	// Text can be null for gaps in multi-source subtitle clips
	if (hasNonNullKey(j, "text")) {
		source.text = getString(j, "source", "text");
	}
	
	auto [in, out] = getInterval(j, "source");
	source.in = in;
	source.out = out;
	
	return source;
}

ShapeControlPoint EDLParser::parseShapeControlPoint(const nlohmann::json& j) {
	ShapeControlPoint cp;
	
	getIfExists(j, "point", cp.point);
	getIfExists(j, "panx", cp.panx);
	getIfExists(j, "pany", cp.pany);
	getIfExists(j, "zoomx", cp.zoomx);
	getIfExists(j, "zoomy", cp.zoomy);
	getIfExists(j, "rotate", cp.rotate);
	getIfExists(j, "shape", cp.shape);
	
	return cp;
}

// ============================================================================
// Other structure parsing
// ============================================================================

Track EDLParser::parseTrack(const nlohmann::json& j) {
	Track track;
	
	static const std::map<std::string, Track::Type> typeMap = {
		{"video", Track::Video},
		{"audio", Track::Audio},
		{"subtitle", Track::Subtitle},
		{"caption", Track::Caption},
		{"burnin", Track::Burnin}
	};
	
	std::string typeStr = getString(j, "track", "type");
	auto it = typeMap.find(typeStr);
	if (it == typeMap.end()) {
		throw InvalidEdlException("Unknown track type: " + typeStr);
	}
	track.type = it->second;
	
	track.number = getPositiveInteger(j, "track", "number");
	
	// Optional fields
	getIfExists(j, "subtype", track.subtype);
	if (hasNonNullKey(j, "subnumber")) {
		track.subnumber = getPositiveInteger(j, "track", "subnumber");
	} else {
		track.subnumber = 1; // Default per reference
	}
	
	// Validate subnumber requires subtype
	if (track.subtype.empty() && track.subnumber != 1) {
		throw InvalidEdlException("Track with subnumber must have subtype");
	}
	
	return track;
}

Motion EDLParser::parseMotion(const nlohmann::json& j) {
	Motion motion;
	
	getIfExists(j, "panX", motion.panX);
	getIfExists(j, "panY", motion.panY);
	getIfExists(j, "zoomX", motion.zoomX);
	getIfExists(j, "zoomY", motion.zoomY);
	getIfExists(j, "rotation", motion.rotation);
	getIfExists(j, "offset", motion.offset);
	getIfExists(j, "duration", motion.duration);
	
	// Check for bezier curves (not supported yet)
	if (hasNonNullKey(j, "bezier")) {
		throw InvalidEdlException("Motion bezier curves are not supported");
	}
	
	return motion;
}

Transition EDLParser::parseTransition(const nlohmann::json& j) {
	Transition transition;
	
	getIfExists(j, "type", transition.type);
	getIfExists(j, "duration", transition.duration);
	
	// Store additional parameters
	for (const auto& [key, value] : j.items()) {
		if (key != "type" && key != "duration" && key != "source" && key != "sources") {
			if (value.is_boolean()) {
				transition.parameters[key] = value.get<bool>();
			} else if (value.is_number_integer()) {
				transition.parameters[key] = value.get<int>();
			} else if (value.is_number_float()) {
				transition.parameters[key] = value.get<double>();
			} else if (value.is_string()) {
				transition.parameters[key] = value.get<std::string>();
			}
		}
	}
	
	return transition;
}

TextFormat EDLParser::parseTextFormat(const nlohmann::json& j) {
	TextFormat format;
	
	getIfExists(j, "font", format.font);
	getIfExists(j, "fontSize", format.fontSize);
	getIfExists(j, "halign", format.halign);
	getIfExists(j, "valign", format.valign);
	getIfExists(j, "textAYUV", format.textAYUV);
	getIfExists(j, "backAYUV", format.backAYUV);
	
	return format;
}

Clip EDLParser::parseClip(const nlohmann::json& j) {
	Clip clip;
	
	// Check for unsupported features first
	checkUnsupportedClipFeatures(j);
	
	// Required fields
	auto [in, out] = getInterval(j, "clip");
	clip.in = in;
	clip.out = out;
	
	clip.track = parseTrack(getObject(j, "clip", "track"));
	
	// Parse source or sources array
	std::string sourceKey = getUniqueNonNullKey(j, "clip", {"source", "sources"});
	if (sourceKey == "source") {
		auto sourceJson = getObject(j, "clip", "source");
		clip.source = parseSource(sourceJson, clip.track);
	} else {
		// sources array
		auto sourcesArray = getArray(j, "clip", "sources");
		if (sourcesArray.empty()) {
			throw InvalidEdlException("Sources array cannot be empty");
		}
		if (sourcesArray.size() > 1) {
			throw InvalidEdlException("Multiple sources in a single clip are not yet supported");
		}
		
		// Parse the single source
		clip.source = parseSource(sourcesArray[0], clip.track);
	}
	
	// Optional fields
	getIfExists(j, "topFade", clip.topFade);
	getIfExists(j, "tailFade", clip.tailFade);
	getIfExists(j, "topFadeYUV", clip.topFadeYUV);
	getIfExists(j, "tailFadeYUV", clip.tailFadeYUV);
	getIfExists(j, "sync", clip.sync);
	
	if (hasNonNullKey(j, "motion")) {
		clip.motion = parseMotion(j["motion"]);
	}
	
	if (hasNonNullKey(j, "transition")) {
		clip.transition = parseTransition(j["transition"]);
	}
	
	if (hasNonNullKey(j, "textFormat")) {
		clip.textFormat = parseTextFormat(j["textFormat"]);
	}
	
	if (hasNonNullKey(j, "channelMap")) {
		auto channelMap = getObject(j, "clip", "channelMap");
		for (const auto& [key, value] : channelMap.items()) {
			int channel = std::stoi(key);
			if (channel < 1 || channel > 128) {
				throw InvalidEdlException("Channel map key must be between 1 and 128: " + key);
			}
			if (!value.is_number()) {
				throw InvalidEdlException("Channel map values must be numbers");
			}
			double level = value.get<double>();
			if (level != 1.0) {
				throw InvalidEdlException("Channel map level must be 1.0 (other values not supported)");
			}
			clip.channelMap[channel] = level;
		}
	}
	
	// Parse effects array if present
	if (hasNonNullKey(j, "effects")) {
		auto effectsArray = getArray(j, "clip", "effects");
		for (const auto& effectJson : effectsArray) {
			if (!effectJson.is_object()) {
				throw InvalidEdlException("Each effect must be an object");
			}
			
			SimpleEffect effect;
			effect.type = getString(effectJson, "effect", "type");
			
			// Get strength, default to 1.0 if not specified
			if (hasNonNullKey(effectJson, "strength")) {
				effect.strength = static_cast<float>(getDouble(effectJson, "effect", "strength"));
			} else {
				effect.strength = 1.0f;
			}
			
			clip.effects.push_back(effect);
		}
	}
	
	return clip;
}

// ============================================================================
// Track management
// ============================================================================

std::string EDLParser::getTrackKey(const Track& track) {
	// Simplified track key generation
	// For our purposes, we'll use simpler keys than the reference
	std::string key;
	
	if (track.type == Track::Video) {
		if (track.subtype == "effects") {
			// Effects tracks will be renamed to fx tracks later
			key = "_effects_" + std::to_string(track.number) + "_" + std::to_string(track.subnumber);
		} else if (track.subtype == "transform" || track.subtype == "colour") {
			key = "video_" + std::to_string(track.number) + "_" + track.subtype;
		} else if (track.subtype.empty()) {
			key = "video_" + std::to_string(track.number);
		} else {
			throw InvalidEdlException("Unknown video track subtype: " + track.subtype);
		}
	} else if (track.type == Track::Audio) {
		if (track.subtype == "level" || track.subtype == "pan") {
			key = "audio_" + std::to_string(track.number) + "_" + track.subtype;
		} else if (track.subtype.empty()) {
			key = "audio_" + std::to_string(track.number);
		} else {
			throw InvalidEdlException("Unknown audio track subtype: " + track.subtype);
		}
	} else if (track.type == Track::Subtitle || track.type == Track::Burnin) {
		std::string typeStr = (track.type == Track::Subtitle) ? "subtitle" : "burnin";
		if (track.subtype == "transform") {
			key = typeStr + "_" + std::to_string(track.number) + "_transform";
		} else if (track.subtype.empty()) {
			key = typeStr + "_" + std::to_string(track.number);
		} else {
			throw InvalidEdlException("Unknown " + typeStr + " track subtype: " + track.subtype);
		}
	} else {
		throw InvalidEdlException("Unsupported track type");
	}
	
	return key;
}

void EDLParser::alignTracksWithNullClips(EDL& edl) {
	// Organize clips by track
	for (const auto& clip : edl.clips) {
		std::string trackKey = getTrackKey(clip.track);
		
		// Check if we need to add null clips for alignment
		if (!edl.tracks.count(trackKey)) {
			edl.tracks[trackKey] = std::vector<Clip>();
		}
		
		auto& track = edl.tracks[trackKey];
		
		// Calculate current track duration
		double trackDuration = 0.0;
		for (const auto& existingClip : track) {
			trackDuration = existingClip.out; // Last clip's out point
		}
		
		// Add null clip if there's a gap
		if (trackDuration < clip.in) {
			Clip nullClip;
			nullClip.in = trackDuration;
			nullClip.out = clip.in;
			nullClip.isNullClip = true;
			nullClip.track = clip.track;
			track.push_back(nullClip);
		} else if (trackDuration > clip.in) {
			throw InvalidEdlException("Track has overlapping clips at time " + 
									 std::to_string(clip.in));
		}
		
		track.push_back(clip);
	}
	
	// Find the overall EDL duration
	double edlDuration = 0.0;
	for (const auto& [trackKey, track] : edl.tracks) {
		if (!track.empty()) {
			double trackDuration = track.back().out;
			if (trackDuration > edlDuration) {
				edlDuration = trackDuration;
			}
		}
	}
	
	// Extend all tracks to match EDL duration with null clips
	for (auto& [trackKey, track] : edl.tracks) {
		if (!track.empty()) {
			double trackDuration = track.back().out;
			if (trackDuration < edlDuration) {
				Clip nullClip;
				nullClip.in = trackDuration;
				nullClip.out = edlDuration;
				nullClip.isNullClip = true;
				if (!track.empty()) {
					nullClip.track = track.back().track;
				}
				track.push_back(nullClip);
			}
		}
	}
	
	// Handle effects tracks - rename them to fx tracks
	std::map<std::string, std::string> effectsToFx;
	int fxTrackNum = 0;
	for (const auto& [trackKey, track] : edl.tracks) {
		if (trackKey.find("_effects_") == 0) {
			std::string fxKey = "fx_" + std::to_string(fxTrackNum++);
			effectsToFx[trackKey] = fxKey;
			
			// Determine which track this effect applies to
			if (!track.empty()) {
				Track parentTrack = track[0].track;
				parentTrack.subtype = ""; // Remove effects subtype
				std::string parentKey = getTrackKey(parentTrack);
				edl.fxAppliesTo[fxKey] = parentKey;
			}
		}
	}
	
	// Rename effects tracks
	for (const auto& [oldKey, newKey] : effectsToFx) {
		edl.tracks[newKey] = std::move(edl.tracks[oldKey]);
		edl.tracks.erase(oldKey);
	}
}

// ============================================================================
// Feature validation
// ============================================================================

void EDLParser::validateUnsupportedFeatures(const nlohmann::json& j) {
	// Document and check for unsupported features at the EDL level
	
	// We support: fps, width, height, clips
	// We don't support: other global settings
	std::set<std::string> supportedKeys = {"fps", "width", "height", "clips"};
	ensureOnlyKeys(j, "EDL", supportedKeys);
}

void EDLParser::checkUnsupportedClipFeatures(const nlohmann::json& clip) {
	// Features we support
	std::set<std::string> supportedKeys = {
		"in", "out", "track", "source", "sources",
		"topFade", "tailFade", "topFadeYUV", "tailFadeYUV",
		"motion", "transition", "sync", "channelMap",
		"textFormat", "effects"
	};
	
	// Features we explicitly don't support yet
	if (hasNonNullKey(clip, "font") || hasNonNullKey(clip, "fonts")) {
		throw InvalidEdlException("Font/fonts in clips are not supported");
	}
	
	// Check for multiple sources
	if (hasNonNullKey(clip, "sources")) {
		auto sources = clip["sources"];
		if (sources.is_array() && sources.size() > 1) {
			throw InvalidEdlException("Multiple sources in a single clip are not yet supported");
		}
	}
	
	// Transitions are complex - only basic support
	if (hasNonNullKey(clip, "transition")) {
		auto transition = clip["transition"];
		if (hasNonNullKey(transition, "source") || hasNonNullKey(transition, "sources")) {
			throw InvalidEdlException("Transition clips with sources are not supported");
		}
	}
}

void EDLParser::checkUnsupportedSourceFeatures(const nlohmann::json& source) {
	// Document features we don't support in sources
	
	// Location sources
	if (hasNonNullKey(source, "location")) {
		throw InvalidEdlException("Location sources are not supported");
	}
	
	// Complex motion with bezier curves
	if (hasNonNullKey(source, "motion") && source["motion"].is_object()) {
		if (hasNonNullKey(source["motion"], "bezier")) {
			throw InvalidEdlException("Motion bezier curves are not supported");
		}
	}
	
	// Some generate types
	if (hasNonNullKey(source, "generate")) {
		auto generate = source["generate"];
		if (generate.is_object() && hasNonNullKey(generate, "type")) {
			std::string type = generate["type"];
			if (type != "black") {
				throw InvalidEdlException("Generate type '" + type + 
										 "' is not yet supported. Only 'black' is currently supported.");
			}
		}
	}
}

} // namespace edl