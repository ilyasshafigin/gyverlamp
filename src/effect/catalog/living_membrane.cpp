#include "living_membrane.h"

#include "../shared.h"

// Effect: Living Membrane - ### Живая мембрана
// Крупные световые острова растут, делятся, соединяются и вытесняют друг друга. Иногда пятно вытягивается в перемычку, которая затем разрывается.
// Authors: ilyasshafigin + Codex

namespace {
namespace LivingMembraneTracking {
  constexpr uint8_t kCore = 128;
  constexpr uint8_t kCount = 8;
  constexpr uint16_t kPixels = WIDTH * HEIGHT;
  constexpr float kTau = 6.28318530718F;

  auto& w = livingMembraneWorkspace;

  float& phase(uint8_t i) {
    return trackingObjectPosX[i];
  }
  float& target(uint8_t i) {
    return trackingObjectPosY[i];
  }
  float& displayed(uint8_t i) {
    return trackingObjectSpeedX[i];
  }
  float& common(uint8_t i) {
    return trackingObjectSpeedY[i];
  }
  float& initial(uint8_t i) {
    return trackingObjectShift[i];
  }
  bool& active(uint8_t i) {
    return trackingObjectIsShift[i];
  }
  uint8_t& state(uint8_t i) {
    return trackingObjectState[i];
  }
  uint8_t& leader(uint8_t i) {
    return trackingObjectHue[i];
  }
  uint8_t owner(uint8_t v) {
    return v & 15;
  }
  uint8_t concentration(uint16_t p) {
    return noise3d[1][p % WIDTH][p / WIDTH];
  }
  uint16_t age(uint16_t a, uint16_t dt) {
    return min(static_cast<uint32_t>(a) + dt, static_cast<uint32_t>(65535));
  }
  float start(float offset, uint32_t clock) {
    return 215.0F * (0.5F + 0.5F * sinf(kTau * (clock / 240000.0F + offset)));
  }
  float nearest(float value, float reference) {
    return value + roundf(reference - value);
  }
  float inverse(float value, float reference, uint32_t clock) {
    const float a = asinf(constrain(value / 107.5F - 1.0F, -1.0F, 1.0F)) / kTau;
    const float time = clock / 240000.0F;
    const float first = nearest(a - time, reference);
    const float second = nearest(0.5F - a - time, reference);
    return fabsf(first - reference) <= fabsf(second - reference) ? first : second;
  }
  uint32_t hash(uint16_t serial) {
    uint32_t h = static_cast<uint32_t>(serial) + 0x9e3779b9U;
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    return h ^ (h >> 16);
  }
  // Four-connected cylinder. Invalid vertical neighbours are the pixel itself.
  uint16_t neighbour(uint16_t p, uint8_t direction) {
    const uint16_t x = p % WIDTH, y = p / WIDTH;
    switch (direction) {
      case 0: return y * WIDTH + (x == 0 ? WIDTH - 1 : x - 1);
      case 1: return y * WIDTH + (x + 1 == WIDTH ? 0 : x + 1);
      case 2: return y == 0 ? p : p - WIDTH;
      default: return y + 1 == HEIGHT ? p : p + WIDTH;
    }
  }
  void release(uint8_t i, uint8_t* previous) {
    active(i) = false;
    state(i) = 0;
    w.mergeGroup[i] = 0;
    for (uint16_t p = 0; p < kPixels; p++) {
      if (owner(previous[p]) == i + 1) previous[p] = 0;
    }
  }
  uint8_t create(uint8_t parent, uint32_t clock) {
    for (uint8_t i = 0; i < kCount; i++) {
      if (active(i)) continue;
      active(i) = true;
      const uint32_t h = hash(++w.birthSerial);
      phase(i) = parent ? inverse(displayed(parent - 1), phase(parent - 1), clock) : (h & 65535U) / 65536.0F;
      target(i) = phase(i);
      displayed(i) = parent ? displayed(parent - 1) : start(phase(i), clock);
      initial(i) = common(i) = phase(i);
      state(i) = parent ? 2 : 0; // bit 1: split awaiting independent confirmation
      leader(i) = i;
      w.birthAge[i] = w.absenceAge[i] = w.mergeAge[i] = 0;
      w.mergeGroup[i] = 0;
      return i + 1;
    }
    return 0;
  }
  void cancel(uint8_t i, uint32_t clock) {
    // A debounce candidate has not altered color: keep its independent fork
    // target verbatim. Only a started blend needs a displayed-color reanchor.
    if (w.mergeGroup[i] && w.mergeAge[i] >= 1500) {
      const float remaining = nearest(target(i), phase(i)) - phase(i);
      phase(i) = inverse(displayed(i), phase(i), clock);
      target(i) = phase(i) + remaining;
    }
    initial(i) = common(i) = phase(i);
    w.mergeAge[i] = 0;
    w.mergeGroup[i] = 0;
    leader(i) = i;
  }
  void setup() {
    memset(&w, 0, sizeof(w));
    for (uint8_t i = 0; i < kCount; i++) {
      phase(i) = target(i) = displayed(i) = common(i) = initial(i) = 0;
      active(i) = false;
      state(i) = 0;
      leader(i) = i;
    }
  }

