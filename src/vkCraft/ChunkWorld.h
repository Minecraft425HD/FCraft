#pragma once

#include <vector>
#include <unordered_map>
#include <mutex>
#include <math.h>

#include "Device.h"
#include "Geometry.h"
#include "ChunkNode.h"

class ChunkNode;

// ── Hash for glm::ivec3 so it can be used as unordered_map key ───────────────
struct IVec3Hash
{
    size_t operator()(const glm::ivec3 &v) const noexcept
    {
        size_t h = std::hash<int>{}(v.x);
        h ^= std::hash<int>{}(v.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(v.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

struct IVec3Equal
{
    bool operator()(const glm::ivec3 &a, const glm::ivec3 &b) const noexcept
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

/**
 * ChunkWorld manages the voxel world graph and provides thread-safe access
 * to block data via a flat registry (index → node).
 */
class ChunkWorld
{
public:
    ChunkNode *root;
    int        seed;

    /**
     * Geometries currently visible.  Written on the main thread only;
     * safe to read during rendering on the same thread.
     */
    std::vector<Geometry*>   geometries;

    /**
     * Nodes currently in view.  Written on the main thread only.
     */
    std::vector<ChunkNode*>  nodes;

    // ── Construction / destruction ────────────────────────────────────────────
    explicit ChunkWorld(int _seed);

    // ── Index helpers ─────────────────────────────────────────────────────────
    glm::ivec3 getIndex(glm::vec3 position);

    // ── World queries ─────────────────────────────────────────────────────────
    /**
     * Synchronous path: collect nodes, generate data + geometry, return list.
     * Still used by the old update path; the new multithreaded path calls
     * collectNodes() + getBlock() separately.
     */
    std::vector<Geometry*> getGeometries(glm::vec3 position, int distance);

    /**
     * Navigate the graph and return the node at @p index.
     * Thread-unsafe: call only from one thread at a time.
     * The internal `current` cursor is now a local variable, so re-entrant
     * calls are safe AS LONG AS only one thread drives graph navigation.
     */
    ChunkNode* getChunkNode(glm::ivec3 index);

    /**
     * Collect all ChunkNodes within @p distance of @p position (BFS via
     * the graph).  Newly created nodes are registered in the registry.
     * Call from the main thread.
     */
    std::vector<ChunkNode*> collectNodes(glm::vec3 position, int distance);

    /**
     * Thread-safe block lookup used by ChunkGeometry during meshing.
     * Returns Chunk::EMPTY if the node is not yet in the registry or has
     * no data.
     */
    int getBlock(glm::ivec3 position);

    // ── Registry ──────────────────────────────────────────────────────────────
    /**
     * Register @p node in the flat index map.  Call after constructing a node
     * so that worker threads can find it via getBlock().
     */
    void registerNode(ChunkNode *node);

    // ── Cleanup ───────────────────────────────────────────────────────────────
    void dispose(VkDevice &device);

private:
    /** Graph-navigation cursor – used only by getChunkNode() on the main thread. */
    ChunkNode *current { nullptr };

    /**
     * Flat O(1) lookup map.  Populated on the main thread (registerNode).
     * Read concurrently by worker threads via getBlock() – protected by
     * registryMutex with a shared_mutex so reads don't block each other.
     */
    std::unordered_map<glm::ivec3, ChunkNode*, IVec3Hash, IVec3Equal> registry;

    /**
     * Guards registry reads/writes.  Workers hold a shared lock while reading;
     * the main thread holds an exclusive lock when inserting new nodes.
     */
    mutable std::mutex registryMutex;
};
