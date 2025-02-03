#include <iostream>
#include <fstream>

#include <iostream>
#include <algorithm>

#include <immintrin.h>
#include <valarray>

#define TRACY_ENABLE
#include "Tracy.hpp"


void print(__m256d vec)
    {
    double values[4];
    _mm256_storeu_pd(values, vec);
    std::cout << "[";
    for (int i = 0; i < 4; i++)
        std::cout << values[i] << " ";
    std::cout << "]" << std::endl;
    }

void print(int val)
    {
    std::cout << val << std::endl;
    }

void print(double val)
    {
    std::cout << val << std::endl;
    }

void print(double* arr, int len)
    {
    std::cout << "[";
    for (int i = 0; i < len; i++)
        std::cout << arr[i] << " ";
    std::cout << "]" << std::endl;
    }


class Colour
    {
private:
    double map_r[256];
    double map_g[256];
    double map_b[256];

public:
    Colour()
        {
        double stops[] = {0.0, 0.16, 0.42, 0.6425, 0.8575, 1.0};
        double reds[] = {0, 32, 237, 255, 0, 0};
        double greens[] = {7, 107, 255, 170, 2, 0};
        double blues[] = {100, 203, 255, 0, 0, 0};

        for (int i = 0; i < 256; i++)
            {
            double pos = static_cast<double>(i) / 255.0;

            int stop = std::upper_bound(stops, stops + 6, pos) - stops - 1;

            if (stop < 0)
                stop = 0;
            if (stop >= 5)
                stop = 4;

            double start = stops[stop];
            double end = stops[stop + 1];
            double range = end - start;
            double factor = (pos - start) / range;

            // Linear interpolation
            map_r[i] = (1 - factor) * reds[stop] + factor * reds[stop + 1];
            map_g[i] = (1 - factor) * greens[stop] + factor * greens[stop + 1];
            map_b[i] = (1 - factor) * blues[stop] + factor * blues[stop + 1];
            }
        }

    void get_colour(int i, double *r, double *g, double *b) const
        {
        if (i < 0) i = 0;
        if (i > 255) i = 255;

        *r = map_r[i];
        *g = map_g[i];
        *b = map_b[i];
        }
    };


class Image {
private:
    Colour colours;
    double** rows;

public:
    int height;
    int width;
    double aspect_ratio;

    Image(int w, int h):
        height(h), width(w), aspect_ratio((double)h / (double)w)
        {
        rows = new double*[height];
        for (int i = 0; i < height; i++)
            {
            rows[i] = new double[width];
            }
        }

    ~Image()
        {
        for (int i = 0; i < height; i++)
            {
            delete[] rows[i];
            }
        delete[] rows;
        }

    void display()
        {
        for (int i = 0; i < height; i++)
            {
            for (int j = 0; j < width; j++)
                {
                if(rows[i][j] > 0)
                    {
                    std::cout << "*";
                    }
                else
                    {
                    std::cout << " ";
                    }
                }
            std::cout << std::endl;
            }
        }

    void write_to_file(std::string filename)
        {
        // ZoneScoped;
        ZoneScopedNC("write_to_file", tracy::Color::Green);
        std::ofstream ofs("../outputs/" + filename + ".ppm", std::ios_base::out | std::ios_base::binary);
        ofs << "P6\n" << width << ' ' << height << "\n255\n";


        double r, g, b;
        for (auto j = 0u; j < height; ++j)
            for (auto i = 0u; i < width; ++i)
                {
                colours.get_colour((int)rows[j][i]%255, &r, &g, &b);

                ofs << static_cast<char>(r)
                    << static_cast<char>(g)
                    << static_cast<char>(b);
                }
            }

    double* get_row_ptr(int row_idx)
        {
        return rows[row_idx];
        }
};

int test_escape(double a, double b)
    {
    const int max_iter = 500;
    double z_real = 0;
    double z_imag = 0;

    double z_real_tmp;

    int iter = 0;
    while (abs(z_real) < 2 && abs(z_imag) < 2 && iter < max_iter)
        {
        z_real_tmp = z_real;
        z_real = z_real*z_real - z_imag*z_imag + a;
        z_imag = 2*z_real_tmp*z_imag + b;
        iter++;
        }
    return (double)iter / (double)max_iter;
    }

void populate_img(Image* img, double complex_centre, double real_centre,
                  double complex_range, double aspect_ratio)
    {
    double real_range = complex_range / aspect_ratio;

    double complex_start = complex_centre + complex_range / 2;
    double complex_end = complex_centre - complex_range / 2;

    double real_start = real_centre - real_range / 2;
    double real_end = real_centre + real_range / 2;

    for (int y = 0; y < img->height; y++)
        {
        double b = complex_start + ((double)y / img->height) * (complex_end - complex_start);

        double* row = img->get_row_ptr(y);
        for (int x = 0; x < img->width; x++)
            {
            double a = real_start + ((double)x / img->width) * (real_end - real_start);
            row[x] = test_escape(a, b) * 255;
            }
        }
    }