  void update(uint16_t elapsed, uint32_t clock) {
    uint8_t* previous = w.maps[w.mapIndex];
    uint8_t* current = w.maps[w.mapIndex ^ 1];
    uint16_t histogram[8] = {};
    uint8_t masks[8] = {};
    memset(current, 0, kPixels);
    memset(w.overlap, 0, sizeof(w.overlap));
    memset(w.componentArea, 0, sizeof(w.componentArea));
    memset(w.trackArea, 0, sizeof(w.trackArea));
    memset(w.primary, 0, sizeof(w.primary));
    memset(w.bestOldComponent, 0, sizeof(w.bestOldComponent));
    w.componentCount = 0;
    for (uint8_t i = 0; i < kCount; i++) {
      if (!active(i)) continue;
      // Evaluate the prior merge at the new clock before possible cancellation.
      if (w.mergeGroup[i] && w.mergeAge[i] >= 1500) {
        const float t = constrain((w.mergeAge[i] - 1500 + elapsed) / 8000.0F, 0.0F, 1.0F);
        const float blend = t * t * (3.0F - 2.0F * t);
        displayed(i) = start(initial(i), clock) * (1.0F - blend) + start(common(i), clock) * blend;
      } else {
        if (!w.mergeGroup[i] && (state(i) & 1)) {
          const float limit = elapsed / 720000.0F; // 1/12 turn in approximately 60 seconds
          phase(i) += constrain(target(i) - phase(i), -limit, limit);
        }
        displayed(i) = start(phase(i), clock);
      }
    }
    // Stable old cores get the bounded component slots first. Fringe never seeds core.
    for (uint8_t pass = 0; pass < 2; pass++) {
      for (uint16_t p = 0; p < kPixels; p++) {
        const bool eligible = concentration(p) >= 46 || ((previous[p] & kCore) && concentration(p) >= 40);
        const uint8_t oldSeed = owner(previous[p]);
        if (current[p] || !eligible || (pass == 0 && (!(previous[p] & kCore) || !oldSeed || !active(oldSeed - 1))))
          continue;
        const uint8_t c = w.componentCount < kCount ? ++w.componentCount : 9;
        uint16_t head = 0, tail = 0;
        current[p] = kCore | c;
        w.queue[tail++] = p;
        while (head < tail) {
          const uint16_t q = w.queue[head++];
          for (uint8_t d = 0; d < 4; d++) {
            const uint16_t n = neighbour(q, d);
            if (current[n]) continue;
            if (concentration(n) < 46 && (!(previous[n] & kCore) || concentration(n) < 40)) continue;
            current[n] = kCore | c; // mark before enqueue; tail can reach 256
            w.queue[tail++] = n;
          }
        }
        if (c <= kCount) {
          w.componentArea[c - 1] = tail;
          for (uint16_t q = 0; q < tail; q++) {
            const uint8_t old = owner(previous[w.queue[q]]);
            if (old && active(old - 1)) w.overlap[c - 1][old - 1]++;
          }
          bool hasOld = false;
          for (uint8_t i = 0; i < kCount; i++)
            if (w.overlap[c - 1][i]) hasOld = true;
          if (tail < 3 && !hasOld) {
            // Unrelated sub-birth specks must not consume scarce component slots.
            for (uint16_t q = 0; q < tail; q++)
              current[w.queue[q]] = kCore | 64;
            w.componentArea[c - 1] = 0;
            w.componentCount--;
          }
        } else {
          // Overflow splits retain the dominant old identity; unrelated excess uses
          // the slow shared fallback. Bit 6 keeps this temporary label distinct.
          memset(histogram, 0, sizeof(histogram));
          for (uint16_t q = 0; q < tail; q++) {
            const uint8_t old = owner(previous[w.queue[q]]);
            if (old && active(old - 1)) histogram[old - 1]++;
          }
          uint8_t best = 0;
          for (uint8_t i = 0; i < kCount; i++)
            if (histogram[i] > (best ? histogram[best - 1] : 0)) best = i + 1;
          for (uint16_t q = 0; q < tail; q++)
            current[w.queue[q]] = kCore | 64 | best;
        }
      }
    }
    // Global greedy positive overlap matching; iteration order provides stable ties.
    uint8_t used = 0;
    for (uint8_t n = 0; n < kCount; n++) {
      uint16_t best = 0;
      uint8_t row = 0, id = 0;
      for (uint8_t c = 0; c < w.componentCount; c++) {
        if (w.primary[c]) continue;
        for (uint8_t i = 0; i < kCount; i++) {
          if (!(used & (1 << i)) && (w.overlap[c][i] > best || (best && w.overlap[c][i] == best && i < id))) {
            best = w.overlap[c][i];
            row = c;
            id = i;
          }
        }
      }
      if (!best) break;
      w.primary[row] = id + 1;
      used |= 1 << id;
    }
    for (uint8_t i = 0; i < kCount; i++) {
      for (uint8_t c = 0; c < w.componentCount; c++) {
        const uint8_t old = w.bestOldComponent[i];
        if (w.overlap[c][i] > (old ? w.overlap[old - 1][i] : 0)) w.bestOldComponent[i] = c + 1;
      }
    }
    for (uint8_t c = 0; c < w.componentCount; c++) {
      if (!w.primary[c]) {
        uint8_t parent = 0;
        for (uint8_t i = 0; i < kCount; i++)
          if (w.overlap[c][i] > (parent ? w.overlap[c][parent - 1] : 0)) parent = i + 1;
        if (w.componentArea[c] >= 3) w.primary[c] = create(parent, clock);
        if (!w.primary[c]) w.primary[c] = parent;
      }
      if (w.primary[c]) masks[c] = 1 << (w.primary[c] - 1);
      for (uint8_t i = 0; i < kCount; i++) {
        if (active(i) && !(used & (1 << i)) && w.bestOldComponent[i] == c + 1) masks[c] |= 1 << i;
      }
    }
    // Seed ALL old overlap pixels before expansion, preserving owner domains inside
    // a connected merge. Queue is reused independently for each component.
    for (uint8_t c = 0; c < w.componentCount; c++) {
      uint16_t head = 0, tail = 0;
      for (uint16_t p = 0; p < kPixels; p++) {
        if (current[p] != (kCore | (c + 1))) continue;
        const uint8_t old = owner(previous[p]);
        if (old && (masks[c] & (1 << (old - 1)))) {
          current[p] = 32 | kCore | old;
          w.queue[tail++] = p;
        }
      }
      if (!tail) {
        for (uint16_t p = 0; p < kPixels; p++) {
          if (current[p] == (kCore | (c + 1))) {
            current[p] = 32 | kCore | w.primary[c];
            w.queue[tail++] = p;
            break;
          }
        }
      }
      while (head < tail) {
        const uint16_t p = w.queue[head++];
        for (uint8_t d = 0; d < 4; d++) {
          const uint16_t n = neighbour(p, d);
          if (current[n] == (kCore | (c + 1))) {
            current[n] = current[p];
            w.queue[tail++] = n;
          }
        }
      }
      // Count distinct domain contact pixels, not edges: one junction pixel
      // touching several neighbours is still insufficient merge evidence.
      memset(histogram, 0, sizeof(histogram));
      for (uint16_t q = 0; q < tail; q++) {
        const uint16_t p = w.queue[q];
        const uint8_t a = owner(current[p]);
        if (!a) continue;
        w.trackArea[a - 1]++;
        bool contact = false;
        for (uint8_t d = 0; d < 4; d++) {
          const uint8_t b = owner(current[neighbour(p, d)]);
          if ((current[neighbour(p, d)] & 32) && b && b != a) contact = true;
        }
        if (contact) histogram[a - 1]++;
      }
      bool adequate = true;
      for (uint8_t i = 0; i < kCount; i++)
        if ((masks[c] & (1 << i)) && histogram[i] < 2) adequate = false;
      // A singleton has no contact requirement.
      if (!(masks[c] & (masks[c] - 1))) adequate = true;
      if (!adequate) masks[c] = 0;
    }
    for (uint16_t p = 0; p < kPixels; p++) {
      if (current[p] & 64) {
        const uint8_t id = owner(current[p]);
        if (id && active(id - 1)) w.trackArea[id - 1]++;
      }
      current[p] &= ~32;
    }
    // Resolve each group's composition atomically before assigning its shared clock.
    for (uint8_t c = 0; c < w.componentCount; c++) {
      const uint8_t group = masks[c];
      if (!group || !(group & (group - 1))) continue;
      const uint8_t primary = w.primary[c] - 1;
      bool changed = false;
      for (uint8_t i = 0; i < kCount; i++)
        if ((group & (1 << i)) && (w.mergeGroup[i] != group || leader(i) != primary)) changed = true;
      if (changed) {
        for (uint8_t i = 0; i < kCount; i++)
          if (group & (1 << i)) {
            cancel(i, clock);
            w.mergeGroup[i] = group;
            leader(i) = primary;
          }
      } else {
        for (uint8_t i = 0; i < kCount; i++)
          if (group & (1 << i)) w.mergeAge[i] = age(w.mergeAge[i], elapsed);
      }
      if (w.mergeAge[primary] >= 1500) {
        if (w.mergeAge[primary] - elapsed < 1500) {
          float mean = 0;
          uint16_t area = 0;
          for (uint8_t i = 0; i < kCount; i++)
            if (group & (1 << i)) {
              mean += displayed(i) * w.trackArea[i];
              area += w.trackArea[i];
            }
          const float offset = inverse(area ? mean / area : displayed(primary), phase(primary), clock);
          for (uint8_t i = 0; i < kCount; i++)
            if (group & (1 << i)) {
              initial(i) = phase(i);
              common(i) = offset;
            }
        }
        const float t = constrain((w.mergeAge[primary] - 1500) / 8000.0F, 0.0F, 1.0F);
        const float blend = t * t * (3.0F - 2.0F * t);
        for (uint8_t i = 0; i < kCount; i++)
          if (group & (1 << i))
            displayed(i) = start(initial(i), clock) * (1.0F - blend) + start(common(i), clock) * blend;
        if (t == 1.0F) {
          phase(primary) = target(primary) = common(primary);
          state(primary) = 1;
          // Retained detached fringe is copied later from the previous map.
          // Remap both buffers before freeing IDs; ordinary expiry remains the
          // destructive release path, while reconciliation transfers ownership.
          for (uint8_t map = 0; map < 2; map++)
            for (uint16_t p = 0; p < kPixels; p++) {
              const uint8_t id = owner(w.maps[map][p]);
              if (id && (group & (1 << (id - 1)))) w.maps[map][p] = (w.maps[map][p] & kCore) | (primary + 1);
            }
          for (uint8_t i = 0; i < kCount; i++)
            if ((group & (1 << i)) && i != primary) release(i, previous);
          w.mergeGroup[primary] = 0;
          w.mergeAge[primary] = 0;
          masks[c] = 1 << primary;
        }
      }
    }
    for (uint8_t i = 0; i < kCount; i++) {
      if (!active(i)) continue;
      bool merging = false;
      for (uint8_t c = 0; c < w.componentCount; c++)
        if ((masks[c] & (1 << i)) && (masks[c] & (masks[c] - 1))) merging = true;
      if (!merging && w.mergeGroup[i]) cancel(i, clock);
      if (w.trackArea[i]) {
        w.absenceAge[i] = 0;
        if (!merging) w.birthAge[i] = age(w.birthAge[i], elapsed);
        if (!(state(i) & 1) && w.birthAge[i] >= 300) {
          if (state(i) & 2) {
            const uint32_t h = hash(++w.birthSerial);
            target(i) = phase(i) + ((h & 1) ? 1.0F : -1.0F) * (0.075F + ((h >> 1) & 255) / 15300.0F);
          }
          state(i) = 1;
        }
      } else {
        w.absenceAge[i] = age(w.absenceAge[i], elapsed);
        if (w.absenceAge[i] >= 1000) release(i, previous);
      }
    }
    // Overflow owners are real cores too; clear their temporary marker.
    for (uint16_t p = 0; p < kPixels; p++)
      current[p] &= ~64;
    // Visible fringe grows from current core ownership only. No-core old fringe
    // survives during grace, but can never become a segmentation seed.
    uint16_t head = 0, tail = 0;
    for (uint16_t p = 0; p < kPixels; p++)
      if ((current[p] & kCore) && owner(current[p])) w.queue[tail++] = p;
    while (head < tail) {
      const uint16_t p = w.queue[head++];
      for (uint8_t d = 0; d < 4; d++) {
        const uint16_t n = neighbour(p, d);
        if (!current[n] && concentration(n) > 28) {
          current[n] = owner(current[p]);
          w.queue[tail++] = n;
        }
      }
    }
    for (uint16_t p = 0; p < kPixels; p++) {
      const uint8_t old = owner(previous[p]);
      if (!current[p] && concentration(p) > 28 && old && active(old - 1)) current[p] = old;
    }
    w.mapIndex ^= 1;
  }

