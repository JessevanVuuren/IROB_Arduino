#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <Arduino.h>
#include <SPI.h>

#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 64

#define FULL_CIRCLE 360

#define DISPLAY_DIN 23
#define DISPLAY_CLK 18
#define DISPLAY_DC 16
#define DISPLAY_CS 5
#define DISPLAY_RESET 17

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

typedef struct {
    Color color;
    float amplitude;
    float frequency;
    int pos_y;
    float speed;
    float darken_color;
    float sin_lookup[FULL_CIRCLE];
} Background;

typedef struct {
    int pos_x;
    int pos_y;
    float leaf1_shade;
    float leaf2_shade;
    float leaf3_shade;
    int height;
    int width;
    int root_height;
    int root_width;
    int speed;
} Tree;

const Color sky = {138, 245, 255};
const Color sun = {255, 255, 0};
const Color tree_bark = {148, 108, 22};
const Color grass = {42, 250, 0};
const Color water = {0, 255, 255};
const Color mountain = {97, 97, 96};
const Color tree_leaf = {1, 97, 15};

const float sun_range = 5000;

Adafruit_SSD1331 display = Adafruit_SSD1331(DISPLAY_CS, DISPLAY_DC, DISPLAY_DIN, DISPLAY_CLK, DISPLAY_RESET);

uint16_t bitmap[SCREEN_HEIGHT * SCREEN_WIDTH] = {};

int amount_of_layer = 0;
Background layers[] = {
    {.color = mountain, .amplitude = 7, .frequency = 17, .pos_y = 10, .speed = 0, .darken_color = 0.6},
    {.color = mountain, .amplitude = 5, .frequency = 8, .pos_y = 15, .speed = 0.5, .darken_color = .8},
    {.color = mountain, .amplitude = 4, .frequency = 5, .pos_y = 20, .speed = 1, .darken_color = 1},
    {.color = grass, .amplitude = 5, .frequency = 4, .pos_y = 30, .speed = 3, .darken_color = 0.6},
    {.color = grass, .amplitude = 3, .frequency = 3, .pos_y = 32, .speed = 6, .darken_color = 0.8},
    {.color = grass, .amplitude = 3, .frequency = 2, .pos_y = 40, .speed = 16, .darken_color = 1},
    {.color = water, .amplitude = 2, .frequency = 20, .pos_y = 60, .speed = 5, .darken_color = 0.6},
    {.color = water, .amplitude = 2, .frequency = 20, .pos_y = 60, .speed = 10, .darken_color = 1}};

int amount_of_trees = 0;
Tree trees[] = {
    {.pos_x = 10, .pos_y = 25, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 40, .pos_y = 27, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 30, .pos_y = 30, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 60, .pos_y = 33, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 63, .pos_y = 35, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6}};

Color darken_color(Color c, float percentage) {
    c.r = c.r * percentage;
    c.g = c.g * percentage;
    c.b = c.b * percentage;

    return c;
}

uint16_t color_to_hex(Color c) {
    return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | c.b >> 3;
}

Color blend_color(Color color1, Color color2, float blend) {
    if (blend < 0) blend = 0;
    if (blend > 1) blend = 1;

    Color c;
    c.r = (uint8_t)(color1.r * (1 - blend) + color2.r * blend);
    c.g = (uint8_t)(color1.g * (1 - blend) + color2.g * blend);
    c.b = (uint8_t)(color1.b * (1 - blend) + color2.b * blend);
    return c;
}

uint16_t color_to_hex(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | b >> 3;
}

void render_screen() {
    display.drawRGBBitmap(0, 0, bitmap, SCREEN_WIDTH, SCREEN_HEIGHT);
}

double time_till_boot() {
    const double one_sec_to_miliseconds = 1000000;
    return (double)esp_timer_get_time() / one_sec_to_miliseconds;
}

void make_screen(Color color) {
    for (size_t y = 0; y < SCREEN_HEIGHT; y++) {
        for (size_t x = 0; x < SCREEN_WIDTH; x++) {
            bitmap[y * SCREEN_WIDTH + x] = color_to_hex(color);
        }
    }
}

Color set_world_layer(Background layer, int x, int y, Color color, double time) {
    int index = (x + (int)(time * layer.speed)) % (FULL_CIRCLE - 1);

    if (layer.sin_lookup[index] * layer.amplitude + layer.pos_y < y) {
        return darken_color(layer.color, layer.darken_color);
    }

    return color;
}

void build_world_layers(double time) {
    for (size_t y = 0; y < SCREEN_HEIGHT; y++) {
        for (size_t x = 0; x < SCREEN_WIDTH; x++) {
            Color color = sky;

            for (size_t i = 0; i < amount_of_layer; i++) {
                int next_layer = i;

                if (i < amount_of_layer - 1) {
                    next_layer++;
                } else if (layers[i].pos_y + layers[i].amplitude < y) {
                    color = layers[i].color;
                }

                if (layers[i].pos_y - layers[i].amplitude <= y && layers[next_layer].pos_y + layers[next_layer].amplitude >= y) {
                    color = set_world_layer(layers[i], x, y, color, time);
                }
            }

            float sun_blend = (SCREEN_WIDTH - x) * (SCREEN_WIDTH - x) + y * y;
            if (sun_blend < sun_range) {
                float norm = sun_blend / sun_range;
                float invert = 1 - norm;

                float blend_curve = 1 - (invert * invert * invert * invert);
                color = blend_color(sun, color, blend_curve);
            }
            bitmap[y * SCREEN_WIDTH + x] = color_to_hex(color);
        }
    }
}

void make_leaf(Tree tree, int space, float shade) {
    for (int y = 0; y < tree.height; y++) {
        for (int x = tree.width - y + tree.width / 2; x <= tree.width + y - tree.width / 2; x++) {
            Color color = darken_color(tree_leaf, shade);
            int index = (y + tree.pos_y + space) * SCREEN_WIDTH + x + tree.pos_x;
            bitmap[index] = color_to_hex(color);
        }
    }
}

void tree(Tree tree, double time) {
    tree.pos_x = tree.pos_x - (time * tree.speed);
    int space = tree.height / 2;

    for (size_t y = 0; y < tree.root_height; y++) {
        for (size_t x = 0; x < tree.root_width; x++) {
            int adjusted_y = y + tree.pos_y + tree.height + space;
            int adjusted_x = x + (tree.width + tree.root_width) / 2 + tree.pos_x;

            bitmap[adjusted_y * SCREEN_WIDTH + adjusted_x] = color_to_hex(tree_bark);
        }
    }

    make_leaf(tree, -space, tree.leaf1_shade);
    make_leaf(tree, 0, tree.leaf2_shade);
    make_leaf(tree, space, tree.leaf3_shade);
}

void plant_trees(double time) {
    for (size_t i = 0; i < amount_of_trees; i++) {
        tree(trees[i], time);
    }
}

void build_sin_table() {
    for (size_t i = 0; i < amount_of_layer; i++) {
        for (size_t y = 0; y < FULL_CIRCLE; y++) {
            layers[i].sin_lookup[y] = sin(radians(y) * layers[i].frequency);
        }
    }
}

void setup() {
    display.begin();
    Serial.begin(115200);

    make_screen((Color){255, 255, 255});

    amount_of_trees = sizeof(trees) / sizeof(trees[0]);
    amount_of_layer = sizeof(layers) / sizeof(layers[0]);

    build_sin_table();
}

unsigned long timing = 0;

void loop() {
    double time = time_till_boot();

    build_world_layers(time);

    plant_trees(time);


    timing = millis();
    render_screen();
    Serial.print("time it took:");
    Serial.println(millis() - timing);
}