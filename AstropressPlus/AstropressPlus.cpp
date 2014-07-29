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
		std::cout << "Command parse warning: " << toMatch << " did not have parameter" << std::endl;
	}
	return nullptr;
}

bool checkParam(char** argv, const char* toMatch)
{
	return strcmp(*argv, toMatch) == 0;
}

void parse(char* argv[], char*& reference, char*& output, char*& outputStdev, std::vector<char*>& inputs, bool& doRegister, double& subsample)
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
		else if (checkParam(argv, "--noreg"))
			doRegister = false;
		else if ((value = grabParam(argv, "--subsample")))
			subsample = stod(std::string(value), nullptr);
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
		{
			std::string arg(*argv);
			if (arg.length() > 0 && arg[0] == '-')
				throw std::runtime_error("Unknown argument " + arg + " (if it's a file, rename the file to not start with a dash)");
			inputs.push_back(*argv);
		}
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
			std::cout << "Bad command: No program name passed in argv (what the heck?)" << std::endl;
			return -1;
		}

		char* referenceFile = nullptr;
		char* output = nullptr;
		char* outputStdev = nullptr;
		std::vector<char*> inputs;
		bool doRegister = true;
		double subsample = 1;
		parse(argv, referenceFile, output, outputStdev, inputs, doRegister, subsample);

		if (inputs.size() == 0)
		{
			std::cout
				<< "Possible arguments:" << std::endl
				<< "  --reference [image] - registers to specified image, as opposed to the first file" << std::endl
				<< "  --out [image] - output filename (must be .fits)" << std::endl
				<< "  --outstdev [image] - stdev output filename (must be .fits)" << std::endl
				<< "  --noreg - Do not align images" << std::endl
				<< "  --subsample [number] - Resize images by factor of [number] when stacking" << std::endl
				<< "  --shear_threshhold [value] - threshhold for rejection based on too high of affine shear (default 0.001)" << std::endl
				<< "  --star_threshhold [percent] - threshhold for flat image (test with --dump_flat) (higher is more accepting) (default 1.0)" << std::endl
				<< "  --freq_removal [percent] - number of low-frequency FFT factors to remove before finding stars (default 20)" << std::endl
				<< "  --dump_dir [directory] - specifies the directory to dump debug information" << std::endl
				<< "  --dump_flat - enable flattened image dumping (for star detection)" << std::endl
				<< "  --dump_stars - enable dumping of found star locations" << std::endl
				<< "  [list of input .fits images]" << std::endl;
			return -1;
		}

		if (!output && !outputStdev)
		{
			std::cout
				<< "No --out or --outstdev specified. Did you mean this?" << std::endl
				<< "Running anyway and discarding result." << std::endl;
		}

		RunningStacker stacker;
		std::cout << "Loading/registering reference " << referenceFile << std::endl;
		auto referenceFits = FitsFile::CreateRead(referenceFile);
		auto reference = FindStars(referenceFits->ReadImage());
		Eigen::MatrixXd transformGuess = Eigen::MatrixXd::Identity(2, 3);

		for (unsigned int inputIndex = 0; inputIndex < inputs.size(); inputIndex++)
		{
			std::cout << "Loading " << inputs[inputIndex] << std::endl;
			auto image = FitsFile::CreateRead(inputs[inputIndex])->ReadImage();
			std::cout << "Removing bad pixels... ";
			int numBadPixels = RemoveBadPixels(image);
			std::cout << "found " << numBadPixels << " bad pixels" << std::endl;
			if (doRegister)
			{
				std::cout << "Finding stars" << std::endl;
				auto stars = FindStars(image);
				std::cout << "Found " << stars.size() << " stars, calculating transform" << std::endl;

				auto newGuess = SolveTransform(stars, reference, transformGuess);

				auto scaleX = sqrt(newGuess(0, 0) * newGuess(0, 0) + newGuess(0, 1) * newGuess(0, 1));
				auto scaleY = (newGuess(0, 0) * newGuess(1, 1) - newGuess(0, 1) * newGuess(1, 0)) / scaleX;
				auto rotation = atan2(newGuess(0, 1), newGuess(0, 0));
				auto shear = (newGuess(0, 0) * newGuess(1, 0) + newGuess(0, 1) * newGuess(1, 1)) / (newGuess(0, 0) * newGuess(1, 1) - newGuess(0, 1) * newGuess(1, 0));

				std::cout << "Scale: " << scaleX << "x " << scaleY << "y Rot: " << rotation << " Translation: " << newGuess(0, 2) << " " << newGuess(1, 2) << " Shear: " << shear << std::endl;

				if (abs(shear) > shearThreshhold)
				{
					std::cout << "WARNING: Transform had a lot of shear, skipping this image" << std::endl;
					std::cout << "WARNING: Try reducing the --star_threshold parameter" << std::endl;
					std::cout << "WARNING: or modyfying --shear_threshhold if this judgement was wrong" << std::endl;
					continue;
				}
				transformGuess = newGuess;

				std::cout << "Resampling and stacking" << std::endl;
				image = AffineResample(image, transformGuess, subsample);
			}
			stacker.Stack(image);
		}
		if (output)
		{
			std::cout << "Saving " << output << std::endl;
			FitsFile::CreateWrite(referenceFits, output)->WriteImage(stacker.Mean());
		}
		if (outputStdev)
		{
			std::cout << "Saving " << outputStdev << std::endl;
			FitsFile::CreateWrite(referenceFits, outputStdev)->WriteImage(stacker.Stdev());
		}
#if DoMainTry
	}
	catch (std::exception e)
	{
		std::cout << "Exception!" << std::endl;
		std::cout << e.what() << std::endl;
		std::cout << "Press any key to exit" << std::endl;
		getchar();
	}
#endif
	return 0;
}
