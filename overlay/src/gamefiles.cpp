#include "gamefiles.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "inflate.h"

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static std::string lower(std::string s) {
    for (auto &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Directorio central del final del archivo (los zip del juego lo llevan también al principio; da igual cuál)
bool GameZip::open(const std::wstring &path) {
    data.clear();
    entries.clear();
    FILE *f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(n > 0 ? n : 0);
    bool ok = n > 22 && fread(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (!ok) return false;
    size_t eocd = std::string::npos;
    for (size_t i = data.size() - 22; i + 1 > 0 && data.size() - i < 66000; i--)
        if (u32(&data[i]) == 0x06054b50) { eocd = i; break; }
    if (eocd == std::string::npos) return false;
    size_t count = u16(&data[eocd + 10]), p = u32(&data[eocd + 16]);
    for (size_t k = 0; k < count && p + 46 <= data.size() && u32(&data[p]) == 0x02014b50; k++) {
        uint16_t nl = u16(&data[p + 28]), xl = u16(&data[p + 30]), cl = u16(&data[p + 32]);
        if (p + 46 + nl > data.size()) break;
        std::string name((const char *)&data[p + 46], nl);
        size_t slash = name.find_last_of('/');
        entries[lower(slash == std::string::npos ? name : name.substr(slash + 1))] = {u32(&data[p + 42]), u32(&data[p + 20]), u32(&data[p + 24]), u16(&data[p + 10])};
        p += 46 + nl + xl + cl;
    }
    return !entries.empty();
}

bool GameZip::read(const std::string &name, std::vector<uint8_t> &out) const {
    auto it = entries.find(lower(name));
    if (it == entries.end()) return false;
    const Entry &e = it->second;
    if (e.local + 30 > data.size() || u32(&data[e.local]) != 0x04034b50) return false;
    size_t start = e.local + 30 + u16(&data[e.local + 26]) + u16(&data[e.local + 28]);
    if (start + e.csize > data.size()) return false;
    out.clear();
    if (e.method == 0) {
        out.assign(data.begin() + start, data.begin() + start + e.csize);
        return true;
    }
    out.reserve(e.usize);
    return e.method == 8 && raw_inflate(&data[start], e.csize, out) && out.size() == e.usize;
}

bool octImage(const std::vector<uint8_t> &oct, int maxSize, Image &img) {
    static const uint8_t magic[8] = {'D', 'D', 'S', ' ', 0x7C, 0, 0, 0};
    size_t i = 0;
    for (; i + 128 <= oct.size(); i++)
        if (!memcmp(&oct[i], magic, 8)) break;
    if (i + 128 > oct.size()) return false;
    const uint8_t *h = &oct[i];
    uint32_t height = u32(h + 12), width = u32(h + 16), mips = u32(h + 28);
    uint32_t pfFlags = u32(h + 80), bits = u32(h + 88);
    // Solo BGRA de 32 bits sin comprimir (todo lo de assets/ui que usamos)
    if (!(pfFlags & 0x40) || bits != 32 || u32(h + 92) != 0xff0000 || u32(h + 96) != 0xff00 || u32(h + 100) != 0xff) return false;
    size_t off = i + 128;
    if (!mips) mips = 1;
    while ((int)std::max(width, height) > maxSize && mips > 1) {
        off += (size_t)width * height * 4;
        width = std::max(width / 2, 1u);
        height = std::max(height / 2, 1u);
        mips--;
    }
    size_t n = (size_t)width * height * 4;
    if (off + n > oct.size()) return false;
    img.w = (int)width;
    img.h = (int)height;
    img.rgba.resize(n);
    const uint8_t *px = &oct[off];
    for (size_t k = 0; k < n; k += 4) {
        img.rgba[k] = px[k + 2];
        img.rgba[k + 1] = px[k + 1];
        img.rgba[k + 2] = px[k];
        img.rgba[k + 3] = px[k + 3];
    }
    return true;
}
