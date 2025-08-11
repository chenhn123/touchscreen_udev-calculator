#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

typedef struct {
    int width, height;
} Screen;

typedef struct {
    char name[32];
    int vendor_id, product_id;
    int x_offset, y_offset;
    int width, height;
    int rotation; // 0, 90, 180, 270
} Touchscreen;

Screen screen = {0};
Touchscreen *touchscreens = NULL;
int ts_count = 0;
int ts_capacity = 0;

// Function prototypes
void print_usage(const char *prog);
int parse_screen(const char *arg);
int parse_touchscreen_args(char *argv[], int index, int rotation);

int main(int argc, char *argv[]) {
    static struct option long_opts[] = {
        {"help",        no_argument,       0, 'h'},
        {"screen",      required_argument, 0, 's'},
        {"touchscreen", no_argument,       0, 't'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "hs:t", long_opts, &long_index)) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 's':
            if (!parse_screen(optarg)) {
                fprintf(stderr, "Invalid screen format or preset: %s\n", optarg);
                return 1;
            }
            break;
        case 't': {
            int num_args = 6;
            if (optind + num_args - 1 >= argc) {
                fprintf(stderr, "Not enough touchscreen arguments\n");
                return 1;
            }

            int rotation = 0;
            if (optind + num_args < argc && isdigit(argv[optind + num_args][0])) {
                rotation = atoi(argv[optind + num_args]);
                if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
                    fprintf(stderr, "Invalid rotation value: %d (must be 0, 90, 180, or 270)\n", rotation);
                    return 1;
                }
                num_args++;
            }

            if (parse_touchscreen_args(argv, optind, rotation) != 0) {
                fprintf(stderr, "Invalid touchscreen parameters\n");
                return 1;
            }
            optind += num_args;
            break;
        }
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // Default screen fallback if none provided
    if (screen.width == 0 || screen.height == 0) {
        fprintf(stderr, "No screen size provided, defaulting to 1920x1080\n");
        screen.width = 1920;
        screen.height = 1080;
    }

    // Output udev rules
    printf("# udev rules for touchscreen calibration\n");
    for (int i = 0; i < ts_count; ++i) {
        Touchscreen *ts = &touchscreens[i];

        float sx = (float)ts->width / screen.width;
        float sy = (float)ts->height / screen.height;
        float ox = (float)ts->x_offset / screen.width;
        float oy = (float)ts->y_offset / screen.height;

        float c0, c1, c2, c3, c4, c5;

        switch (ts->rotation) {
            case 90:
                c0 = 0;     c1 = sy;   c2 = oy;
                c3 = -sx;   c4 = 0;    c5 = 1.0 - ox - sx;
                break;
            case 180:
                c0 = -sx;   c1 = 0;    c2 = 1.0 - ox - sx;
                c3 = 0;     c4 = -sy;  c5 = 1.0 - oy - sy;
                break;
            case 270:
                c0 = 0;     c1 = -sy;  c2 = 1.0 - oy - sy;
                c3 = sx;    c4 = 0;    c5 = ox;
                break;
            case 0:
            default:
                c0 = sx;    c1 = 0;    c2 = ox;
                c3 = 0;     c4 = sy;   c5 = oy;
                break;
        }

        printf(
            "SUBSYSTEM==\"input\", KERNEL==\"event*\", "
            "ATTRS{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", "
            "ENV{LIBINPUT_CALIBRATION_MATRIX}=\"%.6f %.6f %.6f %.6f %.6f %.6f 0 0 1\"\n",
            ts->vendor_id, ts->product_id,
            c0, c1, c2, c3, c4, c5
        );
    }

    free(touchscreens);
    return 0;
}

void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s -s <W>x<H>|1|2 -t <vendor> <product> <x_off> <y_off> <w> <h> [rotation] [...]\n", prog);
    printf("\nOptions:\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -s, --screen WxH      Total screen resolution (e.g., 1920x1080)\n");
    printf("                        Or use preset:\n");
    printf("                        1 = 1920x1080 (default)\n");
    printf("                        2 = 1280x800\n");
    printf("  -t, --touchscreen     Followed by 6 or 7 args:\n");
    printf("                        vendor_id product_id x_offset y_offset width height [rotation]\n");
    printf("                        rotation: optional (0, 90, 180, 270)\n");
    printf("\nExample:\n");
    printf("  %s --screen 1 \\\n"
           "      --touchscreen 0x1234 0xabcd 0 0 960 1080 \\\n"
           "      --touchscreen 0x5678 0xef01 960 0 960 1080 90\n", prog);
}

int parse_screen(const char *arg) {
    if (strcmp(arg, "1") == 0) {
        screen.width = 1920;
        screen.height = 1080;
        return 1;
    } else if (strcmp(arg, "2") == 0) {
        screen.width = 1280;
        screen.height = 800;
        return 1;
    }
    return (sscanf(arg, "%dx%d", &screen.width, &screen.height) == 2);
}

int parse_touchscreen_args(char *argv[], int index, int rotation) {
    if (ts_count >= ts_capacity) {
        ts_capacity = (ts_capacity == 0) ? 2 : ts_capacity * 2;
        Touchscreen *new_arr = realloc(touchscreens, ts_capacity * sizeof(Touchscreen));
        if (!new_arr) {
            perror("realloc");
            return -1;
        }
        touchscreens = new_arr;
    }

    Touchscreen *ts = &touchscreens[ts_count];
    ts->vendor_id  = (int)strtol(argv[index], NULL, 16);
    ts->product_id = (int)strtol(argv[index + 1], NULL, 16);
    ts->x_offset   = atoi(argv[index + 2]);
    ts->y_offset   = atoi(argv[index + 3]);
    ts->width      = atoi(argv[index + 4]);
    ts->height     = atoi(argv[index + 5]);
    ts->rotation   = rotation;
    snprintf(ts->name, sizeof(ts->name), "touchscreen%d", ts_count);

    ts_count++;
    return 0;
}

