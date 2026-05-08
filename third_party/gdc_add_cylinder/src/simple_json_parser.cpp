#include "../include/simple_json_parser.h"
#include <fstream>
#include <cctype>
#include <cmath>

void SimpleJsonParser::skipWhitespace(std::istream& is) {
    while (is.good() && std::isspace(is.peek())) {
        is.get();
    }
}

bool SimpleJsonParser::parseString(std::istream& is, std::string& str) {
    str.clear();
    if (is.get() != '"') return false;
    
    while (is.good()) {
        char c = is.get();
        if (c == '"') return true;
        if (c == '\\') {
            char next = is.get();
            switch (next) {
                case 'n': str += '\n'; break;
                case 't': str += '\t'; break;
                case 'r': str += '\r'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                default: str += next; break;
            }
        } else {
            str += c;
        }
    }
    return false;
}

bool SimpleJsonParser::parseNumber(std::istream& is, double& num) {
    std::string str;
    bool has_dot = false;
    bool has_exp = false;
    
    if (is.peek() == '-') {
        str += is.get();
    }
    
    while (is.good()) {
        char c = is.peek();
        if (std::isdigit(c)) {
            str += is.get();
        } else if (c == '.' && !has_dot) {
            str += is.get();
            has_dot = true;
        } else if ((c == 'e' || c == 'E') && !has_exp) {
            str += is.get();
            has_exp = true;
            if (is.peek() == '+' || is.peek() == '-') {
                str += is.get();
            }
        } else {
            break;
        }
    }
    
    if (str.empty()) return false;
    num = std::stod(str);
    return true;
}

bool SimpleJsonParser::parseArray(std::istream& is, std::vector<JsonValue>& arr) {
    arr.clear();
    if (is.get() != '[') return false;
    
    skipWhitespace(is);
    if (is.peek() == ']') {
        is.get();
        return true;
    }
    
    while (is.good()) {
        JsonValue value;
        if (!parseValue(is, value)) return false;
        arr.push_back(value);
        
        skipWhitespace(is);
        char c = is.peek();
        if (c == ']') {
            is.get();
            return true;
        }
        if (c != ',') return false;
        is.get();
        skipWhitespace(is);
    }
    return false;
}

bool SimpleJsonParser::parseObject(std::istream& is, std::map<std::string, JsonValue>& obj) {
    obj.clear();
    if (is.get() != '{') return false;
    
    skipWhitespace(is);
    if (is.peek() == '}') {
        is.get();
        return true;
    }
    
    while (is.good()) {
        // 解析键
        std::string key;
        if (!parseString(is, key)) return false;
        
        skipWhitespace(is);
        if (is.get() != ':') return false;
        skipWhitespace(is);
        
        // 解析值
        JsonValue value;
        if (!parseValue(is, value)) return false;
        obj[key] = value;
        
        skipWhitespace(is);
        char c = is.peek();
        if (c == '}') {
            is.get();
            return true;
        }
        if (c != ',') return false;
        is.get();
        skipWhitespace(is);
    }
    return false;
}

bool SimpleJsonParser::parseValue(std::istream& is, JsonValue& value) {
    skipWhitespace(is);
    
    char c = is.peek();
    if (c == '"') {
        value.type = JsonValue::STRING;
        return parseString(is, value.string_val);
    } else if (c == '{') {
        value.type = JsonValue::OBJECT;
        return parseObject(is, value.object_val);
    } else if (c == '[') {
        value.type = JsonValue::ARRAY;
        return parseArray(is, value.array_val);
    } else if (c == 'n') {
        // null
        std::string null_str;
        for (int i = 0; i < 4 && is.good(); ++i) null_str += is.get();
        if (null_str == "null") {
            value.type = JsonValue::NULL_VALUE;
            return true;
        }
        return false;
    } else if (c == 't' || c == 'f') {
        // true/false
        std::string bool_str;
        if (c == 't') {
            for (int i = 0; i < 4 && is.good(); ++i) bool_str += is.get();
            if (bool_str == "true") {
                value.type = JsonValue::BOOL;
                value.bool_val = true;
                return true;
            }
        } else {
            for (int i = 0; i < 5 && is.good(); ++i) bool_str += is.get();
            if (bool_str == "false") {
                value.type = JsonValue::BOOL;
                value.bool_val = false;
                return true;
            }
        }
        return false;
    } else if (std::isdigit(c) || c == '-') {
        value.type = JsonValue::NUMBER;
        return parseNumber(is, value.number_val);
    }
    
    return false;
}

bool SimpleJsonParser::parseFile(const std::string& filepath, std::map<std::string, JsonValue>& root) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    JsonValue root_value;
    if (!parseValue(file, root_value) || root_value.type != JsonValue::OBJECT) {
        return false;
    }
    
    root = root_value.object_val;
    return true;
}

// JsonValue 方法实现
SimpleJsonParser::JsonValue& SimpleJsonParser::JsonValue::operator[](const std::string& key) {
    if (type != OBJECT) {
        type = OBJECT;
    }
    return object_val[key];
}

SimpleJsonParser::JsonValue& SimpleJsonParser::JsonValue::operator[](size_t index) {
    if (type != ARRAY) {
        type = ARRAY;
        array_val.clear();
    }
    if (index >= array_val.size()) {
        array_val.resize(index + 1);
    }
    return array_val[index];
}

double SimpleJsonParser::JsonValue::getDouble() const {
    if (type == NUMBER) return number_val;
    if (type == STRING) return std::stod(string_val);
    return 0.0;
}

int SimpleJsonParser::JsonValue::getInt() const {
    return static_cast<int>(getDouble());
}

std::string SimpleJsonParser::JsonValue::getString() const {
    if (type == STRING) return string_val;
    if (type == NUMBER) return std::to_string(number_val);
    return "";
}

bool SimpleJsonParser::JsonValue::has(const std::string& key) const {
    return type == OBJECT && object_val.find(key) != object_val.end();
}

