import numpy
import scipy
import scipy.ndimage
import scipy.optimize
import math

doDumpStars = False
doDumpFlat = False

def initConfig(dumpStars, dumpFlat):
    global doDumpStars
    global doDumpFlat
    doDumpStars = dumpStars
    doDumpFlat = dumpFlat

def threshHold(image):
    image = numpy.fft.fft2(image)
    # HARDCODED PARAMETER: keepSize
    # What is: Number of FFT low-frequency components to remove.
    # Adjust if: Nebula structure is showing through in --dump_flat.
    keepSize = 10
    mask = numpy.zeros(image.shape, dtype=numpy.bool)
    mask[:keepSize,:keepSize] = True
    mask[:keepSize,-keepSize:] = True
    mask[-keepSize:,:keepSize] = True
    mask[-keepSize:,-keepSize:] = True
    image[mask] = 0
    image = numpy.fft.ifft2(image).real
    imageMean = numpy.mean(image)
    imageStdev = numpy.std(image)
    # HARDCODED PARAMETER: keepThreshhold
    # What is: Standard deviations away from the mean a pixel has to be to be considered a star
    # Adjust if: Lots of noise is showing through in --dump_flat
    # See also: removeSizeThreshhold
    keepThreshhold = 2
    idx = (image - imageMean) / imageStdev > keepThreshhold
    image -= numpy.min(image[idx])
    image = numpy.maximum(image, 0)
    return image

# http://wiki.scipy.org/Cookbook/FittingData
def gaussian(height, center_x, center_y, width_x, width_y):
    """Returns a gaussian function with the given parameters"""
    width_x = float(width_x)
    width_y = float(width_y)
    return lambda x,y: height*numpy.exp(-(((center_x-x)/width_x)**2+((center_y-y)/width_y)**2)/2)

def moments(data):
    """Returns (height, x, y, width_x, width_y)
    the gaussian parameters of a 2D distribution by calculating its
    moments """
    total = data.sum()
    X, Y = numpy.indices(data.shape)
    x = (X*data).sum()/total
    y = (Y*data).sum()/total
    col = data[:, int(y)]
    width_x = numpy.sqrt(numpy.abs((numpy.arange(col.size)-y)**2*col).sum()/col.sum())
    row = data[int(x), :]
    width_y = numpy.sqrt(numpy.abs((numpy.arange(row.size)-x)**2*row).sum()/row.sum())
    height = data.max()
    return height, x, y, width_x, width_y

def fitgaussian(data):
    """Returns (height, x, y, width_x, width_y)
    the gaussian parameters of a 2D distribution found by a fit"""
    params = moments(data)
    errorfunction = lambda p: numpy.ravel(gaussian(*p)(*numpy.indices(data.shape)) - data)
    p, success = scipy.optimize.leastsq(errorfunction, params)
    return p
# end scipy link

def findStars(image):
    # find individual features
    image = threshHold(image)

    if doDumpFlat:
        import ImageIo
        ImageIo.dumpImage(image, "flat")

    labels, num_features = scipy.ndimage.label(image)

    if doDumpStars:
        dumpImage = image.copy()
        dumpImageMax = numpy.max(dumpImage)

    # remove small features
    sizes = scipy.ndimage.sum(image > 0, labels, range(num_features + 1))
    
    # HARDCODED PARAMETER: removeSizeThreshhold
    # What is: Minimum size (in pixels) a signal clump needs to be to be considered a star
    # Adjust if: Small stars are getting rejected in --dump_stars OR large noise clumps are getting accepted
    # See also: keepThreshhold
    removeSizeThreshhold = 25

    width, height = image.shape
    for maxx, maxy in scipy.ndimage.maximum_position(image, labels, [x for x in range(num_features + 1) if sizes[x] > removeSizeThreshhold]):
        maxx = int(maxx) # docs say maximum_position returns float
        maxy = int(maxy)
        sliceSize = 3
        if maxx < sliceSize or maxy < sliceSize or maxx >= width - sliceSize or maxy >= height - sliceSize:
            continue
        slice = image[maxx - sliceSize:maxx + sliceSize, maxy - sliceSize:maxy + sliceSize]
        height, x, y, width_x, width_y = fitgaussian(slice)
        x += maxx - sliceSize
        y += maxy - sliceSize
        skewMax = max(width_x / width_y, width_y / width_x)
        if skewMax > 2:
            print("Invalid star found at", x, y, ": too skewed, factor", skewMax)
            continue

        if doDumpStars:
            width_r = (width_x + width_y) / 2 * 4
            xx, yy = numpy.mgrid[:image.shape[0], :image.shape[1]]
            circle = (xx - x) ** 2 + (yy - y) ** 2
            circle = numpy.logical_and(circle < width_r * width_r + 9, circle > width_r * width_r - 9)
            dumpImage += circle * dumpImageMax

        # print("Star at ", x, y, "of height", height, "of width", width_x, width_y)
        yield (height, x, y)

    if doDumpStars:
        import ImageIo
        ImageIo.dumpImage(dumpImage, "stars")

