#pragma once
#include <span>

namespace slo::message {
struct MessageView {
    std::span<char const> buffer;

    std::span<char const> read(std::size_t n, std::size_t offset = 0) {
        return {buffer.data() + offset, n};
    }
};
}