// Descompresor zlib/DEFLATE (RFC 1950/1951) para la partida, sin dependencias.
// Huffman canónico como en «puff» de zlib: tablas de cantidad por longitud y símbolos ordenados.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace inflate_detail {

struct Huff {
    short count[16];
    short symbol[320];
};

struct State {
    const uint8_t *in;
    size_t len, pos = 0;
    unsigned bitbuf = 0, bitcnt = 0;
    std::vector<uint8_t> *out;
    bool err = false;

    int bits(int need) {
        long val = bitbuf;
        while (bitcnt < (unsigned)need) {
            if (pos >= len) { err = true; return 0; }
            val |= (long)in[pos++] << bitcnt;
            bitcnt += 8;
        }
        bitbuf = (unsigned)(val >> need);
        bitcnt -= need;
        return (int)(val & ((1L << need) - 1));
    }
    int decode(const Huff &h) {
        int code = 0, first = 0, index = 0;
        for (int l = 1; l < 16; l++) {
            code |= bits(1);
            if (err) return -1;
            int count = h.count[l];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        err = true;
        return -1;
    }
};

inline int construct(Huff &h, const short *length, int n) {
    memset(h.count, 0, sizeof h.count);
    for (int s = 0; s < n; s++) h.count[length[s]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int l = 1; l < 16; l++) {
        left <<= 1;
        left -= h.count[l];
        if (left < 0) return left;
    }
    short offs[16];
    offs[1] = 0;
    for (int l = 1; l < 15; l++) offs[l + 1] = offs[l] + h.count[l];
    for (int s = 0; s < n; s++)
        if (length[s]) h.symbol[offs[length[s]]++] = (short)s;
    return left;
}

inline bool codes(State &s, const Huff &lencode, const Huff &distcode) {
    static const short lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = s.decode(lencode);
        if (sym < 0) return false;
        if (sym < 256) { s.out->push_back((uint8_t)sym); continue; }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) return false;
        int len = lbase[sym] + s.bits(lext[sym]);
        int dsym = s.decode(distcode);
        if (dsym < 0 || dsym >= 30) return false;
        size_t dist = dbase[dsym] + s.bits(dext[dsym]);
        if (s.err || dist > s.out->size()) return false;
        size_t from = s.out->size() - dist;
        for (int k = 0; k < len; k++) s.out->push_back((*s.out)[from + k]);
    }
}

inline bool fixed(State &s) {
    static Huff lencode, distcode;
    static bool built = false;
    if (!built) {
        short lengths[288];
        int sym = 0;
        for (; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        construct(lencode, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        construct(distcode, lengths, 30);
        built = true;
    }
    return codes(s, lencode, distcode);
}

inline bool dynamic(State &s) {
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320];
    int nlen = s.bits(5) + 257, ndist = s.bits(5) + 1, ncode = s.bits(4) + 4;
    if (s.err || nlen > 286 || ndist > 30) return false;
    int index = 0;
    for (; index < ncode; index++) lengths[order[index]] = (short)s.bits(3);
    for (; index < 19; index++) lengths[order[index]] = 0;
    Huff lencode, distcode;
    if (construct(lencode, lengths, 19) != 0) return false;
    index = 0;
    while (index < nlen + ndist) {
        int sym = s.decode(lencode);
        if (sym < 0) return false;
        if (sym < 16) { lengths[index++] = (short)sym; continue; }
        int len = 0, rep;
        if (sym == 16) {
            if (index == 0) return false;
            len = lengths[index - 1];
            rep = 3 + s.bits(2);
        } else if (sym == 17) rep = 3 + s.bits(3);
        else rep = 11 + s.bits(7);
        if (index + rep > nlen + ndist) return false;
        while (rep--) lengths[index++] = (short)len;
    }
    if (lengths[256] == 0) return false;
    int err = construct(lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return false;
    err = construct(distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return false;
    return codes(s, lencode, distcode);
}

} // namespace inflate_detail

// Descomprime DEFLATE sin cabecera (lo que guardan los zip). Devuelve false si los datos no son válidos.
inline bool raw_inflate(const uint8_t *data, size_t len, std::vector<uint8_t> &out) {
    using namespace inflate_detail;
    State s;
    s.in = data;
    s.len = len;
    s.out = &out;
    int last;
    do {
        last = s.bits(1);
        int type = s.bits(2);
        if (s.err) return false;
        if (type == 0) {
            s.bitbuf = 0;
            s.bitcnt = 0;
            if (s.pos + 4 > s.len) return false;
            unsigned n = s.in[s.pos] | s.in[s.pos + 1] << 8;
            s.pos += 4;
            if (s.pos + n > s.len) return false;
            out.insert(out.end(), s.in + s.pos, s.in + s.pos + n);
            s.pos += n;
        } else if (type == 1) {
            if (!fixed(s)) return false;
        } else if (type == 2) {
            if (!dynamic(s)) return false;
        } else return false;
    } while (!last);
    return true;
}

// Descomprime un flujo zlib (cabecera de 2 bytes + DEFLATE). Devuelve false si los datos no son válidos.
inline bool zlib_inflate(const uint8_t *data, size_t len, std::vector<uint8_t> &out) {
    if (len < 2 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0) return false;
    return raw_inflate(data + 2, len - 2, out);
}
