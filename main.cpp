#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <regex>
#include <charconv>
#include "print.h"

struct JSONObject;

using JSONDict = std::unordered_map<std::string, JSONObject>;
using JSONList = std::vector<JSONObject>;

struct JSONObject
{
    std::variant<std::nullptr_t // null
                 ,
                 bool // true
                 ,
                 int // 42
                 ,
                 double // 3.14
                 ,
                 std::string // "hello"
                 ,
                 JSONList // [42, "hello"]
                 ,
                 JSONDict // {"hello": 985, "world": 211}
                 >
        inner;

    void do_print() const
    {
        printnl(inner);
    }

    template <class T>
    bool is() const
    {
        return std::holds_alternative<T>(inner);
    }

    template <class T>
    T const &get() const
    {
        return std::get<T>(inner);
    }

    template <class T>
    T &get()
    {
        return std::get<T>(inner);
    }
};

template <class T>
std::optional<T> try_parse_num(std::string_view str)
{
    T value;
    auto res = std::from_chars(str.data(), str.data() + str.size(), value);
    if (res.ec == std::errc() && res.ptr == str.data() + str.size())
    {
        return value;
    }
    return std::nullopt;
}

char unescaped_char(char c)
{
    switch (c)
    {
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'b':
        return '\b';
    case 'a':
        return '\a';
    default:
        return c;
    }
}

std::pair<JSONObject, size_t> parse(std::string_view json)
{
    if (json.empty())
    {
        return {JSONObject{std::nullptr_t{}}, 0};
    }
    else if (size_t off = json.find_first_not_of(" \n\r\t\v\f\0"); off != 0 && off != json.npos)
    {
        auto [obj, eaten] = parse(json.substr(off));
        return {std::move(obj), eaten + off};
    }
    // 如果是bool
    else if (json[0] == 't' || json[0] == 'f')
    {
        if (json[0] == 't')
        {
            return {JSONObject{true}, 4};
        }
        if (json[0] == 'f')
        {
            return {JSONObject{false}, 5};
        }
    }
    // 如果是int，double
    else if ('0' <= json[0] && json[0] <= '9' || json[0] == '+' || json[0] == '-')
    {
        std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
        std::cmatch match;
        if (std::regex_search(json.data(), json.data() + json.size(), match, num_re))
        {
            std::string str = match.str();
            if (auto num = try_parse_num<int>(str))
            {
                return {JSONObject{*num}, str.size()};
            }
            if (auto num = try_parse_num<double>(str))
            {
                return {JSONObject{*num}, str.size()};
            }
        }
    }
    // 如果是字符串
    else if (json[0] == '"')
    {
        std::string str;
        enum
        {
            Raw,
            Escaped,
        } phase = Raw;
        size_t i;
        for (i = 1; i < json.size(); i++)
        {
            char ch = json[i];
            if (phase == Raw)
            {
                if (ch == '\\')
                {
                    phase = Escaped;
                }
                else if (ch == '"')
                {
                    i += 1;
                    break;
                }
                else
                {
                    str += ch;
                }
            }
            else if (phase == Escaped)
            {
                str += unescaped_char(ch);
                phase = Raw;
            }
        }
        return {JSONObject{std::move(str)}, i};
    }
    // 如果是列表
    else if (json[0] == '[')
    {
        std::vector<JSONObject> res;
        size_t i;
        for (i = 1; i < json.size();)
        {
            if (json[i] == ']')
            {
                i += 1;
                break;
            }
            auto [obj, eaten] = parse(json.substr(i));
            if (eaten == 0)
            {
                i = 0;
                break;
            }
            res.push_back(std::move(obj));
            i += eaten;
            if (json[i] == ',')
            {
                i += 1;
            }
        }
        return {JSONObject{std::move(res)}, i}; 
    }
    // 如果是字典
    else if (json[0] == '{')
    {
        std::unordered_map<std::string, JSONObject> res;
        size_t i;
        for (i = 1; i < json.size();)
        {
            if (json[i] == '}')
            {
                i += 1;
                break;
            }
            auto [keyobj, keyeaten] = parse(json.substr(i));
            if (keyeaten == 0)
            {
                i = 0;
                break;
            }
            i += keyeaten;
            if (!std::holds_alternative<std::string>(keyobj.inner))
            {
                i = 0;
                break;
            }
            if (json[i] == ':')
            {
                i += 1;
            }
            std::string key = std::move(std::get<std::string>(keyobj.inner));
            auto [valobj, valeaten] = parse(json.substr(i));
            if (valeaten == 0)
            {
                i = 0;
                break;
            }
            i += valeaten;
            res.try_emplace(std::move(key), std::move(valobj));
            if (json[i] == ',')
            {
                i += 1;
            }
        }
        return {JSONObject{std::move(res)}, i};
    }
    return {JSONObject{std::nullptr_t{}}, 0};
}

//模版推导指导
template <class... Fs>
struct overloaded : Fs...
{
    using Fs::operator()...;
};

template <class... Fs>
overloaded(Fs...) -> overloaded<Fs...>;

int main()
{
    std::string_view str = R"JSON({"math": true, "english": "good\n"})JSON";
    auto [obj, eaten] = parse(str);

    print(obj);

    std::visit(
        overloaded{
            [&](bool val)
            {
                print("bool is:", val);
            },
            [&](int val)
            {
                print("int is:", val);
            },
            [&](double val)
            {
                print("double is:", val);
            },
            [&](std::string val)
            {
                print("string is:", val);
            },
            [&](JSONList list)
            {
                for (auto i : list)
                {
                    std::visit(
                        overloaded{
                            [&](bool val)
                            {
                                print("bool is:", val);
                            },
                            [&](int val)
                            {
                                print("int value is:", val);
                            },
                            [&](double val)
                            {
                                print("double value is:", val);
                            },
                            [&](std::string val)
                            {
                                print("string value is:", val);
                            },
                            [&](auto val)
                            {
                                print("unknown value type, value is:", val);
                            }},
                        i.inner);
                }
            },
            [&](JSONDict dict)
            {
                for (auto iter = dict.begin(); iter != dict.end(); ++iter)
                {
                    std::cout << "key is: " << iter->first << ", ";
                    std::visit(
                        overloaded{
                            [&](bool val)
                            {
                                print("bool is:", val);
                            },
                            [&](int val)
                            {
                                print("int value is:", val);
                            },
                            [&](double val)
                            {
                                print("double value is:", val);
                            },
                            [&](std::string val)
                            {
                                print("string value is:", val);
                            },
                            [&](auto val)
                            {
                                print("unknown value type, value is:", val);
                            }},
                        iter->second.inner);
                }
            },
            [&](auto val)
            {
                print("unknown object is:", val);
            }},
        obj.inner);
    return 0;
}
