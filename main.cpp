// FlowMap 水面流动效果演示 (SDL3 + C++)
// 按 1/2/3 切换显示：1=FlowMap, 2=基础纹理, 3=流动效果
// 按 ESC 退出

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Image {
  uint8_t *pixels; // RGBA, 每个通道 8bit
  int width;
  int height;
};

static void log_error(const char *msg) {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", msg, SDL_GetError());
}

static Image load_image(const char *path) {
  Image img = {};
  int channels = 0;
  img.pixels = stbi_load(path, &img.width, &img.height, &channels, 4);
  if (!img.pixels) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot load image: %s", path);
    SDL_Quit();
    std::exit(1);
  }
  SDL_Log("加载图片 %s: %dx%d", path, img.width, img.height);
  return img;
}

static void free_image(Image &img) {
  if (img.pixels) {
    stbi_image_free(img.pixels);
    img.pixels = nullptr;
  }
}

// 将浮点 UV 限制在 [0,1) 并做 wrap
static void wrap_uv(float &u, float &v) {
  u = std::fmod(u, 1.0f);
  v = std::fmod(v, 1.0f);
  if (u < 0.0f)
    u += 1.0f;
  if (v < 0.0f)
    v += 1.0f;
}

// 双线性采样基础纹理，输出 RGBA 到 out[4]
static void sample_bilinear(const Image &img, float u, float v, uint8_t out[4]) {
  wrap_uv(u, v);
  const float fx = u * (img.width - 1);
  const float fy = v * (img.height - 1);
  int x0 = static_cast<int>(std::floor(fx));
  int y0 = static_cast<int>(std::floor(fy));
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  if (x1 >= img.width)
    x1 = 0;
  if (y1 >= img.height)
    y1 = 0;
  const float dx = fx - x0;
  const float dy = fy - y0;
  const uint8_t *p00 = img.pixels + (y0 * img.width + x0) * 4;
  const uint8_t *p10 = img.pixels + (y0 * img.width + x1) * 4;
  const uint8_t *p01 = img.pixels + (y1 * img.width + x0) * 4;
  const uint8_t *p11 = img.pixels + (y1 * img.width + x1) * 4;
  for (int c = 0; c < 4; ++c) {
    const float v0 = p00[c] * (1.0f - dx) + p10[c] * dx;
    const float v1 = p01[c] * (1.0f - dx) + p11[c] * dx;
    const float val = v0 * (1.0f - dy) + v1 * dy;
    out[c] = static_cast<uint8_t>(SDL_clamp(val, 0.0f, 255.0f));
  }
}

// 最近邻采样 FlowMap，返回归一化到 [-1,1] 的流向量 (outU, outV)
static void sample_flow(const Image &flow, float u, float v, float &outU, float &outV) {
  wrap_uv(u, v);
  const int x = static_cast<int>(u * (flow.width - 1)) % flow.width;
  const int y = static_cast<int>(v * (flow.height - 1)) % flow.height;
  const uint8_t *p = flow.pixels + (y * flow.width + x) * 4;
  outU = (p[0] / 255.0f) * 2.0f - 1.0f; // R -> U
  outV = (p[1] / 255.0f) * 2.0f - 1.0f; // G -> V
}

