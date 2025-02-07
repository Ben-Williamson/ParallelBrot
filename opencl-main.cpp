#include <CL/opencl.hpp>   // Or <CL/cl.hpp> / <CL/cl2.hpp> if you have the C++ bindings
#include <iostream>
#include <vector>
#include <cstdlib>   // For exit()
#include <string>
#include <fstream>
#include <sstream>

#define TRACY_ENABLE
#include "Tracy.hpp"

class Image
	{
  public:
    int width;
    int height;

    double* r_vals;
    double* g_vals;
    double* b_vals;

    Image(int w, int h): width(w), height(h) {
      r_vals = new double[width*height];
      g_vals = new double[width*height];
      b_vals = new double[width*height];

      for (int i = 0; i < width*height; i++) {
        r_vals[i] = 0;
        g_vals[i] = 0;
        b_vals[i] = 0;
    	}
    }

    ~Image() {
      delete[] r_vals;
      delete[] g_vals;
      delete[] b_vals;
    }

    void write_to_file(std::string filename) {
        ZoneScoped;

	    std::ofstream ofs("../outputs-opencl/" + filename + ".ppm", std::ios_base::out | std::ios_base::binary);
    	ofs << "P6\n" << width << ' ' << height << "\n255\n";

	    double r, g, b;
    	for (auto j = 0u; j < height; ++j)
        	for (auto i = 0u; i < width; ++i) {
	          r = static_cast<double>(r_vals[i + j*width]);
    	      g = static_cast<double>(g_vals[i + j*width]);
        	  b = static_cast<double>(b_vals[i + j*width]);

              ofs << static_cast<char>(r)
                  << static_cast<char>(g)
                  << static_cast<char>(b);
            }
        }

    // void write_to_file(const std::string& filename) {
    //     std::ofstream ofs("../outputs-opencl/" + filename + ".ppm",
    //                       std::ios_base::binary);
    //     ofs << "P6\n" << width << ' ' << height << "\n255\n";
    //
    //     std::vector<unsigned char> buffer(width * height * 3);
    //
    //     // Parallel fill with OpenMP
    //     for (int j = 0; j < static_cast<int>(height); ++j) {
    //         for (int i = 0; i < static_cast<int>(width); ++i) {
    //             const size_t idx = 3 * (j * width + i);
    //             buffer[idx + 0] = static_cast<unsigned char>(r_vals[i + j * width]);
    //             buffer[idx + 1] = static_cast<unsigned char>(g_vals[i + j * width]);
    //             buffer[idx + 2] = static_cast<unsigned char>(b_vals[i + j * width]);
    //         }
    //     }
    //
    //     ofs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    //     ofs.close();
    // }

 	};


static std::string loadKernelFile(const char* filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening kernel file: " << filename << std::endl;
        exit(1);
    }
    std::ostringstream oss;
    oss << file.rdbuf(); // read file contents into stream
    return oss.str();    // return as string
}

// A helper function to check OpenCL errors (optional)
void checkError(cl_int err, const char* operation)
{
    if (err != CL_SUCCESS)
    {
        std::cerr << "OpenCL Error during: " << operation
                  << " | Error code: " << err << std::endl;
        exit(1);
    }
}

