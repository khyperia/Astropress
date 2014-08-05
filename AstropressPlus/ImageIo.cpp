#include "ImageIo.h"

#include <fitsio.h>
#include <cstdio>
#include <fstream>
#include <iostream>

#ifdef _WIN32
const std::string os_dirsep("\\");
#else
const std::string os_dirsep("/");
#endif

std::runtime_error error(int status)
{
	if (status == 105)
	{
		std::cout << "Error note: error 105 usually means the saving directory doesn't exist" << std::endl;
	}
	return std::runtime_error(std::string("FITS file library error: code ") + std::to_string(status));
}

static std::string dumpDirectory;

void SetDumpDir(std::string directory)
{
	if (directory.length() < os_dirsep.length() || directory.compare(directory.length() - os_dirsep.length(), os_dirsep.length(), os_dirsep) != 0)
		directory += os_dirsep;
	dumpDirectory = directory;
}

void DumpImage(char const* basename, Eigen::MatrixXd const& image)
{
	if (dumpDirectory.length() == 0)
		throw std::runtime_error("Cannot dump image without --dump_dir");
	std::string base = dumpDirectory + std::string(basename);
	std::string final;
	for (int i = 1;; i++)
	{
		final = base + std::to_string(i) + std::string(".fits");
		std::ifstream checker(final);
		if (checker.fail())
			break;
	}
	std::cout << "Dumping " << final << std::endl;

	auto fits = FitsFile::CreateWrite(final.c_str());
	fits->WriteImage(image);
}

class FitsFileImpl : public FitsFile
{
	std::runtime_error error(int status)
	{
		char err[31];
		err[30] = '\0';
		fits_get_errstatus(status, err);
		if (status == 105)
			puts("Error note: error 105 usually means the saving directory doesn't exist");
		return std::runtime_error(std::string("FITS file library error: code ") + std::to_string(status) + std::string("\n")
			+ std::string(err) +
			std::string("\nSee http://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node26.html"));
	}
	int status;
	fitsfile* fitsFile;
	bool needsCreate;

public:
	FitsFileImpl(const char* filename)
		: status(0)
		, needsCreate(false)
	{
		// not using fits_open_test to not check library version
		if (ffopen(&fitsFile, filename, READONLY, &status))
			throw error(status);
	}

	FitsFileImpl(const FitsFileImpl& copyFrom, const char* filename)
		: status(0)
		, needsCreate(false)
	{
		remove(filename);
		if (fits_create_file(&fitsFile, filename, &status))
			throw error(status);
		if (fits_copy_header(copyFrom.fitsFile, fitsFile, &status))
			throw error(status);
	}

	FitsFileImpl(const char* filename, bool dummyOverloadVariable)
		: status(0)
		, needsCreate(true)
	{
		remove(filename);
		if (fits_create_file(&fitsFile, filename, &status))
			throw error(status);
	}

	~FitsFileImpl()
	{
		if (fitsFile == nullptr)
			return;
		if (fits_close_file(fitsFile, &status))
			throw error(status);
		fitsFile = nullptr;
	}

	int numDims()
	{
		int naxis;
		if (fits_get_img_dim(fitsFile, &naxis, &status))
			throw error(status);
		return naxis;
	}

	std::vector<long> dims()
	{
		std::vector<long> result(numDims());
		if (fits_get_img_size(fitsFile, static_cast<int>(result.size()), result.data(), &status))
			throw error(status);
		return result;
	}

	Eigen::MatrixXd ReadImage() override
	{
		auto vecDims = dims();
		if (vecDims.size() != 2)
			throw std::exception("Only monochrome images are supported");
		Eigen::MatrixXd result(vecDims[0], vecDims[1]);
		std::vector<long> firstpix(vecDims.size());
		for (unsigned long i = 0; i < firstpix.size(); i++)
			firstpix[i] = 1;
		if (fits_read_pix(fitsFile, TDOUBLE, firstpix.data(), result.size(), nullptr, result.data(), nullptr, &status))
			throw error(status);
		return result;
	}

	void WriteImage(const Eigen::MatrixXd& image) override
	{
		if (needsCreate)
		{
			long axes[] = { image.rows(), image.cols() };
			if (fits_create_img(fitsFile, DOUBLE_IMG, 2, axes, &status))
				throw error(status);
			needsCreate = false;
		}
		std::vector<long> firstpix(2);
		for (unsigned long i = 0; i < firstpix.size(); i++)
			firstpix[i] = 1;
		if (fits_write_pix(fitsFile, TDOUBLE, firstpix.data(), image.size(), static_cast<void*>(const_cast<double*>(image.data())), &status))
			throw error(status);
	}
};

std::shared_ptr<FitsFile> FitsFile::CreateRead(char const* filename)
{
	return std::make_shared<FitsFileImpl>(filename);
}

std::shared_ptr<FitsFile> FitsFile::CreateWrite(std::shared_ptr<FitsFile> copyHeadersFrom, char const* filename)
{
	return std::make_shared<FitsFileImpl>(*std::dynamic_pointer_cast<FitsFileImpl>(copyHeadersFrom), filename);
}

std::shared_ptr<FitsFile> FitsFile::CreateWrite(char const* filename)
{
	return std::make_shared<FitsFileImpl>(filename, true);
}