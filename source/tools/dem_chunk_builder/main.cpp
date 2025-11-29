// tools/dem_chunk_builders/main.cpp: main for the chunk preprocessor,
//     turning NASA DEM (.hgt) files into chunk-ready heightmaps

#include <iostream>
#include <fstream>

#include "engine/world/chunk.hpp"

int main(int argc, char* argv[]) {
    // 3-arcsecond DEM is 1201 x 1201 ints
    const std::size_t fileBlockSize = 1201;
    constexpr std::size_t chunkSize = engine::world::CHUNK_RESOLUTION;

    const char * inFilename = "assets/N40W106.hgt";
    std::ifstream fin(inFilename, std::ios_base::binary);

    const char * outFilename = "assets/N40W106.chunk";
    std::ofstream fout(outFilename, std::ios_base::binary);

    if(!fin.good()) {
        std::cout << "cannot read asset file: '" << inFilename << "'\n";
        return -1;
    }

    if(!fout.good()) {
        std::cout << "cannot write chunked file: '" << outFilename << "'\n";
        return -1;
    }

    const std::size_t numChunksWide = std::ceill(
        static_cast<float>(fileBlockSize) / static_cast<float>(chunkSize)
    );

    std::cout << "I will read " << numChunksWide << " in both directions\n";

    // file offset: start at first cell
    std::size_t offset = 0;
    fin.seekg(offset);

    // fill and write each chunk's heightmap data (cx,cy)
    uint16_t lastHeight = 0;
    for(std::size_t cy = 0; cy < chunkSize; ++cy) {
        for(std::size_t cx = 0; cx < chunkSize; ++cx) {

            // read chunk from file
            // chunk local coords
            std::array<std::int16_t, chunkSize * chunkSize> chunk;
            for(std::size_t ly = 0; ly < chunkSize; ++ly) {
                for(std::size_t lx = 0; lx < chunkSize; ++lx) {
                    // compute global gx in chunkspace
                    std::size_t gx = cx * chunkSize + lx;
                    std::size_t gy = cy * chunkSize + ly;

                    // compute chunk heightmap index to write
                    std::size_t writeIndex = ly * chunkSize + lx;

                    // are we out of bounds?
                    if(gx >= fileBlockSize || gy >= fileBlockSize) {
                        // use last sample
                        chunk[writeIndex] = lastHeight;
                        continue;
                    }

                    // where are we in the actual file
                    offset = gy * fileBlockSize + gx;
                    // todo: could be doing bulk reads per chunk
                    fin.seekg(2*offset, std::ios::beg);

                    unsigned char buffer[2]{};
                    fin.read(reinterpret_cast<char*>(buffer), 2);
                    uint16_t hiByte = static_cast<uint16_t>(buffer[0]);
                    uint16_t loByte = static_cast<uint16_t>(buffer[1]);

                    uint16_t bytes = (hiByte << 8) | loByte;
                    int16_t height = static_cast<int16_t>(bytes);

                    // store last read height for (x,y) out of boundaries
                    chunk[writeIndex] = height;
                    lastHeight = height;
                }
            }

            // scream
            std::cout << "I read a chunk of genuine tax-payer-funded space program data\n";

            // write chunked mesh to file as raw int16_t
            fout.write(
                reinterpret_cast<const char*>(chunk.data()),
                chunk.size() * sizeof(int16_t)
            );
        }
    }
}
