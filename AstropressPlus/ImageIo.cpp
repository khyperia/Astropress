#include "ImageIo.h"

#include <fitsio.h>
#include <cstdio>
#include <fstream>

#ifdef _WIN32
const std::string os_dirsep("\\");
#else
const std::string os_dirsep("/");
#endif

std::runtime_error error(int status)
{
	if (status == 105)
	{
		puts("Error note: error 105 usually means the saving directory doesn't exist");
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
	printf("Dumping %s\n", final.c_str());
	SaveImage(final.c_str(), image);
}

Eigen::MatrixXd LoadImage(char const* filename)
{
	fitsfile* fitsFile;
	int status = 0;
	if (fits_open_file(&fitsFile, filename, READONLY, &status))
		throw error(status);
	int naxis;
	if (fits_get_img_dim(fitsFile, &naxis, &status))
		throw error(status);
	if (naxis != 2)
		throw std::runtime_error("Only 2d images supported");
	long axes[2];
	if (fits_get_img_size(fitsFile, naxis, axes, &status))
		throw error(status);
	long firstpix[] = { 1, 1 };
	Eigen::MatrixXd returnValue(axes[0], axes[1]);
	if (fits_read_pix(fitsFile, TDOUBLE, firstpix, returnValue.size(), nullptr, returnValue.data(), nullptr, &status))
		throw error(status);
	if (fits_close_file(fitsFile, &status))
		throw error(status);
	return returnValue;
}

void SaveImage(char const* filename, Eigen::MatrixXd const& matrix)
{
	remove(filename); // cfitsio gets upset when it tries to create an already-existing file
	fitsfile* fitsFile;
	int status = 0;
	if (fits_create_file(&fitsFile, filename, &status))
		throw error(status);
	long axes[] = { matrix.rows(), matrix.cols() };
	if (fits_create_img(fitsFile, DOUBLE_IMG, 2, axes, &status))
		throw error(status);
	long firstpix[] = { 1, 1 };
	if (fits_write_pix(fitsFile, TDOUBLE, firstpix, matrix.size(), static_cast<void*>(const_cast<double*>(matrix.data())), &status))
		throw error(status);
	if (fits_close_file(fitsFile, &status))
		throw error(status);
}
