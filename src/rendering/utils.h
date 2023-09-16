#pragma once

#include <vector>
#include <tuple>

#include "vertex.h"

namespace rendering
{
    [[nodiscard]] std::tuple<std::vector<Vertex>, std::vector<math::Vector3u>> subdivide(
        const std::vector<Vertex>& vertices,
        const std::vector<math::Vector3u>& indices
    );
}

template <typename T>
void append_range(std::vector<T>& destination, std::vector<T>&& source) {
    destination.insert(
            destination.end(),
            std::make_move_iterator(source.begin()),
            std::make_move_iterator(source.end())
    );
}
