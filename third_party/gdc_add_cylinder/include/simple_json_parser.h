#ifndef SIMPLE_JSON_PARSER_H
#define SIMPLE_JSON_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <sstream>

// 简单的JSON解析器，专门用于解析相机参数文件
class SimpleJsonParser {
public:
    // JSON值类型
    struct JsonValue {
        enum Type { NULL_VALUE, BOOL, NUMBER, STRING, ARRAY, OBJECT };
        Type type = NULL_VALUE;
        
        bool bool_val = false;
        double number_val = 0.0;
        std::string string_val;
        std::vector<JsonValue> array_val;
        std::map<std::string, JsonValue> object_val;
        
        // 访问方法
        JsonValue& operator[](const std::string& key);
        JsonValue& operator[](size_t index);
        double getDouble() const;
        int getInt() const;
        std::string getString() const;
        bool has(const std::string& key) const;
    };
    
    // 解析JSON文件
    static bool parseFile(const std::string& filepath, std::map<std::string, JsonValue>& root);
    
private:
    static bool parseValue(std::istream& is, JsonValue& value);
    static bool parseObject(std::istream& is, std::map<std::string, JsonValue>& obj);
    static bool parseArray(std::istream& is, std::vector<JsonValue>& arr);
    static bool parseString(std::istream& is, std::string& str);
    static bool parseNumber(std::istream& is, double& num);
    static void skipWhitespace(std::istream& is);
};

#endif // SIMPLE_JSON_PARSER_H

