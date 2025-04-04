// Read an INI file into easy-to-access name/value pairs.

// inih and INIReader are released under the New BSD license:
//
// Copyright (c) 2009, Ben Hoyt
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Ben Hoyt nor the names of its contributors
//       may be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY BEN HOYT ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL BEN HOYT BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Go to the project home page for more info: https://github.com/benhoyt/inih

#ifndef LCF_INIREADER_H
#define LCF_INIREADER_H

#include <unordered_map>
#include <string>
#include <string_view>

namespace lcf {

// Read an INI file into easy-to-access name/value pairs. (Note that I've gone
// for simplicity here rather than speed, but it should be pretty decent.)
class INIReader
{
public:
	// Construct INIReader and parse given filename. See ini.h for more info
	// about the parsing.
	explicit INIReader(std::string filename);

	// Construct INIReader and parse given stream. See ini.h for more info
	// about the parsing.
	// Custom function for liblcf.
	INIReader(std::istream& filestream);

	// Return the result of ini_parse(), i.e., 0 on success, line number of
	// first error on parse error, or -1 on file open error.
	int ParseError() const;

	// Get a string value from INI file, returning default_value if not found.
	std::string_view Get(std::string_view section, std::string_view name,
	                std::string_view default_value) const;

	// Get a string value from INI file, returning default_value if not found,
	// empty, or contains only whitespace.
	std::string_view GetString(std::string_view section, std::string_view name,
	                      std::string_view default_value) const;

	// Get an integer (long) value from INI file, returning default_value if
	// not found or not a valid integer (decimal "1234", "-1234", or hex "0x4d2").
	long GetInteger(std::string_view section, std::string_view name, long default_value) const;

	// Get a real (floating point double) value from INI file, returning
	// default_value if not found or not a valid floating point value
	// according to strtod().
	double GetReal(std::string_view section, std::string_view name, double default_value) const;

	// Get a boolean value from INI file, returning default_value if not found or if
	// not a valid true/false value. Valid true values are "true", "yes", "on", "1",
	// and valid false values are "false", "no", "off", "0" (not case sensitive).
	bool GetBoolean(std::string_view section, std::string_view name, bool default_value) const;

	// Return true if a value exists with the given section and field names.
	bool HasValue(std::string_view section, std::string_view name) const;

private:
	int _error;
	std::unordered_map<std::string, std::string> _values;
	static std::string MakeKey(std::string_view section, std::string_view name);
	static int ValueHandler(void* user, const char* section, const char* name, const char* value);
};

} //namespace lcf

#endif
