import pycuda.driver as cuda
import pycuda.autoinit
from pycuda.compiler import SourceModule
import numpy
import cv2
import sys

imgA = cv2.imread(sys.argv[1]).astype(numpy.float32)
imgB = cv2.imread(sys.argv[2]).astype(numpy.float32)
height, width, depth = imgA.shape
width = numpy.int32(width)
height = numpy.int32(height)
result = numpy.zeros((height,width,depth), numpy.float32)

imgAGPU = cuda.mem_alloc(imgA.size * depth * 4)
imgBGPU = cuda.mem_alloc(imgA.size * depth * 4)
resultGPU = cuda.mem_alloc(result.size * depth * 4)
cuda.memcpy_htod(imgAGPU, imgA)
cuda.memcpy_htod(imgBGPU, imgB)

kernel = SourceModule("""
    typedef struct {float r,g,b;} Pixel;
    typedef struct {Pixel data[3][3];} Block;

    __device__ int min(int a, int b)
    {
        return (a<b) ? a : b;
    }
    
    __device__ int max(int a, int b)
    {
        return (a>b) ? a : b;
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

    __device__ Block loadBlock(float *image, float x, int y, int width, int height)
    {  
        Block block;
        for(int xx=0; xx<3; xx++)
            for(int yy=0; yy<3; yy++)
                block.data[xx][yy] = samplePixel(image, x+xx-1, y+yy-1, width, height);
        return block;
    }

    __device__ float distance(Pixel a, Pixel b)
    {
        float cr = abs(a.r-b.r);
        float cg = abs(a.g-b.g);
        float cb = abs(a.b-b.b);
        //Chebyshev
        return max(max(cr, cg), cb);//cr*cr + cg*cg + cb*cb;
    }

    __device__ float blockDistance(Block a, Block b)
    {
       float d = 0;
        for(int xx=0; xx<3; xx++)
            for(int yy=0; yy<3; yy++)
                d += distance(a.data[xx][yy], b.data[xx][yy]);
        return d;
    }

    __global__ void process
      ( int width, int height,
        float *imgA, float *imgB, float *result )
        {
        int x = threadIdx.x + blockIdx.x * blockDim.x;
        int y = threadIdx.y + blockIdx.y * blockDim.y;

        if(x >= width || y >= height)
            return;

        int id = 3*(width*y + x);
        int maxFocus = width/100;
        float step = 0.1;

        float minDistance = 9999.0;
        for(float i=0; i<maxFocus; i+=step)
        {
            Block bB = loadBlock(imgB, x-i, y, width, height);
            Block bA = loadBlock(imgA, x+i, y, width, height);
            float d = blockDistance(bA, bB);
            if(d < minDistance)
            {
                minDistance = d;
                //image
                //storePixel(result, mix(bA.data[1][1], bB.data[1][1], 0.5), id);
                //focusMap
                float focus = round(i/maxFocus*255);
                storePixel(result, Pixel{focus, focus, focus}, id);
           }
        }
    }
    """)
func = kernel.get_function("process")
func(width, height, imgAGPU, imgBGPU, resultGPU, \
     block=(16, 16, 1), \
     grid=(int(width/16), int(height/16)), shared=0)

cuda.memcpy_dtoh(result, resultGPU)
result = result.astype(numpy.uint8)
#cv2.imwrite(sys.argv[3], result)
medianFocusMap = cv2.medianBlur(result,9)
cv2.imwrite(sys.argv[3], medianFocusMap)
