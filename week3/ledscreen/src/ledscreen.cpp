/*
  This code is for the EPS32 with SSD1331 OLED SCREEN: https://www.waveshare.com/wiki/0.95inch_RGB_OLED_(B)
  This code is written by Jesse van Vuuren
  Date: 11/09/2024

  MIT License
  Copyright (c) 2024 Jesse van Vuuren
*/

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <Arduino.h>
#include <SPI.h>

#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 64

// define full circle so sine wave is seamless
#define FULL_CIRCLE 360

// set communication speed to 115200 baud
#define SERIAL_MONITOR_BAUD_RATE 115200  

// Define all SSD1331 pins
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

// Background layer settings
typedef struct {
    Color color;
    float amplitude;
    float frequency;
    int pos_y;
    float speed;
    float darken_color;
    float sin_lookup[FULL_CIRCLE];
} Background;

// Tree settings
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

// range of sun over the valley
const float sun_range = 5000;

// set all pins for the display and make object
Adafruit_SSD1331 display = Adafruit_SSD1331(DISPLAY_CS, DISPLAY_DC, DISPLAY_DIN, DISPLAY_CLK, DISPLAY_RESET);

// create 2 bitmap's for performance increase when writing to screen
uint16_t old_bitmap[SCREEN_HEIGHT * SCREEN_WIDTH] = {};
uint16_t bitmap[SCREEN_HEIGHT * SCREEN_WIDTH] = {};

// all background levels defined in layers array
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

// all trees defined in trees array
int amount_of_trees = 0;
Tree trees[] = {
    {.pos_x = 10, .pos_y = 25, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 40, .pos_y = 27, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 30, .pos_y = 30, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 60, .pos_y = 33, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6},
    {.pos_x = 63, .pos_y = 35, .leaf1_shade = 1, .leaf2_shade = .6, .leaf3_shade = 1, .height = 10, .width = 5, .root_height = 7, .root_width = 3, .speed = 6}};

// darken a color 1 = no change, 0 = full black
Color darken_color(Color c, float percentage) {
    c.r = c.r * percentage;
    c.g = c.g * percentage;
    c.b = c.b * percentage;

    return c;
}

// convert Color to hex => rgb565
uint16_t color_to_hex(Color c) {
    return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | c.b >> 3;
}

// blend between colors based on percentage 0 = no blend, 1 full blend
Color blend_color(Color color1, Color color2, float percentage) {
    if (percentage < 0) percentage = 0;
    if (percentage > 1) percentage = 1;

    Color c;
    c.r = (uint8_t)(color1.r * (1 - percentage) + color2.r * percentage);
    c.g = (uint8_t)(color1.g * (1 - percentage) + color2.g * percentage);
    c.b = (uint8_t)(color1.b * (1 - percentage) + color2.b * percentage);
    return c;
}

// convert rgb888 to hex => rgb565
uint16_t color_to_hex(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | b >> 3;
}

// draw bitmap on screen ignore unchanged rows
void render_screen(int unchanged_rows) {
    uint16_t *offset_array = bitmap;
    offset_array += SCREEN_WIDTH * unchanged_rows;

    display.drawRGBBitmap(0, unchanged_rows, offset_array, SCREEN_WIDTH, SCREEN_HEIGHT - unchanged_rows);
}

// get time from boot in seconds
double time_from_boot_in_sec() {
    const double one_sec_to_miliseconds = 1000000;
    return (double)esp_timer_get_time() / one_sec_to_miliseconds;
}

// flush screen with one color
void fill_screen_blank_color(Color color) {
    for (size_t y = 0; y < SCREEN_HEIGHT; y++) {
        for (size_t x = 0; x < SCREEN_WIDTH; x++) {
            bitmap[y * SCREEN_WIDTH + x] = color_to_hex(color);
        }
    }
}

