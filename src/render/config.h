/*
 * Copyright 2012, 2013 Moritz Hilscher
 *
 * This file is part of mapcrafter.
 *
 * mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "util.h"

#include <string>
#include <vector>
#include <map>
#include <set>

namespace mapcrafter {
namespace render {

class ConfigSection {
private:
	std::string name;
	std::vector<std::pair<std::string, std::string> > entries;

	int getEntryIndex(const std::string& key) const;
public:
	ConfigSection(const std::string& name = "");
	~ConfigSection();

	const std::string& getName() const;

	bool has(const std::string& key) const;

	void set(const std::string& key, const std::string& value);
	std::string get(const std::string& key) const;
};

/**
 * A simple INI-like config file parser.
 */
class ConfigFile {
private:
	ConfigSection root;
	std::vector<ConfigSection> sections;
	std::vector<std::string> section_names;

	int getSectionIndex(const std::string& section) const;

	bool load(std::istream& stream);
public:
	ConfigFile();
	~ConfigFile();

	bool loadFile(const std::string& filename);

	const std::vector<std::string>& getSections() const;

	bool has(const std::string& section, const std::string& key) const;
	std::string get(const std::string& section, const std::string& key) const;

	template<typename T>
	T get(const std::string& section, const std::string& key) const {
		return as<T>(get(section, key));
	}
};

struct RenderWorldConfig {
	std::string name_short, name_long;

	std::string input_dir;
	std::string templates_dir, images_dir;

	std::set<int> rotation;
	int texture_size;

	RenderWorldConfig();

	void readFromConfig(const ConfigFile& config, const std::string& section);

	void print(std::ostream& stream) const;
};

class RenderConfigParser {
private:
	ConfigFile config;
	RenderWorldConfig default_world;
	std::vector<RenderWorldConfig> worlds;

public:
	RenderConfigParser();
	~RenderConfigParser();

	bool loadFile(const std::string& filename);

	const std::vector<RenderWorldConfig>& getWorlds();
};

}
}

#endif /* CONFIG_H_ */
