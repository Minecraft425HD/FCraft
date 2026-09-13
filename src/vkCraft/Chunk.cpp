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

				//Generate terrain
				data[x][y][z] = getBlock(h, terrain);

				//Generate clouds
				if (h == CLOUD_LEVEL)
				{
					// Use long long to avoid integer overflow when seed is large
					int cloud = getHeight(v, w, (int)((long long)seed * 2)) * 3;

					if (cloud > CLOUD_LEVEL)
					{
						data[x][y][z] = CLOUD;
					}
				}
			}
		}
	}
}

double Chunk::fbm(double x, double y, int seed, double zoom, int octaves)
{
	double frequencyPower = 2.0;
	double amplitudePower = 0.5;

	double noise = 0;
	double amplitudeSum = 0;

	//Loop through the octaves
	for (int a = 0; a < octaves; a++)
	{
		//Increase the frequency with every loop of the octave.
		double frequency = pow(frequencyPower, a);

		//Decrease the amplitude with every loop of the octave.
		double amplitude = pow(amplitudePower, a);
		amplitudeSum += amplitude;

		//Perlin noise functions. It calculates all our zoom and frequency and amplitude
		noise += getNoise(x * frequency / zoom, y * frequency / zoom, seed) * amplitude;
	}

	// Normalize back to roughly [-1, 1] -- without this the summed octaves can
	// overshoot that range (up to ~1.97 with 6 octaves at 0.5 falloff).
	return noise / amplitudeSum;
}

int Chunk::getHeight(int x, int y, int seed, double noiseScale)
{
	// Rolling hills everywhere: moderate-frequency noise with a modest
	// amplitude, so most of the world is gentle, walkable terrain.
	double hills = fbm((double)x, (double)y, seed, noiseScale, 4);

	// Where mountain RANGES go: a second, much lower-frequency noise field.
	// Empirically (see terrain_test4 in the session scratchpad) this field's
	// value across a large area sits below ~0.31 about 85% of the time and
	// below ~0.62 about 99% of the time, so ramping from 0.3 to 0.75 turns
	// roughly the top 10-15% of the map into mountain range, the rest
	// staying flat hills, with a smooth (not hard-edged) transition since
	// it's a ramp over continuous noise rather than a threshold cutoff.
	double mask = fbm((double)x, (double)y, seed + 101, noiseScale * 5.0, 3);
	mask = (mask - 0.3) / (0.75 - 0.3);
	mask = mask < 0.0 ? 0.0 : (mask > 1.0 ? 1.0 : mask);
	mask = mask * mask;

	// Sharper detail noise, only expressed where the mask says "mountain".
	// Folded to its absolute value (ridge noise) so mountains only add
	// jagged peaks instead of sometimes carving the range into a canyon.
	double detail = fbm((double)x, (double)y, seed + 202, noiseScale * 0.5, 5);
	double ridge = detail < 0.0 ? -detail : detail;

	double baseElevation     = 24.0;
	double hillAmplitude     = 10.0;
	double mountainAmplitude = 160.0;

	double height = baseElevation
	              + hills * hillAmplitude
	              + mask * ridge * mountainAmplitude;

	return (int)height;
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

	//Integer declaration
	double s, t, u, v;

	//Get the surrounding pixels to calculate the transition.
	s = findNoise(floorx, floory, seed);
	t = findNoise(floorx + 1, floory, seed);
	u = findNoise(floorx, floory + 1, seed);
	v = findNoise(floorx + 1, floory + 1, seed);

	//Interpolate between the values.
	double int1 = interpolate(s, t, x - floorx);

	//Use x-floorx, to get 1st dimension, it's part of the cosine formula.
	double int2 = interpolate(u, v, x - floorx);

	//Use y-floory, to get the 2nd dimension.
	return interpolate(int1, int2, z - floory);
}

double Chunk::findNoise(double x, double z, int seed)
{
	int n = (int)x + (int)z * 57;
	n += seed;
	n = (n << 13) ^ n;

	// Masked to 31 bits so nn stays in [0, 2^31) -- without it the unsigned
	// int wrapped over the full 32-bit range, making this function return
	// values as low as -3 instead of the intended (-1, 1].
	unsigned int nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;

	return 1.0 - ((double)nn / 1073741824.0);
}

int Chunk::getBlock(int z, int height)
{
	//Above water level
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
	//Under water
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
