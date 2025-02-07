__kernel void vectorAdd(__global int* C, int zoom_level)
{
    int i = get_global_id(0);

    int x = i % 1920;
    int y = i / 1920;

//    double real_centre = 0;
//    double imag_centre = 0;

    float imag_centre =  0.00000000000000000278793706563379402178294753790944364927085054500163081379043930650189386849765202169477470552201325772332454726999999995;
    float real_centre = -1.74995768370609350360221450607069970727110579726252077930242837820286008082972804887218672784431700831100544507655659531379747541999999995;

    float real_range = 5;

    for (int k = 0; k < zoom_level; k++) {
        real_range *= 0.9;
    }

    float imag_range = (float)real_range / 1920. * 1080.;

    float z_real = 0;
    float z_imag = 0;

    float c_real = real_centre - real_range / 2 + real_range * ((float)x / 1920.);
    float c_imag = imag_centre - imag_range / 2 + imag_range * ((float)y / 1080.);

    float z_real_tmp;

    int iters = 0;
    int max_iters = 100 * sqrt(3. / imag_range);

    while (z_real*z_real < 4 & z_imag*z_imag < 4 & iters < max_iters) {
        z_real_tmp = z_real;
        z_real = z_real*z_real - z_imag*z_imag + c_real;
        z_imag = 2*z_real_tmp*z_imag + c_imag;
        iters++;
    }

    C[i] = iters % 255;
}