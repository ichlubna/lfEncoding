import pycuda.driver as cuda
import pycuda.autoinit
from pycuda.compiler import SourceModule
import numpy
import cv2
import sys
import os

directory = sorted(os.listdir(sys.argv[1]))
focus = numpy.single(sys.argv[2])
index = numpy.int32(sys.argv[3])

height, width, depth = cv2.imread(sys.argv[1]+"/"+directory[0]).astype(numpy.float32).shape
width = numpy.int32(width)
height = numpy.int32(height)
result = numpy.zeros((height,width,depth), numpy.float32)
resultGPU = cuda.mem_alloc(result.size * depth * 4)
imgGPU = cuda.mem_alloc(result.size * depth * 4)
mixFactor = numpy.single(1.0/len(directory))

static = cv2.imread(sys.argv[1]+"/"+directory[index]).astype(numpy.float32)
staticGPU = cuda.mem_alloc(result.size * depth * 4)
cuda.memcpy_htod(staticGPU, static)

kernel = SourceModule("""
    typedef struct {float r,g,b;} Pixel;
    typedef struct {Pixel data[3][3];} Block;

    __device__ Pixel addPixel(Pixel a, Pixel b, float factor)
    {
        return Pixel{a.r+factor*b.r, a.g+factor*b.g, a.b+factor*b.b};
    }

    __device__ Pixel mix(Pixel a, Pixel b, float factor)
    {
        float inverted = 1-factor;
        return Pixel{a.r*factor+b.r*inverted, a.g*factor+b.g*inverted, a.b*factor+b.b*inverted};
    }

    __device__ void storePixel(float *image, Pixel px, int id)
    {
        image[id] = px.r;
        image[id+1] = px.g;
        image[id+2] = px.b;
    }
    
    __device__ Pixel loadPixel(float *image, int x, int y, int width, int height)
    {
        int xx = min(x, width-1);
        xx = max(xx, 0);
        int yy = min(y, height-1);
        yy = max(yy, 0);
        int id = 3*(width*yy + xx);
        return {image[id], image[id+1], image[id+2]};
    }

    __device__ Pixel samplePixel(float *image, float x, int y, int width, int height)
    {
        int ax = ceil(x);
        Pixel a = loadPixel(image, ax, y, width, height);
        int bx = floor(x);
        Pixel b = loadPixel(image, bx, y, width, height);
        return mix(a, b, x-floor(x));
    }

    __global__ void process
      ( int width, int height, float *img, float *result, float *staticView, float offset, float mixFactor )
    {
        int x = threadIdx.x + blockIdx.x * blockDim.x;
        int y = threadIdx.y + blockIdx.y * blockDim.y;
        int id = 3*(width*y + x);

        if(x >= width || y >= height)
            return;
        
        Pixel newPixel = samplePixel(img, x+offset, y, width, height);
        if(x+offset >= width || x+offset < 0)
            newPixel = samplePixel(staticView, x, y, width, height);
        Pixel original = loadPixel(result, x, y, width, height);
        Pixel edited = addPixel(original, newPixel, mixFactor);
        storePixel(result, edited, id);
    }
    """)
func = kernel.get_function("process")

for i, filename in enumerate(directory):
    img = cv2.imread(sys.argv[1]+"/"+filename).astype(numpy.float32)
    cuda.memcpy_htod(imgGPU, img)
    offset = numpy.single(((index-i)/len(directory))*focus)
    func(width, height, imgGPU, resultGPU, staticGPU, offset, mixFactor, block=(16, 16, 1), grid=(int(width/16), int(height/16)), shared=0)

cuda.memcpy_dtoh(result, resultGPU)
result = result.astype(numpy.uint8)
cv2.imwrite(sys.argv[4], result)

