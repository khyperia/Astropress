import numpy
import os
import glob
import ctypes
try:
    from PIL import Image
except:
    Image = None

try:
    fitsio = ctypes.CDLL("cfitsio")
except:
    fitsio = None

getDumpDir = None

def initConfig(dumpDir, dumpExt):
    if dumpDir is None:
        return
    if dumpExt is None:
        dumpExt = "tiff"
    while len(dumpExt) > 0 and dumpExt.startswith('.'):
        dumpExt = dumpExt[1:]

    def dumpDirGetter(name):
        index = 0
        filename = None
        while True:
            filename = os.path.join(dumpDir, name + str(index) + "." + dumpExt)
            if not os.path.exists(filename):
                break
            index += 1
        return filename
    
    global getDumpDir
    getDumpDir = dumpDirGetter

def loadFits(filename):
    if fitsio is None:
        raise Exception("Cannot load a .fits file: cfitsio.dll not found")
    fitsFile = ctypes.c_void_p()
    status = ctypes.c_int()
    if fitsio.ffopen(ctypes.byref(fitsFile), ctypes.c_char_p(filename.encode("utf-8")), 0, ctypes.byref(status)):
        raise Exception("FITS error: Could not open file: " + str(status))
    numAxes = ctypes.c_int()
    if fitsio.ffgidm(fitsFile, ctypes.byref(numAxes), ctypes.byref(status)):
        raise Exception("FITS error: Could not get number of axes: " + str(status))
    if numAxes.value != 2:
        raise Exception("FITS error: Only two dimentional images supported")
    axesLength = (ctypes.c_int * numAxes.value)(0, 0)
    if fitsio.ffgisz(fitsFile, numAxes, axesLength, ctypes.byref(status)):
        raise Exception("FITS error: Could not get image dimentions: " + str(status))
    # int CFITS_API ffgpxv(fitsfile *fptr, int  datatype, long *firstpix, LONGLONG nelem, void *nulval, void *array, int *anynul, int *status);
    outputArr = numpy.zeros((axesLength[1], axesLength[0]))
    firstPix = (ctypes.c_long * 2)(1, 1)
    if fitsio.ffgpxv(fitsFile, ctypes.c_int(82), firstPix, ctypes.c_longlong(outputArr.size), ctypes.c_void_p(), outputArr.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), ctypes.c_void_p(), ctypes.byref(status)):
        raise Exception("FITS error: Could not read image data: " + str(status))
    if fitsio.ffclos(fitsFile, ctypes.byref(status)):
        raise Exception("FITS error: Could not close file: " + str(status))
    return outputArr

def saveFits(array, filename):
    if fitsio is None:
        raise Exception("Cannot save a .fits file: cfitsio.dll not found")

    try:
        os.remove(filename) # cfitsio gets upset when it tries to create an already-existing file
    except OSError:
        pass

    # int CFITS_API ffinit(  fitsfile **fptr, const char *filename, int *status);
    fitsFile = ctypes.c_void_p()
    status = ctypes.c_int()
    if fitsio.ffinit(ctypes.byref(fitsFile), ctypes.c_char_p(filename.encode("utf-8")), ctypes.byref(status)):
        raise Exception("FITS error: Could not create file: " + str(status))
    # int CFITS_API ffcrim(fitsfile *fptr, int bitpix, int naxis, long *naxes, int *status);
    naxes = (ctypes.c_long * 2)(array.shape[1], array.shape[0])
    if fitsio.ffcrim(fitsFile, ctypes.c_int(-64), 2, naxes, ctypes.byref(status)):
        raise Exception("FITS error: Could not insert new image: " + str(status))
    # int CFITS_API ffppx(fitsfile *fptr, int datatype, long *firstpix, LONGLONG nelem, void *array, int *status);
    firstPix = (ctypes.c_long * 2)(1, 1)
    if fitsio.ffppx(fitsFile, ctypes.c_int(82), firstPix, ctypes.c_longlong(array.size), array.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), ctypes.byref(status)):
        raise Exception("FITS error: Could not write image data: " + str(status))
    if fitsio.ffclos(fitsFile, ctypes.byref(status)):
        raise Exception("FITS error: Could not close file: " + str(status))

def loadImage(filename):
    print("Loading", filename)
    if filename.lower().endswith(".fits"):
        array = loadFits(filename)
    else:
        if Image is None:
            raise Exception("Cannot load image: Pillow not found")
        array = numpy.array(Image.open(filename))

    array = array.astype(numpy.float64)
    return array

def loadImageGlob(globPattern):
    globs = (glob.glob(pat) for pat in globPattern)
    return (loadImage(image) for sublist in globs for image in sublist if image is not None)

def saveImage(array, filename):
    print("Saving", filename)

    if not os.path.exists(os.path.dirname(filename)):
        os.mkdir(os.path.dirname(filename))

    if filename.lower().endswith(".fits"):
        saveFits(array, filename)
    else:
        if Image is None:
            raise Exception("Cannot save image: Pillow not found")
        # HARDCODED PARAMETER: 65535
        # What is: 16 bit max value, we assume that we are writing a 16 bit image
        # Adjust if: You're not writing a 16 bit tiff :)
        image = Image.fromarray(array.clip(0, 65535).astype(numpy.ushort), 'I;16')
        image.save(filename)

def dumpImage(array, name):
    if getDumpDir is None:
       raise Exception("Cannot dump intermediate files without specifying --dump_dir")
    filename = getDumpDir(name)
    saveImage(array, filename)
