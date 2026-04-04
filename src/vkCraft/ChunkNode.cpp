#include "ChunkNode.h"

ChunkNode::ChunkNode(glm::ivec3 _index, int _seed)
{
    index    = _index;
    seed     = _seed;
    state    = UNINITIALIZED;
    chunk.setIndex(_index);
    geometry = new ChunkGeometry();
    // Data generation is deferred – the caller (ChunkWorld / VkCraft) drives
    // it explicitly so it can be offloaded to worker threads.
}

// ─────────────────────────────────────────────────────────────────────────────
// Graph traversal
// ─────────────────────────────────────────────────────────────────────────────

void ChunkNode::getNodes(std::vector<ChunkNode*> *nodes, int recursive)
{
    bool found = false;
    for (size_t i = 0; i < nodes->size(); i++)
    {
        if ((*nodes)[i]->index == index) { found = true; break; }
    }

    if (!found)
        nodes->push_back(this);

    if (recursive > 0)
    {
        generateNeighbors();
        for (unsigned int i = 0; i < 6; i++)
            neighbors[i]->getNodes(nodes, recursive - 1);
    }
}

Geometry* ChunkNode::getGeometry(ChunkWorld *world)
{
    if (state.load() < GEOMETRY)
        generateGeometry(world);
    return geometry;
}

void ChunkNode::getGeometries(std::vector<Geometry*> *geometries, ChunkWorld *world, int recursive)
{
    if (state.load() < GEOMETRY)
        generateGeometry(world);

    if (std::find(geometries->begin(), geometries->end(), geometry) == geometries->end())
        geometries->push_back(geometry);
    else
        return;

    if (recursive > 0)
    {
        generateNeighbors();
        for (unsigned int i = 0; i < 6; i++)
            neighbors[i]->getGeometries(geometries, world, recursive - 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Neighbour management
// ─────────────────────────────────────────────────────────────────────────────

void ChunkNode::generateNeighbors(int recursive)
{
    searchNeighbors();

    if (!neighbors[LEFT])
    {
        neighbors[LEFT]  = new ChunkNode({index.x - 1, index.y, index.z}, seed);
        neighbors[LEFT]->neighbors[RIGHT] = this;
    }
    if (!neighbors[RIGHT])
    {
        neighbors[RIGHT] = new ChunkNode({index.x + 1, index.y, index.z}, seed);
        neighbors[RIGHT]->neighbors[LEFT] = this;
    }
    if (!neighbors[UP])
    {
        neighbors[UP]   = new ChunkNode({index.x, index.y + 1, index.z}, seed);
        neighbors[UP]->neighbors[DOWN] = this;
    }
    if (!neighbors[DOWN])
    {
        neighbors[DOWN]  = new ChunkNode({index.x, index.y - 1, index.z}, seed);
        neighbors[DOWN]->neighbors[UP] = this;
    }
    if (!neighbors[FRONT])
    {
        neighbors[FRONT] = new ChunkNode({index.x, index.y, index.z - 1}, seed);
        neighbors[FRONT]->neighbors[BACK] = this;
    }
    if (!neighbors[BACK])
    {
        neighbors[BACK]  = new ChunkNode({index.x, index.y, index.z + 1}, seed);
        neighbors[BACK]->neighbors[FRONT] = this;
    }

    if (recursive > 0)
        for (unsigned int i = 0; i < 6; i++)
            neighbors[i]->generateNeighbors(recursive - 1);
}

void ChunkNode::searchNeighbors()
{
    std::vector<ChunkNode*> visited;

    auto trySearch = [&](int dir, glm::ivec3 target) {
        if (neighbors[dir] == nullptr)
        {
            visited.clear();
            ChunkNode *found = searchNode(target, &visited);
            if (found) neighbors[dir] = found;
        }
    };

    trySearch(LEFT,  {index.x - 1, index.y,     index.z    });
    trySearch(RIGHT, {index.x + 1, index.y,     index.z    });
    trySearch(FRONT, {index.x,     index.y,     index.z - 1});
    trySearch(BACK,  {index.x,     index.y,     index.z + 1});
    trySearch(UP,    {index.x,     index.y + 1, index.z    });
    trySearch(DOWN,  {index.x,     index.y - 1, index.z    });
}

ChunkNode* ChunkNode::searchNode(glm::ivec3 target, std::vector<ChunkNode*> *nodes)
{
    if (std::find(nodes->begin(), nodes->end(), this) != nodes->end())
        return nullptr;

    nodes->push_back(this);

    for (size_t i = 0; i < 6; i++)
        if (neighbors[i] && neighbors[i]->index == target)
            return neighbors[i];

    for (size_t i = 0; i < 6; i++)
        if (neighbors[i])
        {
            ChunkNode *n = neighbors[i]->searchNode(target, nodes);
            if (n) return n;
        }

    return nullptr;
}

ChunkNode* ChunkNode::getNeighborPath(int path[], int size)
{
    if (size < 1 || !neighbors[path[0]]) return nullptr;

    ChunkNode *node = neighbors[path[0]];
    for (int i = 1; i < size; i++)
    {
        if (node->neighbors[path[i]])
            node = node->neighbors[path[i]];
        else
            return nullptr;
    }
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data & geometry generation  (thread-safe, idempotent)
// ─────────────────────────────────────────────────────────────────────────────

void ChunkNode::generateData()
{
    // Fast path – no lock needed if already done.
    if (state.load(std::memory_order_acquire) >= DATA) return;

    // Serialize: only one thread runs chunk.generate().
    std::lock_guard<std::mutex> lock(generationMutex);

    // Double-check after acquiring the lock (another thread may have just
    // finished between the fast-path check and the lock acquisition).
    if (state.load(std::memory_order_relaxed) >= DATA) return;

    chunk.generate(seed);

    // Release store: all writes to chunk.data are visible to any thread that
    // subsequently observes state >= DATA via an acquire load.
    state.store(DATA, std::memory_order_release);
}

void ChunkNode::generateGeometry(ChunkWorld *world)
{
    // Ensure data is ready first.
    if (state.load(std::memory_order_acquire) < DATA)
        generateData();

    // Fast path.
    if (state.load(std::memory_order_acquire) >= GEOMETRY) return;

    // Serialize meshing per node.
    std::lock_guard<std::mutex> lock(generationMutex);

    // Double-check.
    if (state.load(std::memory_order_relaxed) >= GEOMETRY) return;

    geometry->generate(&chunk, world);

    state.store(GEOMETRY, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup
// ─────────────────────────────────────────────────────────────────────────────

void ChunkNode::dispose(VkDevice &device)
{
    if (state.load() > DATA)
    {
        geometry->dispose(device);
        state.store(DATA);
    }

    for (unsigned int i = 0; i < 6; i++)
    {
        if (neighbors[i])
        {
            int opposite = i ^ 1;
            neighbors[i]->neighbors[opposite] = nullptr;
            neighbors[i]->dispose(device);
            delete neighbors[i];
            neighbors[i] = nullptr;
        }
    }
}
