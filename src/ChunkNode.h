#pragma once

#include <vector>
#include <random>
#include "Device.h"
#include "ChunkWorld.h"
#include "Chunk.h"
#include "ChunkGeometry.h"

class ChunkGeometry;
class ChunkWorld;

class ChunkNode
{
public:
	/**
	* Uninitialized state, the node has no chunk data and geometry.
	*/
	static const int UNINITIALIZED = 0;

	/**
	* The node only has chunk data.
	*/
	static const int DATA = 1;

	/**
	* The node has chunk and geometry data.
	*/
	static const int GEOMETRY = 2;

	/*
	* left is - x, right + x
	* front is - z, back is + z
	* up is + y, down is - y
	*/
	static const int LEFT  = 0;
	static const int RIGHT = 1;
	static const int FRONT = 2;
	static const int BACK  = 3;
	static const int UP    = 4;
	static const int DOWN  = 5;

	/**
	* Chunk data.
	*/
	Chunk chunk;

	/**
	* Geometry to represent this chunk.
	*/
	ChunkGeometry *geometry;

	/**
	* Node state.
	*/
	int state;

	/**
	* World seed (propagated from ChunkWorld).
	*/
	int seed;

	/**
	* Node index relative to the root.
	*/
	glm::ivec3 index;

	/**
	* Numeric timestamp, that can be compared with a world timestamp and updated on generation.
	* Can be used to avoid recursive propagation of updates in nodes.
	*/
	int timestamp = -1;

	/**
	* Pointer to neighbor chunks.
	*/
	ChunkNode *neighbors[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	ChunkNode(glm::ivec3 _index, int _seed);

	/**
	* Compute a deterministic per-chunk RNG seeded from the world seed and
	* the chunk's XZ position.  The same world seed + same chunk position
	* always produces the same terrain, matching Minecraft's behaviour.
	*
	* Usage inside generateData():
	*   std::mt19937 rng = makeChunkRng();
	*   int height = std::uniform_int_distribution<int>(4, 16)(rng);
	*/
	std::mt19937 makeChunkRng() const
	{
		uint32_t chunkSeed =
			static_cast<uint32_t>(seed)
			^ (static_cast<uint32_t>(index.x) * 1664525u)
			^ (static_cast<uint32_t>(index.z) * 1013904223u);
		return std::mt19937(chunkSeed);
	}

	/**
	* Get geometries from this node and its neighbors recursively.
	*/
	void getNodes(std::vector<ChunkNode*> *nodes, int recursive = 0);

	/**
	* Get geometries from this node and its neighbors recursively.
	*/
	void getGeometries(std::vector<Geometry*> *geometries, ChunkWorld *world, int recursive = 0);

	/**
	* Generate neighbors of this node and assign direct relations between nodes.
	* Before creating the nodes it searches for already known nodes.
	*/
	void generateNeighbors(int recursive = 0);

	/**
	* Try to get node from its index, recursively.
	*/
	ChunkNode* searchNode(glm::ivec3 index, std::vector<ChunkNode*> *nodes);

	/**
	* Search neighbors connections using already known relations.
	* Only searches second order relations.
	*/
	void searchNeighbors();

	/**
	* Get element from path if it is reachable.
	* Path is described as directional jumps in an array.
	*/
	ChunkNode* getNeighborPath(int path[], int size);

	/**
	* Generate data for this node.
	* Use makeChunkRng() for all random decisions to ensure
	* deterministic, seed-based terrain generation.
	*/
	void generateData();

	/**
	* Generate geometry for this node.
	* If it still has no chunk data, generate it first.
	*/
	void generateGeometry(ChunkWorld *world);

	/**
	* Get geometry of this node.
	*/
	Geometry* getGeometry(ChunkWorld *world);

	/**
	* Dispose all the geometries attached to this node recursively.
	* Checks if the geometry has Vulkan buffers and also disposes them.
	*/
	void dispose(VkDevice &device);
};