// this function takes the sin value from the lookup table and compares it with the y value of the screen
// if true the function returns the layer color value with shade
// if false the function returns the input color
Color set_world_layer(Background layer, int x, int y, Color color, double time) {
    int index = (x + (int)(time * layer.speed)) % (FULL_CIRCLE - 1);

    if (layer.sin_lookup[index] * layer.amplitude + layer.pos_y < y) {
        return darken_color(layer.color, layer.darken_color);
    }

    return color;
}

// loops over every background layer and draws the color on the bitmap
// it also calculates the sun value over very pixel
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

// draw a triangle that looks like a leaf. from the tree object
void make_leaf(Tree tree, int space, float shade) {
    for (int y = 0; y < tree.height; y++) {
        for (int x = tree.width - y + tree.width / 2; x <= tree.width + y - tree.width / 2; x++) {
            Color color = darken_color(tree_leaf, shade);

            int adjusted_y = y + tree.pos_y + space;
            int adjusted_x = x + tree.pos_x;

            bitmap[adjusted_y * SCREEN_WIDTH + adjusted_x % SCREEN_WIDTH] = color_to_hex(color);
        }
    }
}

// plant tree in valley
void tree(Tree tree, double time) {
    tree.pos_x = tree.pos_x - (time * tree.speed);
    int space = tree.height / 2;

    for (size_t y = 0; y < tree.root_height; y++) {
        for (size_t x = 0; x < tree.root_width; x++) {
            int adjusted_y = y + tree.pos_y + tree.height + space;
            int adjusted_x = x + (tree.width + tree.root_width) / 2 + tree.pos_x;

            bitmap[adjusted_y * SCREEN_WIDTH + adjusted_x % SCREEN_WIDTH] = color_to_hex(tree_bark);
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

// this function builds the sine wave tables for every background layer before hand.
// sin is quite a heavy calculation witch slows the animation down
// making a lookup table beforehand increases performance drastically
void build_sin_table() {
    for (size_t i = 0; i < amount_of_layer; i++) {
        for (size_t y = 0; y < FULL_CIRCLE; y++) {
            layers[i].sin_lookup[y] = sin(radians(y) * layers[i].frequency);
        }
    }
}

// compares two frames (old and new bitmap) and returns the number of unchanged rows
// unchanged rows are skipped during rendering to optimize render time
int compare_bitmap_y_axis() {
    for (size_t i = 0; i < SCREEN_HEIGHT; i++) {
        bool same = true;

        for (size_t x = 0; x < SCREEN_WIDTH; x++) {
            if (bitmap[i * SCREEN_WIDTH + x] != old_bitmap[i * SCREEN_WIDTH + x]) {
                same = false;
                break;
            }
        }

        if (!same) return i;
    }
    return SCREEN_HEIGHT;
}

void setup() {
    display.begin();
    Serial.begin(SERIAL_MONITOR_BAUD_RATE);

    // clear screen
    fill_screen_blank_color((Color){255, 255, 255});

    // calculate int length of arrays;
    amount_of_trees = sizeof(trees) / sizeof(trees[0]);
    amount_of_layer = sizeof(layers) / sizeof(layers[0]);

    // build lookup table for the sin function
    build_sin_table();

    Serial.println("Starting main render loop");
}



void loop() {
    // get timing and fps so is displays the performance and fps
    unsigned long timing = millis();
    unsigned long fps = millis();

    // get time in seconds from boot to make animation play
    double time = time_from_boot_in_sec();

    Serial.print("Build world in: ");

    // draw world layers in order on screen
    build_world_layers(time);

    // draw all trees on screen
    plant_trees(time);

    Serial.print(millis() - timing);
    Serial.print(" millis, Render in: ");

    // reset timing for rendering to screen
    timing = millis();

    // get the count of matching rows in the bitmap so it can ignore un-updated pixels
    int rows_alike = compare_bitmap_y_axis();
    
    // render bitmap to screen
    render_screen(rows_alike);

    Serial.print(millis() - timing);
    Serial.print(" millis, fps: ");
    Serial.println(1000.0 / (float)(millis() - fps));

    // copy new bitmap to old bitmap
    memcpy(old_bitmap, bitmap, sizeof(old_bitmap));
}