#pragma once

#include <string>
#include <vector>

namespace edl {

struct Motion {
	float panX = 0.0f;     // -1 to 1
	float panY = 0.0f;     // -1 to 1
	float zoomX = 1.0f;    // zoom factor
	float zoomY = 1.0f;    // zoom factor
	float rotation = 0.0f; // degrees
};

struct Transition {
	std::string type;      // "dissolve", "wipe", etc.
	double duration = 0.0;
};

struct Source {
	std::string mediaId;
	std::string trackId;   // "V1", "A1", etc.
	double in = 0.0;       // Source timecode in seconds
	double out = 0.0;      // Source timecode in seconds
	
	// Optional
	int width = 0;
	int height = 0;
	int fps = 0;
	float rotation = 0.0f;
	bool flip = false;
};

struct Track {
	enum Type { Video, Audio, Subtitle, Caption };
	Type type = Video;
	int number = 1;
	std::string subtype;   // "transform", "effects", etc.
	int subnumber = 0;
};

struct Clip {
	double in = 0.0;       // Timeline position in seconds
	double out = 0.0;      // Timeline position in seconds
	Track track;
	Source source;
	
	// Optional
	float topFade = 0.0f;
	float tailFade = 0.0f;
	Motion motion;
	Transition transition;
};

struct EDL {
	int fps = 30;
	int width = 1920;
	int height = 1080;
	std::vector<Clip> clips;
};

} // namespace edl