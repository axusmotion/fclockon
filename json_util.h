// json_util.h — Minimal self-contained JSON reader/writer for flat config files.
// Supports flat JSON objects with string, int, and bool values.
// No external dependencies.
#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>

class JsonConfig {
public:
    void setString(const std::string& key, const std::string& value) {
        m_data[key] = "\"" + escapeJson(value) + "\"";
    }

    void setInt(const std::string& key, int value) {
        m_data[key] = std::to_string(value);
    }

    void setBool(const std::string& key, bool value) {
        m_data[key] = value ? "true" : "false";
    }

    std::string getString(const std::string& key, const std::string& defaultVal = "") const {
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        const std::string& v = it->second;
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
            return unescapeJson(v.substr(1, v.size() - 2));
        }
        return v;
    }

    int getInt(const std::string& key, int defaultVal = 0) const {
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        const std::string& v = it->second;
        // Strip quotes if present
        std::string clean = v;
        if (clean.size() >= 2 && clean.front() == '"' && clean.back() == '"')
            clean = clean.substr(1, clean.size() - 2);
        char* end = nullptr;
        long result = std::strtol(clean.c_str(), &end, 10);
        if (end == clean.c_str()) return defaultVal;
        return static_cast<int>(result);
    }

    bool getBool(const std::string& key, bool defaultVal = false) const {
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        const std::string& v = it->second;
        if (v == "true") return true;
        if (v == "false") return false;
        // Handle quoted booleans
        if (v == "\"true\"") return true;
        if (v == "\"false\"") return false;
        return defaultVal;
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();
        file.close();

        return parse(content);
    }

    bool saveToFile(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << "{\n";
        size_t count = 0;
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            file << "  \"" << escapeJson(it->first) << "\": " << it->second;
            if (++count < m_data.size()) file << ",";
            file << "\n";
        }
        file << "}\n";

        file.close();
        return true;
    }

private:
    std::map<std::string, std::string> m_data;

    bool parse(const std::string& json) {
        m_data.clear();
        size_t pos = 0;

        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != '{') return false;
        pos++; // skip '{'

        while (pos < json.size()) {
            skipWhitespace(json, pos);
            if (pos >= json.size()) return false;
            if (json[pos] == '}') return true;

            // Parse key
            std::string key;
            if (!parseString(json, pos, key)) return false;

            skipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') return false;
            pos++; // skip ':'

            skipWhitespace(json, pos);
            if (pos >= json.size()) return false;

            // Parse value (as raw string representation)
            std::string value;
            if (json[pos] == '"') {
                // String value
                std::string strVal;
                if (!parseString(json, pos, strVal)) return false;
                value = "\"" + escapeJson(strVal) + "\"";
            } else if (json[pos] == 't' || json[pos] == 'f') {
                // Boolean
                if (json.substr(pos, 4) == "true") {
                    value = "true";
                    pos += 4;
                } else if (json.substr(pos, 5) == "false") {
                    value = "false";
                    pos += 5;
                } else {
                    return false;
                }
            } else if (json[pos] == '-' || (json[pos] >= '0' && json[pos] <= '9')) {
                // Number
                size_t start = pos;
                if (json[pos] == '-') pos++;
                while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
                if (pos < json.size() && json[pos] == '.') {
                    pos++;
                    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
                }
                value = json.substr(start, pos - start);
            } else if (json[pos] == 'n') {
                // null
                if (json.substr(pos, 4) == "null") {
                    value = "null";
                    pos += 4;
                } else {
                    return false;
                }
            } else {
                return false;
            }

            m_data[key] = value;

            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                pos++; // skip comma
            }
        }
        return false;
    }

    static void skipWhitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
               s[pos] == '\n' || s[pos] == '\r')) {
            pos++;
        }
    }

    static bool parseString(const std::string& s, size_t& pos, std::string& out) {
        if (pos >= s.size() || s[pos] != '"') return false;
        pos++; // skip opening quote
        out.clear();
        while (pos < s.size()) {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                switch (s[pos]) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    default: out += s[pos]; break;
                }
                pos++;
            } else if (s[pos] == '"') {
                pos++; // skip closing quote
                return true;
            } else {
                out += s[pos];
                pos++;
            }
        }
        return false; // unterminated string
    }

    static std::string escapeJson(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\t': result += "\\t"; break;
                case '\r': result += "\\r"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    static std::string unescapeJson(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                i++;
                switch (s[i]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    default: result += s[i]; break;
                }
            } else {
                result += s[i];
            }
        }
        return result;
    }
};
