#pragma once

#include <atomic>
#include <mutex>
#include <algorithm>
#include "Device.h"
#include "ChunkWorld.h"
#include "Chunk.h"
#include "ChunkGeometry.h"

class ChunkGeometry;
class ChunkWorld;

class ChunkNode
{
public:
    // ── State constants ──────────────────────────────────────────────────────
    static const int UNINITIALIZED = 0; ///< No chunk data, no geometry.
    static const int DATA          = 1; ///< Chunk voxel data is ready.
    static const int GEOMETRY      = 2; ///< CPU mesh is ready, pending GPU upload.

    // ── Direction constants ───────────────────────────────────────────────────
    /*  left  = -x, right = +x
        front = -z, back  = +z
        up    = +y, down  = -y  */
    static const int LEFT  = 0;
    static const int RIGHT = 1;
    static const int FRONT = 2;
    static const int BACK  = 3;
    static const int UP    = 4;
    static const int DOWN  = 5;

    // ── Data members ─────────────────────────────────────────────────────────
    Chunk         chunk;
    ChunkGeometry *geometry;

    /**
     * Node state. Written atomically so worker threads can transition
     * UNINITIALIZED→DATA→GEOMETRY without a coarse lock.
     */
    std::atomic<int> state { UNINITIALIZED };

    /**
     * Mutex that serialises generateData() and generateGeometry() calls so
     * that two worker threads don't mesh the same node concurrently.
     */
    std::mutex generationMutex;

    int        seed;
    glm::ivec3 index;
    int        timestamp = -1;

    /** Pointers to the six direct neighbours (null until generated). */
    ChunkNode *neighbors[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    // ── Constructor / destructor ──────────────────────────────────────────────
    ChunkNode(glm::ivec3 _index, int _seed);

    // ── Graph traversal ───────────────────────────────────────────────────────
    void getNodes(std::vector<ChunkNode*> *nodes, int recursive = 0);
    void getGeometries(std::vector<Geometry*> *geometries, ChunkWorld *world, int recursive = 0);

    // ── Neighbour management ──────────────────────────────────────────────────
    void generateNeighbors(int recursive = 0);
    ChunkNode* searchNode(glm::ivec3 index, std::vector<ChunkNode*> *nodes);
    void searchNeighbors();
    ChunkNode* getNeighborPath(int path[], int size);

    // ── Data & geometry generation ────────────────────────────────────────────
    /**
     * Generate voxel data for this node (thread-safe, idempotent).
     * Safe to call from a worker thread; each node has independent data.
     */
    void generateData();

    /**
     * Generate CPU mesh for this node (thread-safe, idempotent).
     * Requires all neighbour nodes to have at least DATA state so that
     * cross-chunk face-culling queries succeed.
     */
    void generateGeometry(ChunkWorld *world);

    /** Convenience wrapper used by the old synchronous path. */
    Geometry* getGeometry(ChunkWorld *world);

    // ── Cleanup ───────────────────────────────────────────────────────────────
    void dispose(VkDevice &device);
};
