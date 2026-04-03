#include "ChunkGeometry.h"

const glm::vec4 ChunkGeometry::BLOCK_UVS[8] =
{
	ChunkGeometry::calculateUV(12, 14, 16), //EMPTY
	ChunkGeometry::calculateUV(0, 0, 16),   //GRASS
	ChunkGeometry::calculateUV(2, 1, 16),   //SAND
	ChunkGeometry::calculateUV(1, 0, 16),   //STONE
	ChunkGeometry::calculateUV(2, 0, 16),   //DIRT
	ChunkGeometry::calculateUV(15, 13, 16), //WATER
	ChunkGeometry::calculateUV(15, 15, 16), //LAVA
	ChunkGeometry::calculateUV(2, 4, 16)    //CLOUD
};

const glm::vec4 ChunkGeometry::FOLIAGE_UVS[2] =
{
	ChunkGeometry::calculateUV(12, 0, 16), //FLOWER_RED  (500)
	ChunkGeometry::calculateUV(13, 0, 16)  //FLOWER_YELLOW (501)
};

glm::vec4 ChunkGeometry::getBlockUV(int value)
{
	// Foliage blocks start at 500
	if (value >= 500)
	{
		int foliageIndex = value - 500;
		if (foliageIndex >= 0 && foliageIndex < 2)
		{
			return FOLIAGE_UVS[foliageIndex];
		}
		// Unknown foliage: fall back to EMPTY UV
		return BLOCK_UVS[0];
	}

	// Regular blocks: must be in [0, 7]
	if (value >= 0 && value < 8)
	{
		return BLOCK_UVS[value];
	}

	// Unknown block: fall back to EMPTY UV
	return BLOCK_UVS[0];
}

glm::vec4 ChunkGeometry::calculateUV(int x, int y, int size)
{
	float step = 1.0f / size;
	return glm::vec4(step * x, step * y, step * x + step, step * y + step);
}