  float pixelStart(uint8_t x, uint8_t y, uint32_t clock) {
    const uint8_t* map = w.maps[w.mapIndex];
    const uint16_t p = y * WIDTH + x;
    const uint8_t id = owner(map[p]);
    const float fallback = start(0, clock);
    const float value = id && active(id - 1) ? displayed(id - 1) : fallback;
    if (map[p] & kCore) return value;
    // Only fringe boundaries receive the existing separable scalar smoothing.
    float sum = 0;
    uint8_t weight = 0;
    for (int8_t dy = -1; dy <= 1; dy++) {
      const uint8_t sy = constrain(static_cast<int16_t>(y) + dy, 0, HEIGHT - 1);
      for (int8_t dx = -1; dx <= 1; dx++) {
        const uint8_t sx = (x + WIDTH + dx) % WIDTH;
        const uint8_t other = owner(map[sy * WIDTH + sx]);
        const uint8_t k = (dx == 0 ? 2 : 1) * (dy == 0 ? 2 : 1);
        sum += k * (other && active(other - 1) ? displayed(other - 1) : fallback);
        weight += k;
      }
    }
    return sum / weight;
  }
} // namespace LivingMembraneTracking
} // namespace

uint32_t EffectLivingMembrane::randomBits() {
  randomState_ ^= randomState_ << 13;
  randomState_ ^= randomState_ >> 17;
  randomState_ ^= randomState_ << 5;
  return randomState_;
}

