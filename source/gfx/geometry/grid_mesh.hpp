#pragma once

#include <vector>

#include "core/types.hpp"

namespace gfx::geometry {

struct GridMesh {
    std::vector<core::u16> vertexBufferX{};
    std::vector<core::u16> vertexBufferZ{};
    std::vector<core::u16> indexBuffer{};
    core::u32 indexCount{};
    core::u32 vertexStride{};
};

class MeshGenerator {
public:
    static GridMesh createGridMesh(std::size_t resolution) {
        if(resolution > std::numeric_limits<core::u16>::max()) {
            static_assert("resolution too great for use with core::u16");
        }

        const std::size_t N = resolution;
        GridMesh grid{};

        // generate grid vertices
        grid.vertexBufferX.reserve(N * N);
        grid.vertexBufferZ.reserve(N * N);
        for(core::u16 z = 0; z < N; ++z) {
            for(core::u16 x = 0; x < N; ++x) {
                grid.vertexBufferX.push_back(x);
                grid.vertexBufferZ.push_back(z);
            }
        }
        // vX: [0, 1, 2, ..., 0, 1, 2, ..., N - 1]
        // vZ: [0, 0, 0, ..., 1, 1, 1, ..., N - 1]
        // generate grid indices
        grid.indexBuffer.reserve((N - 1) * (N - 1) * 6);
        for(core::u16 z = 0; z < N - 1; ++z) {
            for(core::u16 x = 0; x < N - 1; ++x) {
                // quad is:
                //  i0 -- i1 -- (x increasing)
                //   |    |
                //  i2 -- i3
                //   |
                //  (z increasing)

                // i0: get index of vertex (x,z) at column x, row z
                core::u16 i0 = z * N + x;
                core::u16 i1 = i0 + 1; // next col
                core::u16 i2 = i0 + N; // next row
                core::u16 i3 = i2 + 1; // diag

                // first triangle
                grid.indexBuffer.push_back(i0);
                grid.indexBuffer.push_back(i2);
                grid.indexBuffer.push_back(i1);

                // second triangle
                grid.indexBuffer.push_back(i1);
                grid.indexBuffer.push_back(i2);
                grid.indexBuffer.push_back(i3);
            }
        }

        return grid;
    }
};

}