void populate_img_vectorised(Image* img, double complex_centre, double real_centre,
                             double complex_range)
    {
    // ZoneScoped;
    ZoneScopedNC("populate_img_vectorised", tracy::Color::PowderBlue);

    int max_iter = 200 * sqrt(3. / complex_range);

    double real_range = complex_range / img->aspect_ratio;

    double complex_start = complex_centre + complex_range / 2;
    double complex_end = complex_centre - complex_range / 2;

    double real_start = real_centre - real_range / 2;
    double real_end = real_centre + real_range / 2;

    double real_values[img->width];

    for (int x = 0; x < img->width; x++)
        {
        real_values[x] = real_start + ((double)x / img->width) * (real_range);
        }

    const __m256d abs_mask = _mm256_set1_pd(0x7FFFFFFFFFFFFFFF);
    const __m256d threshold = _mm256_set1_pd(2.0);

    for (int y = 0; y < img->height; y++)
        {
        double* row = img->get_row_ptr(y);

        double c_imag_scalar = complex_start + ((double)y / img->height) * (complex_end - complex_start);

        __m256d c_imag = _mm256_set1_pd(c_imag_scalar);

        for (int x = 0; x < img->width; x+=4)
            {
            int iters = 0;
            __m256d z_real = _mm256_set1_pd(0.);
            __m256d z_imag = _mm256_set1_pd(0.);

            const __m256d c_real = _mm256_loadu_pd(&real_values[x]);

            while (iters < max_iter)
                {
                const __m256d z_real_tmp = z_real;
                z_real = _mm256_sub_pd(
                    _mm256_mul_pd(z_real, z_real),
                    _mm256_mul_pd(z_imag, z_imag)
                );

                z_real = _mm256_add_pd(z_real, c_real);

                z_imag = _mm256_mul_pd(
                    _mm256_mul_pd(z_real_tmp, z_imag),
                    _mm256_set1_pd(2.)
                );

                z_imag = _mm256_add_pd(z_imag, c_imag);

                __m256d abs_vec = _mm256_and_pd(z_real, abs_mask);
                __m256d cmp_result = _mm256_cmp_pd(abs_vec, threshold, _CMP_GT_OQ);

                if(_mm256_movemask_pd(cmp_result)) break;
                iters++;
                }

            double z_real_arr[4] = {0};
            double z_imag_arr[4] = {0};

            _mm256_storeu_pd(z_real_arr, z_real);
            _mm256_storeu_pd(z_imag_arr, z_imag);

            for (int x_scalar = 0; x_scalar < 4; x_scalar++)
                {

                int scalar_iters = iters;
                double z_real_scalar = z_real_arr[x_scalar];
                double z_imag_scalar = z_imag_arr[x_scalar];

                while (
                        abs(z_real_scalar) < 2. &&
                        abs(z_imag_scalar) < 2.
                        )
                    {
                    double z_real_tmp_scalar = z_real_scalar;
                    z_real_scalar = z_real_scalar*z_real_scalar - z_imag_scalar*z_imag_scalar + real_values[x+x_scalar];
                    z_imag_scalar = 2*z_real_tmp_scalar*z_imag_scalar + c_imag_scalar;

                    if (scalar_iters > max_iter) break;
                    scalar_iters++;
                    }
                row[x+x_scalar] = (double)scalar_iters;
                }
            }
        }
    }

int main()
    {
//    0.743643887037151 + 0.131825904205330i
    // double complex_centre = 0.0091976760;
    // double real_centre = 0.2766433120;

    // double complex_centre = 1.;
    // double real_centre = 0.;

    double complex_centre = 0.00000000000000000278793706563379402178294753790944364927085054500163081379043930650189386849765202169477470552201325772332454726999999995;
    double real_centre =  -1.74995768370609350360221450607069970727110579726252077930242837820286008082972804887218672784431700831100544507655659531379747541999999995;

    double complex_range = 3;

#pragma omp parallel for schedule(dynamic,1)
    for (int i = 0; i < 250; i++)
        {
        std::cout << i << std::endl;
        // Image img(1920, 1080);

        Image img(600, 400);

        populate_img_vectorised(&img, complex_centre, real_centre, complex_range * std::pow(0.9, i));
        img.write_to_file(std::to_string(i));
        }
    return 0;
    }
