# Lightfield random-access coding and measurement framework

Having a directory with sorted LF images, the data can be encoded as:
```
./lfCodec --input input/ --output output/
```

To encode a frame in the middle of the sequence use:
```
./lfCodec --input ./output/ --output ./decoded/ --factor 0.5
```

The implementation also measures the en/decoding times and size of the results.

## Scripts
Additional scripts used for measurements.
- interpolate.py - simple interpolator using a simplified technique of [LF Rendering](http://fit.vut.cz/research/publication/12429/)
- metrics - prints image similarity metrics PSNR, SSIM, WMAF
- sparser - creates sparser dataset from the given dense one
- visualQuality - used to measure average times and visual quality 
- reverseOrder - simple tool that reverses the order of files in a folder - renaming
- lfBlending.py - implements a simple LF blending algorithm with variable focus value 
- GPUDecodingTest - compares the decoding performance of GPU accelerated H.265 and JPEG2000
- timeHEVC.sh - comparison of decoding times between all I and P frames
- avgCalculator.py - helper script for processing of the measured data - gets avg values from columns in the files
- measurement - helper script for visual quality measurement

