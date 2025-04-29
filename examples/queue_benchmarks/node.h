#pragma once

#include <cstddef>
#include <cstdint>
#include <cstddef>

template<size_t PAYLOAD_SIZE>
struct Node {
    char payload[PAYLOAD_SIZE];
    Node *next;
};