def sortStars(stars):
    stars = list(stars)
    stars.sort(key=lambda val: val[0])
    stars = [(star[1], star[2]) for star in stars]
    # HARDCODED PARAMETER: maxStars
    # What is: Maximum number of brightest stars to keep
    # Adjust if: Processing is taking excessively long OR match acuraccy is poor due to low number of stars
    # See also: numberStarsToMatchOn
    maxStars = 50
    if len(stars) > maxStars:
        stars = stars[:maxStars]
    return stars

def closestLength(left, right, allowedLeft, allowedRight):
    min = 100000000
    found = -1,-1
    allowedInd = -1,-1
    lall = 0
    for l, li in ((left[i], i) for i in allowedLeft):
        rall = 0
        for r, ri in ((right[i], i) for i in allowedRight):
            diffx, diffy = l[0] - r[0], l[1] - r[1]
            diff = diffx * diffx + diffy * diffy
            if diff < min:
                min = diff
                found = li, ri
                allowedInd = lall, rall
            rall += 1
        lall += 1
    return found, allowedInd, min

def sortICP(left, right, transform):
    left = list(left)
    rightT = list(numpy.array((transform * numpy.matrix(right).T).T))
    resultLeft = []
    resultRight = []
    allowedLeft = list(range(len(left)))
    allowedRight = list(range(len(right)))
    indexList = []
    # HARDCODED PARAMETER: numberStarsToMatchOn
    # What is: Number of forced star matches to find (closest pairs first)
    # Adjust if: Inability to find a correct transformation, OR low accuracy of a match
    # See also: maxStars
    # Note from khyperia: I hate this constant. This should be more dynamic than it is right now.
    numberStarsToMatchOn = min(len(left), len(right)) // 2
    for iteration in range(numberStarsToMatchOn):
        (indL, indR), (allL, allR), mini = closestLength(left, rightT, allowedLeft, allowedRight)
        resultLeft.append(left[indL])
        resultRight.append(right[indR])
        indexList.append((indL, indR))
        del(allowedLeft[allL])
        del(allowedRight[allR])
    return resultLeft, resultRight, indexList

def ICP(leftList, rightList, guess):
    leftList = numpy.array(leftList)
    rightList = numpy.array([r + (1,) for r in rightList])
    oldIndexList = None
    iters = 0
    # HARDCODED PARAMETER: maxIcpIterations
    # What is: Number of Iterative Closest Point iterations to go through
    # Adjust if: It seems to be converging, but not enough iterations allowed OR a cyclic loop happens and it takes excessively long to time out
    maxIcpIterations = 20
    while iters < maxIcpIterations:
        resultLeft, resultRight, indexList = sortICP(leftList, rightList, guess)
        if indexList == oldIndexList:
            break
        guess = numpy.matrix(resultLeft).T * numpy.linalg.pinv(numpy.matrix(resultRight).T)
        oldIndexList = indexList
        iters += 1
    print("ICP took", iters, "iterations to converge")
    return guess

def register(reference, images):
    referenceStars = None
    guess = numpy.matrix([[1, 0, 0], [0, 1, 0]])
    for image in images:
        print("Registering")
        if referenceStars is None:
            dontAlign = reference is None
            if dontAlign:
                reference = image
            referenceStars = sortStars(findStars(reference))
            if dontAlign:
                yield image
                continue
        imageStars = sortStars(findStars(image))
        oldGuess = guess
        guess = ICP(imageStars, referenceStars, guess)

        scale = math.sqrt(guess[0,0] ** 2 + guess[0,1] ** 2), math.sqrt(guess[1,0] ** 2 + guess[1,1] ** 2)
        rotation = math.atan2(guess[1,0], guess[1,1]), math.atan2(-guess[0,1], guess[0,0])
        offset = guess[0,2], guess[1,2]
        affine = guess[:,:2]
        print("Scale:", "{0:.3f},{1:.3f}".format(*scale), "Rotation:", "{0:.3f},{1:.3f}".format(*rotation), "Offset:", "{0:.3f},{1:.3f}".format(*offset))

        rotDiff = numpy.sum(numpy.square(numpy.array([math.cos(rotation[0]), math.sin(rotation[0])]) - numpy.array([math.cos(rotation[1]), math.sin(rotation[1])])))
        # HARDCODED PARAMETER: maxRotDiff
        # What is: Threshhold that detects a bad affine transformation (due to theta calculation being different between the two rows of the affine transformation, i.e. strong skew)
        # Adjust if: Incorrect image matches are being applied OR correct image matches are being rejected
        maxRotDiff = 0.2
        if rotDiff > maxRotDiff * maxRotDiff:
            guess = oldGuess
            print("ERROR: BAD AFFINE TRANSFORMATION, skipping image!")
            continue

        yield scipy.ndimage.affine_transform(image, affine, offset, cval=numpy.median(image))
        