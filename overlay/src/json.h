// Lector de JSON mínimo para content.json y savemap.json (solo lectura; objetos con el orden del archivo).
#pragma once
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct Json {
    enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
    bool b = false;
    double n = 0;
    std::string s;                                       // UTF-8
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    const Json &operator[](const char *key) const {
        for (auto &kv : obj)
            if (kv.first == key) return kv.second;
        return none();
    }
    const Json &operator[](const std::string &key) const { return (*this)[key.c_str()]; }
    const Json &operator[](size_t i) const { return i < arr.size() ? arr[i] : none(); }
    bool has(const char *key) const {
        for (auto &kv : obj)
            if (kv.first == key) return true;
        return false;
    }
    bool isNull() const { return kind == Null; }
    size_t size() const { return kind == Arr ? arr.size() : obj.size(); }
    const std::string &str() const { return s; }
    int num() const { return (int)n; }
    static const Json &none() {
        static Json j;
        return j;
    }

    static bool parse(const std::string &text, Json &out) {
        out = Json();
        size_t i = 0;
        if (!value(text, i, out)) return false;
        ws(text, i);
        return i == text.size();
    }

private:
    static void ws(const std::string &t, size_t &i) {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++;
    }
    static void utf8(std::string &o, unsigned c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | c >> 6); o += (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o += (char)(0xE0 | c >> 12); o += (char)(0x80 | (c >> 6 & 63)); o += (char)(0x80 | (c & 63)); }
        else { o += (char)(0xF0 | c >> 18); o += (char)(0x80 | (c >> 12 & 63)); o += (char)(0x80 | (c >> 6 & 63)); o += (char)(0x80 | (c & 63)); }
    }
    static bool hex4(const std::string &t, size_t i, unsigned &c) {
        if (i + 4 > t.size()) return false;
        c = 0;
        for (size_t k = i; k < i + 4; k++) {
            char h = t[k];
            c = c * 16 + (h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : 99);
            if (c > 0xFFFF) return false;
        }
        return true;
    }
    static bool string(const std::string &t, size_t &i, std::string &o) {
        if (t[i] != '"') return false;
        i++;
        while (i < t.size() && t[i] != '"') {
            char c = t[i++];
            if (c != '\\') { o += c; continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
            case 'n': o += '\n'; break;
            case 't': o += '\t'; break;
            case 'r': o += '\r'; break;
            case 'b': o += '\b'; break;
            case 'f': o += '\f'; break;
            case 'u': {
                unsigned c1;
                if (!hex4(t, i, c1)) return false;
                i += 4;
                if (c1 >= 0xD800 && c1 < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                    unsigned c2;
                    if (hex4(t, i + 2, c2) && c2 >= 0xDC00 && c2 < 0xE000) { c1 = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00); i += 6; }
                }
                utf8(o, c1);
                break;
            }
            default: o += e;
            }
        }
        if (i >= t.size()) return false;
        i++;
        return true;
    }
    static bool value(const std::string &t, size_t &i, Json &o) {
        ws(t, i);
        if (i >= t.size()) return false;
        char c = t[i];
        if (c == '{') {
            o.kind = Obj;
            i++;
            ws(t, i);
            if (i < t.size() && t[i] == '}') { i++; return true; }
            for (;;) {
                ws(t, i);
                std::string k;
                if (i >= t.size() || !string(t, i, k)) return false;
                ws(t, i);
                if (i >= t.size() || t[i++] != ':') return false;
                o.obj.emplace_back(std::move(k), Json());
                if (!value(t, i, o.obj.back().second)) return false;
                ws(t, i);
                if (i >= t.size()) return false;
                if (t[i] == ',') { i++; continue; }
                if (t[i] == '}') { i++; return true; }
                return false;
            }
        }
        if (c == '[') {
            o.kind = Arr;
            i++;
            ws(t, i);
            if (i < t.size() && t[i] == ']') { i++; return true; }
            for (;;) {
                o.arr.emplace_back();
                if (!value(t, i, o.arr.back())) return false;
                ws(t, i);
                if (i >= t.size()) return false;
                if (t[i] == ',') { i++; continue; }
                if (t[i] == ']') { i++; return true; }
                return false;
            }
        }
        if (c == '"') { o.kind = Str; return string(t, i, o.s); }
        if (t.compare(i, 4, "true") == 0) { o.kind = Bool; o.b = true; i += 4; return true; }
        if (t.compare(i, 5, "false") == 0) { o.kind = Bool; i += 5; return true; }
        if (t.compare(i, 4, "null") == 0) { o.kind = Null; i += 4; return true; }
        size_t start = i;
        while (i < t.size() && (strchr("+-.eE", t[i]) || (t[i] >= '0' && t[i] <= '9'))) i++;
        if (i == start) return false;
        o.kind = Num;
        o.n = strtod(t.substr(start, i - start).c_str(), nullptr);
        return true;
    }
};
