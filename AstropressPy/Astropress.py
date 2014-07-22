import Register
import Stack
import ImageIo
import argparse

# Dependancies:
# python 3.4 (should work on 3.3, etc., however this is untested)
# numpy
# scipy
# Pillow (optional, for standard image loading/saving)
# cfitsio.dll/so (optional, for .fits loading/saving)

# These usually exist as packages on common linux distrubutions as
# "python3", "python3-numpy", "python3-scipy", "python3-pillow", "cfitsio"
# although on ubuntu Pillow is known as "python3-imaging"

# Important!
# This program uses an algorithm known as Iterative Closest Point for point set registration
# http://en.wikipedia.org/wiki/Iterative_closest_point
# This means that input images *must* be relatively similar in both their translation and their rotations.
# Something like max 25% the total size of the image translation, and max 30 degrees rotation should be fine.
# This mostly just means that if you did a meridian flip, manually rotate the flipped images before feeding into this program.

# Also, the "intial guess" transformation is carried between each image, so drift is accounted for.
# (the final transformation of image N is used as the initial guess for image N+1)
# This means that you should probably list your images in chronological order, if there is considerable drift.
# However, only one image is used as the reference frame - it is not updated, to reduce accumulative error.
# This image is specified as either the first image in the sequence, or as --reference [image]
# (if using --reference, the reference frame *is not* included in the stack)

# Notes on the implementation of algorithms:
# The author (khyperia) does not like constants or user-input parameters in their astro processing code.
# They would prefer it if all constants/inputs were instead dynamically figured out at runtime.
# However, this dream scenerio is not always the case, and some constants need to be put in place.
# Therefore, all constants are marked with a special comment - and what they are, and why you should change them.
# These are mostly in Register.py
# Here's a fake example:

# HARDCODED PARAMETER: stdevThreshholdAmount
# What is: Threshhold that rejects outliers based on their z-score (i.e. zScore=(x-mean)/stdev) being larger than an amount
# Adjust if: Outliers are included in the result OR inliers are being rejected

if __name__ == "__main__":
    argParser = argparse.ArgumentParser(description="Calibrate, register, and stack astrophotography images")
    argParser.add_argument("images", nargs='+', metavar="image", help="Input images to stack")
    argParser.add_argument("--reference", nargs='?', metavar="image", help="Reference image (not included in stack)")
    argParser.add_argument("--out", nargs='?', metavar="filename", help="Output file (mean)")
    argParser.add_argument("--outstdev", nargs='?', metavar="filename", help="Output file (stdev)")
    argParser.add_argument("--dump_dir", nargs='?', metavar="directory", help="Output directory for image dumps")
    argParser.add_argument("--dump_format", nargs='?', metavar="extension", help="Output format for image dumps")
    argParser.add_argument("--dump_stars", action="store_true", help="Dump found stars to an image")
    argParser.add_argument("--dump_flat", action="store_true", help="Dump flattened image (which is what the star finder uses)")
    argSuccess = True
    try:
        args = argParser.parse_args()
    except (Exception, SystemExit):
        argSuccess = False
    
    if argSuccess:
        ImageIo.initConfig(args.dump_dir, args.dump_format)
        Register.initConfig(args.dump_stars, args.dump_flat)
    
        list = ImageIo.loadImageGlob(args.images)
        reference = None
        if args.reference is not None:
            reference = ImageIo.loadImage(args.reference)
        reg = Register.register(reference, list)
        stack, stackDev = Stack.stack(reg)
        if args.out is not None:
            ImageIo.saveImage(stack, args.out)
        if args.outstdev is not None:
            ImageIo.saveImage(stackDev, args.outstdev)