int main(int argc, char *argv[]) {
  const bool screenshot_mode = std::getenv("SCREENSHOT") != nullptr;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    log_error("SDL_Init 失败");
    return 1;
  }

  const int W = 960, H = 720;
  SDL_Window *window =
      SDL_CreateWindow("FlowMap(Input a key: 1=>FLowMap; 2=>BasicTexture 3=>Flowing effect)", W, H, SDL_WINDOW_RESIZABLE);
  if (!window) {
    log_error("Create window failed.");
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    log_error("Create render failed.");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Image flow = load_image("flowmap.png");
  Image base = load_image("base.png");

  SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, W, H);
  if (!target) {
    log_error("Create texture failed."); /* cleanup */
    return 1;
  }

  bool running = true;
  int display_mode = 2;
  if (const char *m = std::getenv("MODE"))
    display_mode = SDL_clamp(std::atoi(m), 0, 2);

  const float flow_speed = 0.20f;
  const float flow_strength = 0.25f;
  const float tiling_u = 2.0f;
  const float tiling_v = 2.0f;

  uint8_t *shot = nullptr;
  if (screenshot_mode)
    shot = new uint8_t[W * H * 4];
  const Uint64 start = SDL_GetTicks();

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT)
        running = false;
      if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
        case SDLK_1:
          display_mode = 0;
          break;
        case SDLK_2:
          display_mode = 1;
          break;
        case SDLK_3:
          display_mode = 2;
          break;
        case SDLK_ESCAPE:
          running = false;
          break;
        }
      }
    }

    // --- FlowMap 核心算法 ---
    float time_s = SDL_GetTicks() / 1000.0f;
    float frac = std::fmod(time_s * flow_speed, 1.0f);  // 在一个周期(1.0)上运行了多远
    float phase_a = frac;                               // 动画a的时间帧
    float phase_b = std::fmod(frac + 0.5f, 1.0f);       // 动画b的时间帧,与前者相差半个周期
    float blend = std::abs(frac * 2.0f - 1.0f);         // 2个动画的混合比例。

    void *px = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(target, nullptr, &px, &pitch))
      break;

    for (int y = 0; y < H; ++y) {
      uint8_t *row = (uint8_t *)px + y * pitch;
      float v = (float)y / (H - 1);
      for (int x = 0; x < W; ++x) {
        float u = (float)x / (W - 1);
        uint8_t *p = row + x * 4;

        if (display_mode == 0) {
          float fu, fv;
          sample_flow(flow, u, v, fu, fv);
          p[0] = (uint8_t)((fu * 0.5f + 0.5f) * 255);
          p[1] = (uint8_t)((fv * 0.5f + 0.5f) * 255);
          p[2] = 0;
          p[3] = 255;
        } else if (display_mode == 1) {
          uint8_t c[4];
          sample_bilinear(base, u * tiling_u, v * tiling_v, c);
          p[0] = c[0];
          p[1] = c[1];
          p[2] = c[2];
          p[3] = c[3];
        } else {
          float fu;
          float fv;
          sample_flow(flow, u, v, fu, fv); // 获得uv贴图对应坐标的向量
          fu *= flow_strength;  // 放大系数
          fv *= flow_strength;

          float bu = u * tiling_u; // 让坐标可以wrap回滚。
          float bv = v * tiling_v; // 让坐标可以wrap回滚。
          uint8_t ca[4];
          uint8_t cb[4];

          sample_bilinear(base, bu + fu * phase_a, bv + fv * phase_a, ca); // 采样偏移(根据时间放大向量，就会形成动画)
          sample_bilinear(base, bu + fu * phase_b, bv + fv * phase_b, cb); // 动画偏移半个周期。

          //混合动画，前一个动画会淡出，后一个动画会淡入。
          for (int c = 0; c < 4; ++c)
            p[c] = (uint8_t)SDL_clamp(ca[c] * (1 - blend) + cb[c] * blend, 0, 255);
        }
      }
    }

    if (screenshot_mode)
      for (int y = 0; y < H; ++y)
        memcpy(shot + y * W * 4, (uint8_t *)px + y * pitch, W * 4);

    SDL_UnlockTexture(target);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, target, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    if (screenshot_mode && SDL_GetTicks() - start > 1500) {
      SDL_Surface *s =
          SDL_CreateSurfaceFrom(W, H, SDL_PIXELFORMAT_RGBA32, shot, W * 4);
      if (s) {
        SDL_SaveBMP(s, "output.bmp");
        SDL_DestroySurface(s);
      }
      running = false;
    }
  }

  delete[] shot;
  free_image(flow);
  free_image(base);
  SDL_DestroyTexture(target);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
