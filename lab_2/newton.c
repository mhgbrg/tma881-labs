#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <stdbool.h> 
#include <getopt.h> 
#include <pthread.h> 

// Implementation plan:
// x. Function for doing Newton's method for a single x value
// x. Function for doing Newton's method for a 1000x1000 grid using a single thread, writing result to file
// x. Write results to file
// x. Parse command line args
// x. Handle poly_degree = 1...9  
// 6. Optimize
// 7. Multithreaded implementation of above

void print_complex_double(double complex dbl);
void init_roots();
void init_results_matrix();
void* worker_thread_main(void* restrict arg);
struct result newton(double complex x);
bool illegal_value(double complex x);
int get_nearby_root(double complex x);
double complex next_x(double complex x);
double complex f(double complex x);
double complex f_deriv(double complex x);
void write_files();

#define OUT_OF_BOUNDS 10000000000
#define ERROR_MARGIN 0.001
#define X_MIN -2.0
#define X_MAX 2.0
#define MAX_ITERATIONS 50
#define COLOR_TRIPLET_LEN 12
#define GRAYSCALE_COLOR_LEN 4

long picture_size;
char poly_degree;

char num_roots;
double complex* roots;

char num_threads;

struct result {
    char root;
    char iterations;
};

struct result* results_values;
struct result** results;

char attractors_colors[10][COLOR_TRIPLET_LEN] = {
    "181 181 181 ", // Color used for points that don't converge
    "204 51  46  ",
    "208 106 47  ",
    "208 152 47  ",
    "208 200 47  ",
    "119 208 47  ",
    "51  177 209 ",
    "51  83  209 ",
    "175 51  209 ",
    "208 47  149 ",
};

int main(int argc, char* argv[]) {
    // TODO: Handle errors when arguments are not supplied correctly.
    int option;
    while ((option = getopt(argc, argv, "t:l:")) != -1) {
        switch (option) {
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'l':
                picture_size = atoi(optarg);
                break;
            default:
                printf("Usage: ./newton -t<num_thread> -l<picture_size> <poly_degree>");
                return 1;
        }
    }
    poly_degree = atoi(argv[argc - 1]);

    printf("t: %d, l: %ld, d: %d\n", num_threads, picture_size, poly_degree);

    init_roots();
    init_results_matrix();

    pthread_t threads[num_threads];

    int ret;
    for (char i = 0; i < num_threads; i++) {
        char* arg = (char*) malloc(sizeof(char));
        *arg = i;
        if ((ret = pthread_create(threads + i, NULL, worker_thread_main, (void*) arg))) {
            printf("Error creating thread: %d\n", ret);
            exit(1);
        }
    }

    for (char i = 0; i < num_threads; i++) {
        if ((ret = pthread_join(threads[i], NULL))) {
            printf("Error joining thread: %d\n", ret);
            exit(1);
        }
    }

    write_files();

    free(roots);
    free(results);
    free(results_values);

    return 0;
}

void print_complex_double(double complex dbl) {
    printf("%lf%+lfi\n", creal(dbl), cimag(dbl));
}

void init_roots() {
    num_roots = poly_degree;
    roots = (double complex*) malloc(sizeof(double complex) * num_roots);
    roots[0] = 1;
    switch (poly_degree) {
        case 1:
            break;
        case 2:
            roots[1] = -1;
            break;
        case 3:
            roots[1] = -cpow(-1, 1.0 / 3.0);
            roots[2] = cpow(-1, 2.0 / 3.0);
            break;
        case 4:
            roots[1] = -1;
            roots[2] = -I;
            roots[3] = I;
            break;
        case 5:
            roots[1] = -cpow(-1, 0.2);
            roots[2] = cpow(-1, 0.4);
            roots[3] = -cpow(-1, 0.6);
            roots[4] = cpow(-1, 0.8);
            break;
        case 6:
            roots[1] = -1;
            roots[2] = -cpow(-1, 1.0 / 3.0);
            roots[3] = cpow(-1, 1.0 / 3.0);
            roots[4] = -cpow(-1, 2.0 / 3.0);
            roots[5] = cpow(-1, 2.0 / 3.0);
            break;
        case 7:
            roots[1] = -cpow(-1, 1.0 / 7.0);
            roots[2] = cpow(-1, 2.0 / 7.0);
            roots[3] = -cpow(-1, 3.0 / 7.0);
            roots[4] = cpow(-1, 4.0 / 7.0);
            roots[5] = -cpow(-1, 5.0 / 7.0);
            roots[6] = cpow(-1, 6.0 / 7.0);
            break;
        case 8:
            roots[1] = -1;
            roots[2] = -I;
            roots[3] = I;
            roots[4] = -cpow(-1, 0.25);
            roots[5] = cpow(-1, 0.25);
            roots[6] = -cpow(-1, 0.75);
            roots[7] = cpow(-1, 0.75);
            break;
        case 9:
            roots[1] = -cpow(-1, 1.0 / 9.0);
            roots[2] = cpow(-1, 2.0 / 9.0);
            roots[3] = -cpow(-1, 3.0 / 9.0);
            roots[4] = cpow(-1, 4.0 / 9.0);
            roots[5] = -cpow(-1, 5.0 / 9.0);
            roots[6] = cpow(-1, 6.0 / 9.0);
            roots[7] = -cpow(-1, 7.0 / 9.0);
            roots[8] = cpow(-1, 8.0 / 9.0);
            break;
    }
}

