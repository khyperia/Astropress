#define COMPILE_WINDOWS_USE_GUI 1

#include <string>
#include <vector>
#include <memory>
#include <numeric>
#include <fitsio.h>

std::vector<std::string> GetArgsCommon(int argc, char* argv[])
{
	std::vector<std::string> result;
	for (int i = 1; i < argc; i++)
		result.push_back(std::string(argv[i]));
	return result;
}

#if _WIN32 && COMPILE_WINDOWS_USE_GUI

#undef TCHAR
#undef TBYTE
#include <shobjidl.h>
#include <locale>
#include <codecvt>

std::vector<std::string> IHateWindows() // I really do.
{
	IFileOpenDialog *pfd = nullptr;
	CoInitialize(nullptr);
	CoCreateInstance(CLSID_FileOpenDialog,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pfd));
	pfd->SetTitle(L"Open .fits files");
	FILEOPENDIALOGOPTIONS options;
	pfd->GetOptions(&options);
	options |= FOS_ALLOWMULTISELECT;
	pfd->SetOptions(options);
	auto resultShow = pfd->Show(nullptr);
	if (!SUCCEEDED(resultShow))
		return std::vector<std::string>();
	IShellItemArray* array;
	pfd->GetResults(&array);
	DWORD count;
	array->GetCount(&count);
	std::vector<std::string> result(count);
	for (unsigned int i = 0; i < result.size(); i++)
	{
		IShellItem* element;
		array->GetItemAt(i, &element);
		LPWSTR wstr;
		element->GetDisplayName(SIGDN_FILESYSPATH, &wstr);
		result[i] = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(std::wstring(wstr));
		CoTaskMemFree(wstr);
		element->Release();
	}

	array->Release();
	pfd->Release();
	CoUninitialize();
	return result;
}

std::vector<std::string> GetArgs(int argc, char* argv[])
{
	if (argc < 2)
		return IHateWindows();
	return GetArgsCommon(argc, argv);
}
#else
std::vector<std::string> GetArgs(int argc, char* argv[])
{
	if (argc < 2)
	{
		// linux people know how to use terminals, screw the GUI.
		puts("Usage:");
		puts("AstroStats [list of .fits files]");
		puts("Press any key to exit");
		getchar();
		return std::vector<std::string>();
	}
	return GetArgsCommon(argc, argv);
}
#endif

struct Image
{
	std::vector<long> size_;
	std::vector<double> data_;

	Image()
		: size_(0)
		, data_(0)
	{
	}

	Image(std::vector<long> size)
		: size_(size)
		, data_(std::accumulate(size.begin(), size.end(), 1L,
		[](long left, long right) { return left * right; }))
	{
	}

	void operator +=(const Image& right)
	{
		if (right.size_ != size_)
			throw std::runtime_error("Images had different dimentions");
		for (unsigned long i = 0; i < data_.size(); i++)
			data_[i] += right.data_[i];
	}

	void operator -=(const Image& right)
	{
		if (right.size_ != size_)
			throw std::runtime_error("Images had different dimentions");
		for (unsigned long i = 0; i < data_.size(); i++)
			data_[i] -= right.data_[i];
	}

	void operator *=(const Image& right)
	{
		if (right.size_ != size_)
			throw std::runtime_error("Images had different dimentions");
		for (unsigned long i = 0; i < data_.size(); i++)
			data_[i] *= right.data_[i];
	}

	void operator /=(double right)
	{
		for (unsigned long i = 0; i < data_.size(); i++)
			data_[i] /= right;
	}

	void sqrtAll()
	{
		for (unsigned long i = 0; i < data_.size(); i++)
			data_[i] /= sqrt(data_[i]);
	}
};

class FitsFile
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

public:
	FitsFile()
		: fitsFile(nullptr)
		, status(0)
	{

	}

	FitsFile(const char* filename)
		: status(0)
	{
		// not using fits_open_test to not check library version
		if (ffopen(&fitsFile, filename, READONLY, &status))
			throw error(status);
	}

	FitsFile(const FitsFile& copyFrom, const char* filename)
		: status(0)
	{
		remove(filename);
		if (fits_create_file(&fitsFile, filename, &status))
			throw error(status);
		if (fits_copy_header(copyFrom.fitsFile, fitsFile, &status))
			throw error(status);
	}

	~FitsFile()
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

	std::shared_ptr<Image> ReadImage()
	{
		auto image = std::make_shared<Image>(dims());
		std::vector<long> firstpix(image->size_.size());
		for (unsigned long i = 0; i < firstpix.size(); i++)
			firstpix[i] = 1;
		if (fits_read_pix(fitsFile, TDOUBLE, firstpix.data(), image->data_.size(), nullptr, image->data_.data(), nullptr, &status))
			throw error(status);
		return image;
	}

	void WriteImage(const Image& image)
	{
		std::vector<long> firstpix(image.size_.size());
		for (unsigned long i = 0; i < firstpix.size(); i++)
			firstpix[i] = 1;
		if (fits_write_pix(fitsFile, TDOUBLE, firstpix.data(), image.data_.size(), static_cast<void*>(const_cast<double*>(image.data_.data())), &status))
			throw error(status);
	}
};

int main(int argc, char* argv[])
{
	try
	{
		auto args = GetArgs(argc, argv);
		if (args.size() == 0)
			return -1;

		std::shared_ptr<Image> mean;
		std::shared_ptr<Image> M2;
		std::shared_ptr<Image> delta;
		std::shared_ptr<Image> temporary;

		auto outputFilename = args[0];
		auto lastIndexOutput = outputFilename.find_last_of("/\\");
		if (lastIndexOutput != std::string::npos)
			outputFilename = outputFilename.substr(0, lastIndexOutput + 1);
		outputFilename += "standardDeviation.fits";
		std::shared_ptr<FitsFile> outputFile;

		for (unsigned int n = 0; n < args.size(); n++)
		{
			printf("Computing %s\n", args[n].c_str());
			FitsFile file(args[n].c_str());
			auto image = file.ReadImage();
			if (n == 0)
			{
				mean = image;
				M2 = std::make_shared<Image>(mean->size_);
				delta = std::make_shared<Image>(mean->size_);
				temporary = std::make_shared<Image>(mean->size_);
				outputFile = std::make_shared<FitsFile>(file, outputFilename.c_str());
			}
			else
			{
				// http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Incremental_algorithm
				*delta = *image;
				*delta -= *mean;

				*temporary = *delta;
				*temporary /= n;
				*mean += *temporary;

				*temporary = *image;
				*temporary -= *mean;
				*delta *= *temporary;
				*M2 += *delta;
			}
		}
		puts("Computing final standard deviation");
		if (args.size() > 1)
			*M2 /= args.size() - 1;
		M2->sqrtAll();
		printf("Saving %s\n", outputFilename.c_str());
		outputFile->WriteImage(*M2);
	}
	catch (std::exception ex)
	{
		puts("Exception!");
		puts(ex.what());
		puts("Press any key to exit");
		getchar();
	}
}
