#include "EigenHeaders.h"
#include <iostream>
#include "ImageIo.h"
#include "StarFinder.h"
#include "AffineSolver.h"
#include "AffineResampler.h"
#include "RunningStacker.h"
#include <stdlib.h>

#define DoMainTry 1

static double shearThreshhold = 0.001;

void SetShearThreshhold(char const* str)
{
	try
	{
		shearThreshhold = stod(std::string(str));
		if (shearThreshhold < 0)
			throw std::runtime_error("--shear_threshhold cannot be < 0");
	}
	catch (std::invalid_argument)
	{
		throw std::runtime_error("Could not parse --shear_threshhold argument");
	}
}

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

bool checkParam(char** argv, const char* toMatch)
{
	if (strcmp(*argv, toMatch) == 0)
	{
		return true;
	}
	return false;
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
		else if ((value = grabParam(argv, "--shear_threshhold")))
			SetShearThreshhold(value);
		else if ((value = grabParam(argv, "--star_threshhold")))
			SetFlatPercent(value);
		else if ((value = grabParam(argv, "--freq_removal")))
			SetNumLowFrequencies(value);
		else if ((value = grabParam(argv, "--dump_dir")))
			SetDumpDir(std::string(value));
		else if (checkParam(argv, "--dump_flat"))
			EnableFlatImageDump();
		else if (checkParam(argv, "--dump_stars"))
			EnableStarLocationDump();
		else
			inputs.push_back(*argv);
		argv++;
	}
	if (!reference && inputs.size() > 0)
		reference = inputs[0];
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
			puts("Possible arguments:");
			puts("  --reference [image] - registers to specified image, as opposed to the first file");
			puts("  --out [image] - output filename (must be .fits)");
			puts("  --outstdev [image] - stdev output filename (must be .fits)");
			puts("  --shear_threshhold [value] - threshhold for rejection based on too high of affine shear (default 0.001)");
			puts("  --star_threshhold [percent] - threshhold for flat image (test with --dump_flat) (higher is more accepting) (default 1.0)");
			puts("  --freq_removal [percent] - number of low-frequency FFT factors to remove before finding stars (default 20)");
			puts("  --dump_dir [directory] - specifies the directory to dump debug information");
			puts("  --dump_flat - enable flattened image dumping (for star detection)");
			puts("  --dump_stars - enable dumping of found star locations");
			puts("  [list of input .fits images]");
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

			auto scaleX = sqrt(newGuess(0, 0) * newGuess(0, 0) + newGuess(0, 1) * newGuess(0, 1));
			auto scaleY = (newGuess(0, 0) * newGuess(1, 1) - newGuess(0, 1) * newGuess(1, 0)) / scaleX;
			auto rotation = atan2(newGuess(0, 1), newGuess(0, 0));
			auto shear = (newGuess(0, 0) * newGuess(1, 0) + newGuess(0, 1) * newGuess(1, 1)) / (newGuess(0, 0) * newGuess(1, 1) - newGuess(0, 1) * newGuess(1, 0));

			printf("Scale: %.3fx %.3fy Rot: %.5f Translation: %.2f %.2f Shear: %.5f\n", scaleX, scaleY, rotation, newGuess(0, 2), newGuess(1, 2), shear);

			if (abs(shear) > shearThreshhold)
			{
				puts("WARNING: Transform had a lot of shear, skipping this image");
				puts("WARNING: Try reducing the --star_threshold parameter");
				puts("WARNING: or modyfying --shear_threshhold if this judgement was wrong");
				continue;
			}
			transformGuess = newGuess;

			puts("Resampling and stacking");
			auto resample = AffineResample(image, transformGuess);
			stacker.Stack(resample);
		}
		if (output)
		{
			printf("Saving %s\n", output);
			SaveImage(output, stacker.Mean());
		}
		if (outputStdev)
		{
			printf("Saving %s\n", outputStdev);
			SaveImage(outputStdev, stacker.Stdev());
		}
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