uint8_t EffectLivingMembrane::quantize(int32_t value) {
  value = constrain(value, 0, 255 * 4096);
  // Unbiased sub-byte integration; this randomness is never displayed directly.
  return static_cast<uint8_t>((value + (randomBits() & 4095U)) / 4096);
}

void EffectLivingMembrane::setup(EffectContext& ctx) {
  accumulated_ = 0;
  paletteClockMs_ = 0;
  brightnessGain_ = 1.0F;
  randomState_ = (static_cast<uint32_t>(random16()) << 16) | random16();
  if (randomState_ == 0) randomState_ = 1;
  // Rotate only the displayed cylinder. Preserve the canonical field and its
  // exact stochastic integration sequence, including concentration seed draws.
  uint32_t placement = randomState_;
  placement ^= placement << 13;
  placement ^= placement >> 17;
  placement ^= placement << 5;
  displayOffsetX_ = placement % WIDTH;
  LivingMembraneTracking::setup();
  // Keep the byte-quantized diffusion inside the tested non-extinguishing range.
  const float domain = 1.0F + static_cast<float>(ctx.scale) / 2040.0F;
  for (uint8_t x = 0; x < WIDTH; x++) {
    for (uint8_t y = 0; y < HEIGHT; y++) {
      noise3d[0][x][y] = 255;
      noise3d[1][x][y] = 0;
      for (uint8_t island = 0; island < 2; island++) {
        const float centerX = (island == 0 ? 4.0F : 12.0F) * WIDTH / 16.0F;
        const float centerY = (island == 0 ? 5.0F : 11.0F) * HEIGHT / 16.0F;
        const float direct = fabsf(x - centerX);
        const float dx = min(direct, static_cast<float>(WIDTH) - direct) * 16.0F / WIDTH;
        const float dy = (y - centerY) * 16.0F / HEIGHT;
        if (dx * dx + dy * dy <= (island == 0 ? 6.0F : 5.0F) * domain) {
          noise3d[0][x][y] = 128;
          noise3d[1][x][y] = 64 + randomBits() % 6;
        }
      }
    }
  }
}