int main()
{
  int h = 1080;
  int w = 1920;
    // ----------------------------------------------------
    // 2) Initialize Data on the Host
    int N = h*w;
    std::vector<int> h_C(N);


    // ----------------------------------------------------
    // 3) Query the OpenCL platform and device
    cl_int err;
    cl_uint numPlatforms = 0;
    checkError(clGetPlatformIDs(0, nullptr, &numPlatforms), "clGetPlatformIDs (count)");

    if (numPlatforms == 0) {
        std::cerr << "No OpenCL platforms found!" << std::endl;
        return 1;
    }

    std::vector<cl_platform_id> platforms(numPlatforms);
    checkError(clGetPlatformIDs(numPlatforms, platforms.data(), nullptr), "clGetPlatformIDs (list)");

    // Just pick the first platform for simplicity
    cl_platform_id platform = platforms[0];

    // Query devices on this platform
    cl_uint numDevices = 0;
    checkError(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &numDevices), "clGetDeviceIDs (count)");

    if (numDevices == 0) {
        std::cerr << "No OpenCL devices found on the platform!" << std::endl;
        return 1;
    }

    std::vector<cl_device_id> devices(numDevices);
    checkError(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, numDevices, devices.data(), nullptr), "clGetDeviceIDs (list)");

    // Pick the first device
    cl_device_id device = devices[0];
    // ----------------------------------------------------

    // ----------------------------------------------------
    // 4) Create an OpenCL context and command queue
    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    checkError(err, "clCreateContext");

    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
    checkError(err, "clCreateCommandQueue");
    // ----------------------------------------------------

    // ----------------------------------------------------
    // 5) Create buffers on the device
    size_t bytes = N * sizeof(int);

    cl_mem d_C = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, nullptr, &err);
    checkError(err, "clCreateBuffer(d_C)");
    // ----------------------------------------------------

    // ----------------------------------------------------
    // 6) Write host data to device buffers
    // ----------------------------------------------------

    // ----------------------------------------------------
    // 7) Create the program from source, build it, and create the kernel

    const std::string kernelSource = loadKernelFile("../simplebrot.cl");
    const char* source = kernelSource.c_str();

    std::cout << kernelSource << std::endl;

    cl_program program = clCreateProgramWithSource(context, 1, &source, nullptr, &err);
    checkError(err, "clCreateProgramWithSource");

    // Build (compile) the program
    err = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS)
    {
        // Print build errors if any
        size_t logSize = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        std::string buildLog(logSize, '\0');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, &buildLog[0], nullptr);
        std::cerr << "Error in kernel:\n" << buildLog << std::endl;
        checkError(err, "clBuildProgram");
    }

    // Create the kernel
    cl_kernel kernel = clCreateKernel(program, "vectorAdd", &err);
    checkError(err, "clCreateKernel(vectorAdd)");
    // ----------------------------------------------------

    // ----------------------------------------------------
    // 8) Set kernel arguments
//    checkError(clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_A), "clSetKernelArg(A)");
//    checkError(clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_B), "clSetKernelArg(B)");

    for (int zoom_level = 0; zoom_level < 100; zoom_level++) {
        {
            ZoneScopedN("wait for queue");
            checkError(clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_C), "clSetKernelArg(C)");
            checkError(clSetKernelArg(kernel, 1, sizeof(int), &zoom_level), "clSetKernelArg(zoom_level)");
            // ----------------------------------------------------

            // ----------------------------------------------------
            // 9) Enqueue the kernel execution
            size_t globalSize = N;  // We have N elements
            // We use a 1D NDRange

            checkError(clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &globalSize, nullptr, 0, nullptr, nullptr),
                     "clEnqueueNDRangeKernel");

            checkError(clFinish(queue), "clFinish");

            // ----------------------------------------------------

            // ----------------------------------------------------
            // 10) Read results back to the host
            checkError(clEnqueueReadBuffer(queue, d_C, CL_TRUE, 0, bytes, h_C.data(), 0, nullptr, nullptr),
                       "clEnqueueReadBuffer(d_C)");
        }
        // ----------------------------------------------------

        // ----------------------------------------------------
        // 11) Print results
        Image img(w, h);

        for (int i = 0; i < N; i++)
        {
            img.r_vals[i] = h_C[i];
            img.g_vals[i] = h_C[i];
            img.b_vals[i] = h_C[i];
        }
        img.write_to_file(std::to_string(zoom_level));
    }
    // ----------------------------------------------------

    // ----------------------------------------------------
    // 12) Cleanup
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseMemObject(d_C);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    // ----------------------------------------------------

    return 0;
}
