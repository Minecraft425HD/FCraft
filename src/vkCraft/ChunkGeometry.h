#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <vector>
#include <array>

#include "Geometry.h"
#include "Vertex.h"
#include "Chunk.h"
#include "ChunkWorld.h"

class ChunkWorld;

/**
 * Geometry to represent a chunk in the world.
 */
class ChunkGeometry : public Geometry
{
public:
	/**
	 * List of UVs for the blocks.
	 * Index matches Chunk block constants (GRASS=1, SAND=2, etc.)
	 */
	static const glm::vec4 BLOCK_UVS[8];

	/**
	* List of UVs for the foliage.
	*/
	static const glm::vec4 FOLIAGE_UVS[2];

	/**
	 * Safe UV lookup: maps any block value to a valid UV.
	 * Foliage blocks (>=500) are mapped to FOLIAGE_UVS.
	 * Out-of-range values fall back to BLOCK_UVS[0] (EMPTY UV).
	 */
	static glm::vec4 getBlockUV(int value);

	/**
	 * Calculate the UV for the element x, y in a grid with size.
	 */
	static glm::vec4 calculateUV(int x, int y, int size);

	/**
	 * Generate new geometry data for the attached chunk.
	 */
	void generate(Chunk *chunk, ChunkWorld *world);
};
