import numpy

def stack(images):
    # We must not iterate over the input twice to get mean *and* standard deviation, so we use:
    # http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Incremental_algorithm
    n = 0
    mean = None
    M2 = None

    for image in images:
        n = n + 1
        if mean is None:
            mean = image
            M2 = numpy.zeros(image.shape, dtype=image.dtype)
            continue
        
        delta = image - mean
        mean = mean + delta / n
        M2 = M2 + delta * (image - mean)
        print("Stacked image")

    if M2 is None:
        stdev = None
    else:
        stdev = numpy.sqrt(M2 / n) # I don't like Bessel's correction.

    return mean, stdev