void EffectLivingMembrane::advance(uint16_t diffusionU, uint16_t diffusionV) {
  uint8_t previous[2][WIDTH];
  uint8_t next[2][WIDTH];
  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      const uint8_t left = x == 0 ? WIDTH - 1 : x - 1;
      const uint8_t right = x + 1 == WIDTH ? 0 : x + 1;
      const uint8_t below = y + 1 == HEIGHT ? y : y + 1;
      const uint32_t u = noise3d[0][x][y];
      const uint32_t v = noise3d[1][x][y];
      const int32_t reaction = 16 * ((u * v * v * 256U) / 65025U);
      for (uint8_t layer = 0; layer < 2; layer++) {
        // Only the preceding row was overwritten; all other reads are old state.
        const uint8_t* above = y == 0 ? nullptr : previous[layer];
        const int32_t center = noise3d[layer][x][y];
        const int32_t cardinal =
          noise3d[layer][left][y] + noise3d[layer][right][y] + (above ? above[x] : center) + noise3d[layer][x][below];
        const int32_t diagonal = (above ? above[left] : noise3d[layer][left][y]) +
                                 (above ? above[right] : noise3d[layer][right][y]) + noise3d[layer][left][below] +
                                 noise3d[layer][right][below];
        const int32_t laplacian = 4 * cardinal + diagonal - 20 * center;
        const int32_t value = layer == 0 ? 4096 * center + diffusionU * laplacian - reaction + 78 * (255 - center)
                                         : 4096 * center + diffusionV * laplacian + reaction - 287 * center;
        next[layer][x] = quantize(value);
      }
    }
    for (uint8_t layer = 0; layer < 2; layer++) {
      for (uint8_t x = 0; x < WIDTH; x++) {
        previous[layer][x] = noise3d[layer][x][y];
        noise3d[layer][x][y] = next[layer][x];
      }
    }
  }
}

