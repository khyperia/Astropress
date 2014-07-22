#include "EigenAssert.h"
#include <iostream>
#include "ImageIo.h"
#include "StarFinder.h"
#include "AffineSolver.h"
#include "AffineResampler.h"
#include "RunningStacker.h"
#include <stdlib.h>

#define DoMainTry 1

char* grabParam(char**& argv, const char* toMatch)
{
	if (strcmp(*argv, toMatch) == 0)
	{
		argv++;
		char* value = *argv;
		if (value)
		{
			return value;
		}

		argv--; // back up to make sure parse() doesn't overshoot the null
		printf("Command parse warning: %s did not have parameter\n", toMatch);
	}
	return nullptr;
}

void parse(char* argv[], char*& reference, char*& output, char*& outputStdev, std::vector<char*>& inputs)
{
	argv++;
	while (*argv)
	{
		char* value;
		if ((value = grabParam(argv, "--reference")))
			reference = value;
		else if ((value = grabParam(argv, "--out")))
			output = value;
		else if ((value = grabParam(argv, "--outstdev")))
			outputStdev = value;
		else
			inputs.push_back(*argv);
		argv++;
	}
	if (!reference && inputs.size() > 0)
		reference = inputs[inputs.size() / 2];
}

int main(int argc, char* argv[])
{
#if DoMainTry
	try
	{
#endif
		if (argc == 0)
		{
			printf("Bad command: No program name passed in argv (what the heck?)\n");
			return -1;
		}

		char* referenceFile = nullptr;
		char* output = nullptr;
		char* outputStdev = nullptr;
		std::vector<char*> inputs;
		parse(argv, referenceFile, output, outputStdev, inputs);

		if (inputs.size() == 0)
		{
			printf("Usage: %s [--out [image]] [--outstdev [image]] [--reference [image]] [list of input .fits images]\n", argv[0]);
			return -1;
		}

		if (!output && !outputStdev)
		{
			puts("No --out or --outstdev specified. Did you mean this?");
			puts("Running anyway and discarding result.");
		}

		RunningStacker stacker;
		printf("Loading/registering reference %s\n", referenceFile);
		auto reference = FindStars(LoadImage(referenceFile));
		Eigen::MatrixXd transformGuess = Eigen::MatrixXd::Identity(2, 3);

		for (unsigned int inputIndex = 0; inputIndex < inputs.size(); inputIndex++)
		{
			printf("Loading %s\n", inputs[inputIndex]);
			auto image = LoadImage(inputs[inputIndex]);
			printf("Finding stars\n");
			auto stars = FindStars(image);
			printf("Found %i stars, calculating transform\n", stars.size());

			auto newGuess = SolveTransform(stars, reference, transformGuess);

			auto scale1 = sqrt(newGuess(0, 0) * newGuess(0, 0) + newGuess(0, 1) * newGuess(0, 1));
			auto scale2 = sqrt(newGuess(1, 0) * newGuess(1, 0) + newGuess(1, 1) * newGuess(1, 1));
			auto rotation1 = atan2(newGuess(1, 0), newGuess(1, 1));
			auto rotation2 = atan2(-newGuess(0, 1), newGuess(0, 0));
			printf("Scale: %f or %f Rot: %f or %f Translation: %f %f\n", scale1, scale2, rotation1, rotation2, newGuess(0, 2), newGuess(1, 2));

			if ((Eigen::Vector2d(cos(rotation1), sin(rotation1)) - Eigen::Vector2d(cos(rotation2), sin(rotation2))).squaredNorm() > 0.1 * 0.1)
			{
				puts("BAD TRANSFORM, too much shear (probably invalid): Skipping image!");
				continue;
			}
			transformGuess = newGuess;

			puts("Resampling");
			auto resample = AffineResample(image, transformGuess);
			puts("Stacking");
			stacker.Stack(resample);
		}
		if (output)
			SaveImage(output, stacker.Mean());
		if (outputStdev)
			SaveImage(outputStdev, stacker.Stdev());
#if DoMainTry
	}
	catch (std::exception e)
	{
		puts("Exception!");
		puts(e.what());
		puts("Press any key to exit");
		getchar();
	}
#endif
	return 0;
}
