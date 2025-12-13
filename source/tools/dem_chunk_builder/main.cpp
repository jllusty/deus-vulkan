// tools/dem_chunk_builders/main.cpp: main for the chunk preprocessor,
//     turning NASA DEM (.hgt) files into chunk-ready heightmaps (.chunk)
//
// Chunk Binary File Format (.chunk)
// [HEADER] - uint64_t of number of chunks
// [TOC RECORDS] - a ChunkTOC for each Chunk, see engine/world/chunk_data.hpp
// [CHUNK] - Chunk Heightmap

#include <iostream>
#include <fstream>

#include "engine/world/chunk_data.hpp"

int main(int argc, char* argv[]) {
    using namespace engine::world;
    // todo: program arguments for DEM parameters
    // 3-arcsecond DEM is 1201 x 1201 ints
    //const std::size_t fileBlockSize = 3601;
    // 1-arcsecond DEM is 3601 x 3601 ints
    const std::size_t fileBlockSize = 3601;
    constexpr std::size_t chunkSize = CHUNK_RESOLUTION;

    const char * inFilename = "assets/N40W106.hgt";
    std::ifstream fin(inFilename, std::ios_base::binary);

    const char * outFilename = "assets/N40W106.chunk";
    std::ofstream fout(outFilename, std::ios_base::binary);

    if(!fin.good()) {
        std::cout << "cannot read asset file: '" << inFilename << "'\n";
        return -1;
    }

    if(!fout.good()) {
        std::cout << "cannot write to chunked file: '" << outFilename << "'\n";
        return -1;
    }

    // we write full-sized chunks even when there isn't enough data in a chunk
    // at the file scope. we just use the last sampled height.
    const std::size_t numChunksWide = std::ceill(
        static_cast<float>(fileBlockSize) / static_cast<float>(chunkSize)
    );

    // total number of chunks we will write into the .chunk file
    std::uint64_t numChunks = numChunksWide * numChunksWide;

    // write header - number of chunks
    fout.write(reinterpret_cast<char*>(&numChunks), sizeof(numChunks));

    // write TOCs first, update the chunk coordinates and offsets
    // as we generate the chunks
    ChunkTOC chunkTOC{};
    for(std::size_t cy = 0; cy < numChunksWide; ++cy) {
        for(std::size_t cx = 0; cx < numChunksWide; ++cx) {
            fout.write(reinterpret_cast<char*>(&chunkTOC), sizeof(chunkTOC));
        }
    }

    // file offset: start at first cell
    std::size_t offset = 0;
    fin.seekg(offset);

    // chunk TOC offset
    std::size_t offsetTOC = sizeof(numChunks);
    // fill and write each chunk's heightmap data (cx,cy)
    int16_t lastHeight = 0;
    for(std::size_t cy = 0; cy < numChunksWide; ++cy) {
        for(std::size_t cx = 0; cx < numChunksWide; ++cx) {

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

                    // read big-endian signed 16-bit integers
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

            // go back and update the chunkTOC with this offset
            std::uint64_t chunkOffset = fout.tellp();
            fout.seekp(offsetTOC, std::ios::beg);
            chunkTOC.chunkX = cx;
            chunkTOC.chunkZ = cy;
            chunkTOC.offset = chunkOffset;
            fout.write(reinterpret_cast<char*>(&chunkTOC), sizeof(chunkTOC));

            // seek back to where we were going to write the chunk
            fout.seekp(chunkOffset, std::ios::beg);
            // write chunked heights to file as raw int16_t
            fout.write(
                reinterpret_cast<const char*>(chunk.data()),
                chunk.size() * sizeof(int16_t)
            );

            // update TOC offset
            offsetTOC += sizeof(ChunkTOC);
        }
    }
}
