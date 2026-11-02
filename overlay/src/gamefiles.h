// Arte del juego leído de su propia carpeta (assets/ui/*/*.zip): iconos, imágenes de coleccionables y el cursor.
// Misma lógica que desktop/gameicons.py: cada .oct lleva una textura DDS BGRA de 32 bits sin comprimir.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
};

// Un zip del juego en memoria; read() descomprime un archivo de dentro por su nombre (sin distinguir mayúsculas)
class GameZip {
public:
    bool open(const std::wstring &path);
    bool read(const std::string &name, std::vector<uint8_t> &out) const;

private:
    std::vector<uint8_t> data;
    struct Entry { size_t local; uint32_t csize, usize; uint16_t method; };
    std::map<std::string, Entry> entries;
};

// Textura de un .oct como RGBA, del mipmap más grande que quepa en maxSize
bool octImage(const std::vector<uint8_t> &oct, int maxSize, Image &img);