void ChunkGeometry::generate(Chunk *chunk, ChunkWorld *world)
{
	vertices.clear();
	indices.clear();

	int size = 0;

	glm::ivec3 start = { chunk->index.x * Chunk::SIZE, chunk->index.y * Chunk::SIZE , chunk->index.z * Chunk::SIZE };

	for (int x = 0; x < Chunk::SIZE; x++)
	{
		for (int z = 0; z < Chunk::SIZE; z++)
		{
			for (int y = 0; y < Chunk::SIZE; y++)
			{
				int value = chunk->data[x][y][z];

				if (value != Chunk::EMPTY)
				{
					// Safe UV lookup — prevents OOB for foliage block IDs (500+)
					glm::vec4 uv = getBlockUV(value);

					//Base
					float ix = x + start.x;
					float iy = y + start.y;
					float iz = z + start.z;

					//Positive
					float px = 0.5f + ix;
					float py = 0.5f + iy;
					float pz = 0.5f + iz;

					//Negative
					float nx = ix - 0.5f;
					float ny = iy - 0.5f;
					float nz = iz - 0.5f;

					// Helper: check if neighbor block is empty, checking cross-chunk via world
					auto isNeighborEmpty = [&](int bx, int by, int bz) -> bool
					{
						// Inside this chunk: use direct data access
						if (bx >= 0 && bx < Chunk::SIZE &&
							by >= 0 && by < Chunk::SIZE &&
							bz >= 0 && bz < Chunk::SIZE)
						{
							return chunk->data[bx][by][bz] == Chunk::EMPTY;
						}
						// Cross-chunk: query via world
						glm::ivec3 worldPos = {
							chunk->index.x * Chunk::SIZE + bx,
							chunk->index.y * Chunk::SIZE + by,
							chunk->index.z * Chunk::SIZE + bz
						};
						return world->getBlock(worldPos) == Chunk::EMPTY;
					};

					//Top face
					if (isNeighborEmpty(x, y + 1, z))
					{
						indices.push_back(size + 1);
						indices.push_back(size);
						indices.push_back(size + 2);
						indices.push_back(size + 1);
						indices.push_back(size + 2);
						indices.push_back(size + 3);

						vertices.push_back({ { nx, py, pz },{ 0, 1, 0 },{ uv.x, uv.y } });
						vertices.push_back({ { nx, py, nz },{ 0, 1, 0 },{ uv.z, uv.y } });
						vertices.push_back({ { px, py, pz },{ 0, 1, 0 },{ uv.x, uv.w } });
						vertices.push_back({ { px, py, nz },{ 0, 1, 0 },{ uv.z, uv.w } });
						size += 4;
					}

					//Bottom face
					if (isNeighborEmpty(x, y - 1, z))
					{
						indices.push_back(size + 3);
						indices.push_back(size + 1);
						indices.push_back(size);
						indices.push_back(size + 3);
						indices.push_back(size);
						indices.push_back(size + 2);

						vertices.push_back({ { px, ny, pz },{ 0, -1, 0 },{ uv.x, uv.y } });
						vertices.push_back({ { px, ny, nz },{ 0, -1, 0 },{ uv.z, uv.y } });
						vertices.push_back({ { nx, ny, pz },{ 0, -1, 0 },{ uv.x, uv.w } });
						vertices.push_back({ { nx, ny, nz },{ 0, -1, 0 },{ uv.z, uv.w } });
						size += 4;
					}

					//Front face
					if (isNeighborEmpty(x, y, z + 1))
					{
						indices.push_back(size);
						indices.push_back(size + 2);
						indices.push_back(size + 3);
						indices.push_back(size);
						indices.push_back(size + 3);
						indices.push_back(size + 1);

						vertices.push_back({ { nx, ny, pz },{ 0, 0, 1 },{ uv.x, uv.y } });
						vertices.push_back({ { nx, py, pz },{ 0, 0, 1 },{ uv.z, uv.y } });
						vertices.push_back({ { px, ny, pz },{ 0, 0, 1 },{ uv.x, uv.w } });
						vertices.push_back({ { px, py, pz },{ 0, 0, 1 },{ uv.z, uv.w } });
						size += 4;
					}

					//Back face
					if (isNeighborEmpty(x, y, z - 1))
					{
						indices.push_back(size + 2);
						indices.push_back(size + 3);
						indices.push_back(size + 1);
						indices.push_back(size + 2);
						indices.push_back(size + 1);
						indices.push_back(size);

						vertices.push_back({ { px, ny, nz },{ 0, 0, -1 },{ uv.x, uv.y } });
						vertices.push_back({ { px, py, nz },{ 0, 0, -1 },{ uv.z, uv.y } });
						vertices.push_back({ { nx, ny, nz },{ 0, 0, -1 },{ uv.x, uv.w } });
						vertices.push_back({ { nx, py, nz },{ 0, 0, -1 },{ uv.z, uv.w } });
						size += 4;
					}

					//Right face
					if (isNeighborEmpty(x + 1, y, z))
					{
						indices.push_back(size + 2);
						indices.push_back(size + 3);
						indices.push_back(size + 1);
						indices.push_back(size + 2);
						indices.push_back(size + 1);
						indices.push_back(size);

						vertices.push_back({ { px, ny, pz },{ 1, 0, 0 },{ uv.x, uv.y } });
						vertices.push_back({ { px, py, pz },{ 1, 0, 0 },{ uv.z, uv.y } });
						vertices.push_back({ { px, ny, nz },{ 1, 0, 0 },{ uv.x, uv.w } });
						vertices.push_back({ { px, py, nz },{ 1, 0, 0 },{ uv.z, uv.w } });
						size += 4;
					}

					//Left face
					if (isNeighborEmpty(x - 1, y, z))
					{
						indices.push_back(size);
						indices.push_back(size + 2);
						indices.push_back(size + 3);
						indices.push_back(size);
						indices.push_back(size + 3);
						indices.push_back(size + 1);

						vertices.push_back({ { nx, ny, nz },{ -1, 0, 0 },{ uv.x, uv.y } });
						vertices.push_back({ { nx, py, nz },{ -1, 0, 0 },{ uv.z, uv.y } });
						vertices.push_back({ { nx, ny, pz },{ -1, 0, 0 },{ uv.x, uv.w } });
						vertices.push_back({ { nx, py, pz },{ -1, 0, 0 },{ uv.z, uv.w } });
						size += 4;
					}
				}
			}
		}
	}
}
