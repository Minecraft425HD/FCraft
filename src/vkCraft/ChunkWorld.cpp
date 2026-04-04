#include "ChunkWorld.h"

ChunkWorld::ChunkWorld(int _seed)
{
    seed    = _seed;
    root    = new ChunkNode({0, 0, 0}, seed);
    current = root;
    registerNode(root);
}

// ─────────────────────────────────────────────────────────────────────────────
// Registry
// ─────────────────────────────────────────────────────────────────────────────

void ChunkWorld::registerNode(ChunkNode *node)
{
    std::lock_guard<std::mutex> lock(registryMutex);
    registry.emplace(node->index, node);
}

// ─────────────────────────────────────────────────────────────────────────────
// Index helper
// ─────────────────────────────────────────────────────────────────────────────

glm::ivec3 ChunkWorld::getIndex(glm::vec3 position)
{
    int x = position.x >= 0 ? (int)(position.x / Chunk::SIZE) : (int)(position.x / Chunk::SIZE) - 1;
    int y = position.y >= 0 ? (int)(position.y / Chunk::SIZE) : (int)(position.y / Chunk::SIZE) - 1;
    int z = position.z >= 0 ? (int)(position.z / Chunk::SIZE) : (int)(position.z / Chunk::SIZE) - 1;
    return { x, y, z };
}

// ─────────────────────────────────────────────────────────────────────────────
// Graph navigation  (main-thread only)
// ─────────────────────────────────────────────────────────────────────────────

ChunkNode* ChunkWorld::getChunkNode(glm::ivec3 index)
{
    glm::ivec3 offset = current->index - index;

    auto step = [&](int dir, int opp, int &coord, int target)
    {
        while (coord > target)
        {
            if (!current->neighbors[dir])
            {
                current->generateNeighbors();
                registerNode(current->neighbors[dir]);
            }
            current = current->neighbors[dir];
            --coord;
        }
        while (coord < target)
        {
            if (!current->neighbors[opp])
            {
                current->generateNeighbors();
                registerNode(current->neighbors[opp]);
            }
            current = current->neighbors[opp];
            ++coord;
        }
    };

    // Navigate X
    step(ChunkNode::LEFT,  ChunkNode::RIGHT, offset.x, 0);
    // Navigate Y (note: offset.y > 0 means we are ABOVE target → go DOWN)
    step(ChunkNode::DOWN,  ChunkNode::UP,    offset.y, 0);
    // Navigate Z
    step(ChunkNode::FRONT, ChunkNode::BACK,  offset.z, 0);

    return current;
}

// ─────────────────────────────────────────────────────────────────────────────
// Node collection  (main-thread)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<ChunkNode*> ChunkWorld::collectNodes(glm::vec3 position, int distance)
{
    ChunkNode *center = getChunkNode(getIndex(position));

    std::vector<ChunkNode*> result;
    center->getNodes(&result, distance);

    // Register any newly created nodes so worker threads can find them.
    for (ChunkNode *n : result)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        registry.emplace(n->index, n);  // emplace is a no-op if key exists
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Thread-safe block lookup
// ─────────────────────────────────────────────────────────────────────────────

int ChunkWorld::getBlock(glm::ivec3 position)
{
    glm::ivec3 chunkIdx = getIndex(glm::vec3(position));

    ChunkNode *node = nullptr;
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        auto it = registry.find(chunkIdx);
        if (it == registry.end()) return Chunk::EMPTY;
        node = it->second;
    }

    // Acquire load pairs with the release store in generateData():
    // ensures chunk.data writes are visible here.
    if (node->state.load(std::memory_order_acquire) < ChunkNode::DATA)
        return Chunk::EMPTY;

    int x = ((position.x % Chunk::SIZE) + Chunk::SIZE) % Chunk::SIZE;
    int y = ((position.y % Chunk::SIZE) + Chunk::SIZE) % Chunk::SIZE;
    int z = ((position.z % Chunk::SIZE) + Chunk::SIZE) % Chunk::SIZE;

    return node->chunk.data[x][y][z];
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy synchronous path (still used internally by getGeometries)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Geometry*> ChunkWorld::getGeometries(glm::vec3 position, int distance)
{
    ChunkNode *center = getChunkNode(getIndex(position));

    nodes.clear();
    center->getNodes(&nodes, distance);

    for (ChunkNode *n : nodes)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        registry.emplace(n->index, n);
    }

    geometries.clear();
    for (ChunkNode *n : nodes)
        geometries.push_back(n->getGeometry(this));

    return geometries;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup
// ─────────────────────────────────────────────────────────────────────────────

void ChunkWorld::dispose(VkDevice &device)
{
    root->dispose(device);
    delete root;
    root = nullptr;
}