void init_results_matrix() {
    results_values = (struct result*) malloc(sizeof(struct result) * picture_size * picture_size);
    results = (struct result**) malloc(sizeof(struct result*) * picture_size);
    for (size_t i = 0, j = 0; i < picture_size; i++, j += picture_size) {
        results[i] = results_values + j;
    }
}

void* worker_thread_main(void* restrict arg) {
    char offset = *((char*) arg);
    struct result row_results[picture_size];
    double step_size = fabs(X_MAX - X_MIN) / picture_size;
    double im = X_MIN + offset * step_size;
    double im_step_size = fabs(X_MAX - X_MIN) / picture_size * num_threads;
    for (long i = offset; i < picture_size; i += num_threads, im += im_step_size) {
        double re = X_MIN;
        for (long j = 0; j < picture_size; j++, re += step_size) {
            // printf("i, j: %d, %d\n", i, j);
            double complex x = re + im * I;
            row_results[j] = newton(x);
        }
        memcpy(results[i], row_results, sizeof(struct result) * picture_size);
    }
    return NULL;
}

struct result newton(double complex x) {
    struct result res;

    int i;
    for (i = 0; ; i++) {
        if (illegal_value(x)) {
            res.root = -1;
            break;
        }

        int root = get_nearby_root(x);
        if (root != -1) {
            res.root = root;
            break;
        }

        x = next_x(x);
    }

    if (i > MAX_ITERATIONS) {
        res.iterations = MAX_ITERATIONS;
    } else {
        res.iterations = i;
    }
    return res;
}

bool illegal_value(double complex x) {
    // TODO: Flip order of checks, most common should be first
    if (cabs(x) < ERROR_MARGIN || fabs(creal(x)) > OUT_OF_BOUNDS || fabs(cimag(x)) > OUT_OF_BOUNDS) {
        return true;
    }
    return false;
}

int get_nearby_root(double complex x) {
    for (char i = 0; i < num_roots; i++) {
        double complex root = roots[i];
        double dist = cabs(x - root); // NOTE: +, -, * and / are overloaded for complex numbers
        if (dist < ERROR_MARGIN) {
            return i;
        }
    }
    return -1;
}

double complex next_x(double complex x) {
    switch (poly_degree) {
        case 1:
            return 1.0;
        case 2:
            return 1.0 / (2.0 * x) + x / 2.0;
        case 3:
            return 1.0 / (3.0 * x*x) + 2.0 * x / 3.0;
        case 4:
            return 1.0 / (4.0 * x*x*x) + 3.0 * x / 4.0;
        case 5:
            return 1.0 / (5.0 * x*x*x*x) + 4.0 * x / 5.0;
        case 6:
            return 1.0 / (6.0 * x*x*x*x*x) + 5.0 * x / 6.0;
        case 7:
            return 1.0 / (7.0 * x*x*x*x*x*x) + 6.0 * x / 7.0;
        case 8:
            return 1.0 / (8.0 * x*x*x*x*x*x*x) + 7.0 * x / 8.0;
        case 9:
            return 1.0 / (9.0 * x*x*x*x*x*x*x*x) + 8.0 * x / 9.0;
        default:
            printf("Invalid poly_degree %d\n", poly_degree);
            exit(1);
    }
}

void write_files() {
    char attractors_filename[25];
    char convergence_filename[26];
    sprintf(attractors_filename, "newton_attractors_x%d.ppm", poly_degree);
    sprintf(convergence_filename, "newton_convergence_x%d.ppm", poly_degree);
    FILE* fp_attractors = fopen(attractors_filename, "w");
    FILE* fp_convergence = fopen(convergence_filename, "w");

    fprintf(fp_attractors, "P3\n%ld %ld\n255\n", picture_size, picture_size);
    fprintf(fp_convergence, "P2\n%ld %ld\n%d\n", picture_size, picture_size, MAX_ITERATIONS);

    // + 1 is to make space for newline character at end of line
    size_t buf_attractors_len = picture_size * COLOR_TRIPLET_LEN + 1;
    size_t buf_convergence_len = picture_size * GRAYSCALE_COLOR_LEN + 1;

    for (size_t i = 0; i < picture_size; i++) {
        char buf_attractors[buf_attractors_len];
        char buf_convergence[buf_convergence_len];

        for (size_t j = 0, offset_attractors = 0, offset_convergence = 0; j < picture_size; j++) {
            struct result result = results[i][j];
            
            char* root_color = attractors_colors[result.root + 1];
            strncpy(buf_attractors + offset_attractors, root_color, COLOR_TRIPLET_LEN);
            offset_attractors += COLOR_TRIPLET_LEN;

            sprintf(buf_convergence + offset_convergence, "%3d ", result.iterations);
            offset_convergence += GRAYSCALE_COLOR_LEN;
        }
        
        buf_attractors[buf_attractors_len - 1] = '\n';
        buf_convergence[buf_convergence_len - 1] = '\n';
        
        fwrite(buf_attractors, sizeof(char), buf_attractors_len, fp_attractors);
        fwrite(buf_convergence, sizeof(char), buf_convergence_len, fp_convergence);
    }

    fclose(fp_attractors);
    fclose(fp_convergence);
}