void EffectLivingMembrane::render(EffectContext& ctx) {
  const uint16_t rate = 24 + static_cast<uint16_t>(ctx.speed) * 96 / 255;
  const float domain = 1.0F + static_cast<float>(ctx.scale) / 2040.0F;
  const uint16_t diffusionU = static_cast<uint16_t>(4096.0F * 0.16F * domain / 20.0F + 0.5F);
  const uint16_t diffusionV = static_cast<uint16_t>(4096.0F * 0.08F * domain / 20.0F + 0.5F);
  const uint32_t elapsed = ctx.deltaMs > 100 ? 100 : ctx.deltaMs;
  // Keep palette motion independent of simulation RNG and safely bounded.
  constexpr uint32_t kPalettePeriodMs = 240000;
  paletteClockMs_ = (paletteClockMs_ + elapsed) % kPalettePeriodMs;
  accumulated_ += elapsed * rate;
  for (uint8_t steps = 0; accumulated_ >= 1000 && steps < 12; steps++) {
    accumulated_ -= 1000;
    advance(diffusionU, diffusionV);
  }

  LivingMembraneTracking::update(elapsed, paletteClockMs_);

  float maximumMask = 0.0F;
  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      float mask = constrain((noise3d[1][x][y] - 28.0F) / 62.0F, 0.0F, 1.0F);
      mask = mask * mask * (3.0F - 2.0F * mask);
      if (mask > maximumMask) maximumMask = mask;
    }
  }

  constexpr float kMaximumBrightnessGain = 4.0F;
  constexpr float kGainAttackMs = 160.0F;
  constexpr float kGainReleaseMs = 650.0F;
  const float targetGain = maximumMask > 0.0F ? constrain(1.0F / maximumMask, 1.0F, kMaximumBrightnessGain) : 1.0F;
  const float smoothingMs = targetGain > brightnessGain_ ? kGainAttackMs : kGainReleaseMs;
  const float blend = constrain(static_cast<float>(elapsed) / smoothingMs, 0.0F, 1.0F);
  brightnessGain_ += (targetGain - brightnessGain_) * blend;

  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      float mask = constrain((noise3d[1][x][y] - 28.0F) / 62.0F, 0.0F, 1.0F);
      mask = mask * mask * (3.0F - 2.0F * mask);
      const float correctedMask = constrain(mask * brightnessGain_, 0.0F, 1.0F);
      const uint8_t intensity = static_cast<uint8_t>(3.0F + 220.0F * correctedMask);
      CRGB pixel;
      if (ctx.palette) {
        // Palette color uses the smoothed field; brightness uses the structural mask and global gain.
        const uint8_t left = x == 0 ? WIDTH - 1 : x - 1;
        const uint8_t right = x + 1 == WIDTH ? 0 : x + 1;
        const uint8_t above = y == 0 ? y : y - 1;
        const uint8_t below = y + 1 == HEIGHT ? y : y + 1;
        const float smoothedV = (noise3d[1][left][above] + 2.0F * noise3d[1][x][above] + noise3d[1][right][above] +
                                 2.0F * noise3d[1][left][y] + 4.0F * noise3d[1][x][y] + 2.0F * noise3d[1][right][y] +
                                 noise3d[1][left][below] + 2.0F * noise3d[1][x][below] + noise3d[1][right][below]) /
                                16.0F;
        float colorMask = constrain((smoothedV - 28.0F) / 62.0F, 0.0F, 1.0F);
        colorMask = colorMask * colorMask * (3.0F - 2.0F * colorMask);

        constexpr float kPaletteWindow = 40.0F;
        const float start = LivingMembraneTracking::pixelStart(x, y, paletteClockMs_);
        const uint8_t index = static_cast<uint8_t>(start + colorMask * (kPaletteWindow - 1.0F) + 0.5F);
        pixel = ColorFromPalette(*ctx.palette, index, 255, LINEARBLEND);
        const uint8_t maximumChannel = max(pixel.r, max(pixel.g, pixel.b));
        if (maximumChannel > 16) {
          float transition = constrain((maximumChannel - 16.0F) / 32.0F, 0.0F, 1.0F);
          transition = transition * transition * (3.0F - 2.0F * transition);
          const float targetPeak = maximumChannel + (255.0F - maximumChannel) * transition;
          const float normalization = targetPeak / maximumChannel;
          pixel.r = static_cast<uint8_t>(pixel.r * normalization + 0.5F);
          pixel.g = static_cast<uint8_t>(pixel.g * normalization + 0.5F);
          pixel.b = static_cast<uint8_t>(pixel.b * normalization + 0.5F);
        }
      } else {
        pixel = CHSV(static_cast<uint8_t>(155.0F - 55.0F * mask), 230, 255);
      }
      pixel.nscale8(intensity);
      ctx.led.drawPixel((static_cast<uint16_t>(x) + displayOffsetX_) % WIDTH, y, pixel);
    }
  }
}
