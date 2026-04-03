#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <vector>

/**
 * Chunks store the world data.
 *
 * They are generated using a world generator and are organized into a chunk world.
 */
class Chunk
{
public:
	static const int SIZE = 32;
	static const int WATER_LEVEL = 0;
	static const int CLOUD_LEVEL = SIZE * 3;

	static const int EMPTY = 0;

	// Block types
	static const int GRASS = 1;
	static const int SAND = 2;
	static const int STONE = 3;
	static const int DIRT = 4;
	static const int WATER = 5;
	static const int LAVA = 6;
	static const int CLOUD = 7;

	// Foliage (plants, flowers, trees)
	static const int FLOWER_RED = 500;
	static const int FLOWER_YELLOW = 501;
	static const int BIRCH = 502;

	/**
	 * Chunk data, constants defined in this class.
	 */
	int data[SIZE][SIZE][SIZE];

	/**
	 * Chunk position, in steps of one.
	 */
	glm::ivec3 index;

	/**
	 * Chunk constructor receives a position and creates chunk data based on it.
	 *
	 * The chunk position is organized in x,y,z; each step represents a move
	 * of CHUNK_SIZE in the world.
	 */
	void setIndex(glm::ivec3 _index);

	/**
	 * Get world position of a block inside this chunk.
	 */
	glm::vec3 getWorldPosition(int x, int y, int z);

	/**
	 * Generate chunk data based on its position.
	 */
	void generate(int seed);

	/**
	 * Get terrain height at (x, y) world coordinates.
	 *
	 * noiseScale controls the horizontal zoom of the noise function.
	 * Also receives the world seed.
	 */
	int getHeight(int x, int y, int seed, double noiseScale = 150);

	/**
	 * Interpolate values using cosine interpolation.
	 */
	double interpolate(double a, double b, double x);

	/**
	 * Get smooth Perlin-style noise for (x, z) with the given seed.
	 */
	double getNoise(double x, double z, int seed);

	double findNoise(double x, double z, int seed);

	/**
	 * Determine the block type at height z in a column with the given terrain height.
	 */
	int getBlock(int z, int height);
};
