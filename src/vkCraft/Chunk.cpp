#include "Chunk.h"

void Chunk::setIndex(glm::ivec3 _index)
{
	index = _index;
}

glm::vec3 Chunk::getWorldPosition(int x, int y, int z)
{
	return glm::vec3(index.x * SIZE + x, index.y * SIZE + y, index.z * SIZE + z);
}

void Chunk::generate(int seed)
{
	for (int x = 0; x < SIZE; x++)
	{
		for (int z = 0; z < SIZE; z++)
		{
			int v = x + index.x * SIZE;
			int w = z + index.z * SIZE;

			int terrain = getHeight(v, w, seed);

			for (int y = 0; y < SIZE; y++)
			{
				int h = y + index.y * SIZE;

				// Generate terrain
				data[x][y][z] = getBlock(h, terrain);

				// Generate clouds
				if (h == CLOUD_LEVEL)
				{
					int cloud = getHeight(v, w, seed * 2) * 3;

					if (cloud > CLOUD_LEVEL)
					{
						data[x][y][z] = CLOUD;
					}
				}
			}
		}
	}
}

int Chunk::getHeight(int x, int y, int seed, double noiseScale)
{
	double frequencyPower = 2.0;
	double amplitudePower = 0.5;

	double zoom = noiseScale;
	double noise = 0;

	int octaves = 6;

	// Loop through the octaves
	for (int a = 0; a < octaves - 1; a++)
	{
		// Increase the frequency with every octave
		double frequency = pow(frequencyPower, a);

		// Decrease the amplitude with every octave
		double amplitude = pow(amplitudePower, a);

		// Perlin noise – combines zoom, frequency and amplitude
		noise += getNoise(((double)x) * frequency / zoom, ((double)y) / zoom * frequency, seed) * amplitude;
	}

	double maxHeight = 32 * 4.0;
	double minHeight = 0.0;

	return (int)(((noise + 1) / 2.0) * (maxHeight - minHeight));
}
	
double Chunk::interpolate(double a, double b, double x)
{
	double ft = x * 3.1415927;
	double f = (1.0 - cos(ft)) * 0.5;
	return a * (1.0 - f) + b * f;
}

double Chunk::getNoise(double x, double z, int seed)
{
	double floorx = (double)((int)x);
	double floory = (double)((int)z);

	// Get the four surrounding lattice points to calculate the transition
	double s, t, u, v;
	s = findNoise(floorx,     floory,     seed);
	t = findNoise(floorx + 1, floory,     seed);
	u = findNoise(floorx,     floory + 1, seed);
	v = findNoise(floorx + 1, floory + 1, seed);

	// Interpolate along x for both rows
	double int1 = interpolate(s, t, x - floorx);
	double int2 = interpolate(u, v, x - floorx);

	// Interpolate along z between the two rows
	return interpolate(int1, int2, z - floory);
}

double Chunk::findNoise(double x, double z, int seed)
{
	int n = (int)x + (int)z * 57;
	n += seed;
	n = (n << 13) ^ n;

	unsigned int nn = (n * (n * n * 60493 + 19990303) + 1376312589);

	return 1.0 - ((double)nn / 1073741824.0);
}

int Chunk::getBlock(int z, int height)
{
	// Above water level
	if (z > WATER_LEVEL)
	{
		if (z > height)
		{
			return EMPTY;
		}
		if (z == height)
		{
			return GRASS;
		}
		if (z > height - 7)
		{
			return DIRT;
		}
	}
	// Below water level
	else
	{
		if (z > height + 5)
		{
			return WATER;
		}
		if (z > height - 5)
		{
			return SAND;
		}
		if (z > height - 10)
		{
			return DIRT;
		}
	}

	return STONE;
}
