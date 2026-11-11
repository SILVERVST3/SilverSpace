#pragma once
// SilverSpace - motor de DSP (apenas C++ padrao, sem JUCE).
// Ideia: analisa o espectro do sidechain em 32 bandas e aplica a curva
// INVERSA (reducao) na faixa principal, via STFT (FFT 2048, sobreposicao 4x).
// Latencia fixa = kFFTSize amostras.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace silverspace
{
constexpr int   kNumBands = 32;
constexpr int   kFFTSize  = 2048;
constexpr int   kOverlap  = 4;
constexpr int   kHop      = kFFTSize / kOverlap;
constexpr float kMinFreq  = 20.0f;
constexpr float kMaxFreq  = 20000.0f;
constexpr float kMaxCutDb = 30.0f; // reducao maxima com Amount = 100 %

struct Params
{
    float amount        = 0.4f;   // 0..1
    float lowCutHz      = 20.0f;
    float highCutHz     = 20000.0f;
    bool  freeze        = false;
    bool  midSide       = false;  // false = L/R, true = M/S
    float pan           = 0.0f;   // -1..1
    float attackMs      = 20.0f;
    float releaseMs     = 200.0f;
    bool  scAudition    = false;
    float sensitivityDb = -70.0f; // limiar do gate de nivel do sidechain
    float rangeDb       = 30.0f;  // quantos dB abaixo do pico ainda sofrem reducao
    float mix           = 1.0f;   // 0..1
    float outputDb      = 0.0f;
};

// ---------------------------------------------------------------- FFT radix-2
class FFT
{
public:
    FFT()
    {
        int bits = 0;
        while ((1 << bits) < kFFTSize) ++bits;
        for (int i = 0; i < kFFTSize; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        for (int k = 0; k < kFFTSize / 2; ++k)
        {
            const double a = 2.0 * 3.14159265358979323846 * (double) k / (double) kFFTSize;
            cosT[(size_t) k] = (float) std::cos(a);
            sinT[(size_t) k] = (float) std::sin(a);
        }
    }

    void forward(float* re, float* im) const { transform(re, im, -1.0f); }

    void inverse(float* re, float* im) const
    {
        transform(re, im, 1.0f);
        const float s = 1.0f / (float) kFFTSize;
        for (int i = 0; i < kFFTSize; ++i) { re[i] *= s; im[i] *= s; }
    }

private:
    void transform(float* re, float* im, float sign) const
    {
        for (int i = 0; i < kFFTSize; ++i)
        {
            const int j = rev[(size_t) i];
            if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
        }
        for (int len = 2; len <= kFFTSize; len <<= 1)
        {
            const int half = len >> 1, step = kFFTSize / len;
            for (int i = 0; i < kFFTSize; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)];
                    const float wi = sign * sinT[(size_t) (j * step)];
                    const int a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                }
        }
    }

    std::array<int, kFFTSize>        rev {};
    std::array<float, kFFTSize / 2>  cosT {}, sinT {};
};

// --------------------------------------------------------------------- motor
class SpaceEngine
{
public:
    SpaceEngine()
    {
        for (int k = 0; k < kFFTSize; ++k)
        {
            const double h = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * (double) k / (double) kFFTSize);
            hann[(size_t) k]  = (float) h;
            sqrtH[(size_t) k] = (float) std::sqrt(h);
        }
        prepare(48000.0);
    }

    static constexpr int latency() { return kFFTSize; }

    void prepare(double sampleRate)
    {
        fs = sampleRate;
        const double binHz  = fs / (double) kFFTSize;
        const double nyq    = fs * 0.5;
        const double ratio  = (double) kMaxFreq / (double) kMinFreq;

        for (int b = 0; b < kNumBands; ++b)
        {
            const double lo = kMinFreq * std::pow(ratio, (double) b / kNumBands);
            const double hi = kMinFreq * std::pow(ratio, (double) (b + 1) / kNumBands);
            const double fc = kMinFreq * std::pow(ratio, ((double) b + 0.5) / kNumBands);
            bandCenter[(size_t) b] = (float) fc;
            bandUsable[(size_t) b] = fc < nyq * 0.98;

            int k0 = (int) std::ceil(lo / binHz);
            int k1 = (int) std::floor(hi / binHz);
            k0 = std::max(1, k0);
            k1 = std::min(kFFTSize / 2 - 1, k1);
            if (k1 >= k0) { bandK0[(size_t) b] = k0; bandK1[(size_t) b] = k1; bandInterp[(size_t) b] = false; }
            else
            {
                const double p = fc / binHz;
                int kk = std::max(1, (int) std::floor(p));
                kk = std::min(kk, kFFTSize / 2 - 2);
                bandK0[(size_t) b] = kk;
                bandK1[(size_t) b] = kk + 1;
                bandFrac[(size_t) b] = (float) std::min(1.0, std::max(0.0, p - (double) kk));
                bandInterp[(size_t) b] = true;
            }
        }

        // posicao fracionaria de cada bin no eixo "indice de banda" (para interpolar a curva)
        for (int k = 0; k <= kFFTSize / 2; ++k)
        {
            const double f = std::max(1.0, (double) k * binHz);
            double u = std::log(f / (double) kMinFreq) / std::log(ratio) * kNumBands - 0.5;
            u = std::min((double) kNumBands - 1.0001, std::max(0.0, u));
            const int i0 = (int) std::floor(u);
            binBand[(size_t) k] = i0;
            binFrac[(size_t) k] = (float) (u - (double) i0);
            binFreq[(size_t) k] = (float) ((double) k * binHz);
        }
        reset();
    }

    void reset()
    {
        for (auto* v : { &inA, &inB, &inSL, &inSR, &outA, &outB, &outSL, &outSR, &dryL, &dryR })
            std::fill(v->begin(), v->end(), 0.0f);
        pos = 0; hopCount = 0; dryPos = 0;
        env.fill(-120.0f); red.fill(0.0f); levels.fill(-120.0f);
        firstFrame = true;
    }

    // Processa L/R no lugar. scL/scR = nullptr -> sidechain interno (a propria faixa).
    void process(float* L, float* R, const float* scL, const float* scR, int n, const Params& p)
    {
        const float outGain = std::pow(10.0f, p.outputDb / 20.0f);
        const float mix = std::min(1.0f, std::max(0.0f, p.mix));
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            const float sL = scL ? scL[i] : inL;
            const float sR = scR ? scR[i] : (scL ? scL[i] : inR);

            const float dL = dryL[(size_t) dryPos], dR = dryR[(size_t) dryPos];
            dryL[(size_t) dryPos] = inL; dryR[(size_t) dryPos] = inR;
            dryPos = (dryPos + 1) & (kFFTSize - 1);

            float a = inL, b = inR;
            if (p.midSide) { a = 0.5f * (inL + inR); b = 0.5f * (inL - inR); }

            const size_t ps = (size_t) pos;
            const float oa = outA[ps], ob = outB[ps], osl = outSL[ps], osr = outSR[ps];
            outA[ps] = outB[ps] = outSL[ps] = outSR[ps] = 0.0f;
            inA[ps] = a; inB[ps] = b; inSL[ps] = sL; inSR[ps] = sR;
            pos = (pos + 1) & (kFFTSize - 1);

            if (++hopCount >= kHop) { hopCount = 0; processFrame(p); }

            float wl = oa, wr = ob;
            if (p.midSide) { wl = oa + ob; wr = oa - ob; }

            if (p.scAudition) { L[i] = osl * outGain; R[i] = osr * outGain; }
            else
            {
                L[i] = (dL * (1.0f - mix) + wl * mix) * outGain;
                R[i] = (dR * (1.0f - mix) + wr * mix) * outGain;
            }
        }
    }

    const std::array<float, kNumBands>& bandLevelsDb() const { return levels; }   // sidechain (suavizado)
    const std::array<float, kNumBands>& bandReductionDb() const { return red; }   // reducao (<= 0)

private:
    static float smooth01(float x) { x = std::min(1.0f, std::max(0.0f, x)); return x * x * (3.0f - 2.0f * x); }

    static float rangeWeight(float f, float lowCut, float highCut)
    {
        float w = 1.0f;
        if (lowCut > 20.5f)   w *= smooth01(std::log2(std::max(f, 1.0f) / lowCut) / 0.5f);
        if (highCut < 19500.f) w *= smooth01(std::log2(highCut / std::max(f, 1.0f)) / 0.5f);
        return w;
    }

    void processFrame(const Params& p)
    {
        constexpr int N = kFFTSize, H = N / 2;
        float* mre = mainRe.data(); float* mim = mainIm.data();
        float* sre = scRe.data();   float* sim = scIm.data();

        for (int k = 0; k < N; ++k)
        {
            const size_t idx = (size_t) ((pos + k) & (N - 1));
            mre[k] = inA[idx] * sqrtH[(size_t) k];
            mim[k] = inB[idx] * sqrtH[(size_t) k];
            sre[k] = inSL[idx] * hann[(size_t) k];
            sim[k] = inSR[idx] * hann[(size_t) k];
        }
        fft.forward(mre, mim);
        fft.forward(sre, sim);

        // ---- potencia do sidechain por bin (media L/R), normalizada: seno 0 dBFS = 1
        const float norm = 1.0f / ((float) N * 0.25f * (float) N * 0.25f);
        for (int k = 1; k < H; ++k)
        {
            const int kk = N - k;
            const float sr = sre[k] + sre[kk], si = sim[k] - sim[kk];   // Xk + conj(Xkk)
            const float dr = sre[k] - sre[kk], di = sim[k] + sim[kk];   // Xk - conj(Xkk)
            binPow[(size_t) k] = 0.5f * 0.25f * (sr * sr + si * si + dr * dr + di * di) * norm;
        }
        binPow[0] = binPow[1];
        binPow[(size_t) H] = binPow[(size_t) (H - 1)];

        // ---- audicao do sidechain (com Low/High cut)
        if (p.scAudition)
        {
            for (int k = 0; k <= H; ++k)
            {
                const float w = rangeWeight(binFreq[(size_t) k], p.lowCutHz, p.highCutHz);
                sre[k] *= w; sim[k] *= w;
                if (k > 0 && k < H) { sre[N - k] *= w; sim[N - k] *= w; }
            }
            fft.inverse(sre, sim);
            for (int k = 0; k < N; ++k)
            {
                const size_t idx = (size_t) ((pos + k) & (N - 1));
                outSL[idx] += sre[k] * 0.5f;
                outSR[idx] += sim[k] * 0.5f;
            }
        }

        // ---- niveis por banda + envelope attack/release (em dB, por quadro)
        if (!p.freeze)
        {
            const float dt = (float) kHop / (float) fs;
            const float aA = std::exp(-dt / (std::max(0.1f, p.attackMs) * 0.001f));
            const float aR = std::exp(-dt / (std::max(1.0f, p.releaseMs) * 0.001f));

            std::array<float, kNumBands> w {};
            float ref = -120.0f;
            for (int b = 0; b < kNumBands; ++b)
            {
                float pw;
                const int k0 = bandK0[(size_t) b], k1 = bandK1[(size_t) b];
                if (bandInterp[(size_t) b])
                    pw = binPow[(size_t) k0] + (binPow[(size_t) k1] - binPow[(size_t) k0]) * bandFrac[(size_t) b];
                else
                {
                    float s = 0.0f;
                    for (int k = k0; k <= k1; ++k) s += binPow[(size_t) k];
                    pw = s / (float) (k1 - k0 + 1);
                }
                float lvl = bandUsable[(size_t) b] ? 10.0f * std::log10(std::max(pw, 1.0e-12f)) : -120.0f;
                float& e = env[(size_t) b];
                if (firstFrame) e = lvl;
                else if (lvl > e) e = aA * e + (1.0f - aA) * lvl;
                else              e = aR * e + (1.0f - aR) * lvl;
                levels[(size_t) b] = e;

                w[(size_t) b] = rangeWeight(bandCenter[(size_t) b], p.lowCutHz, p.highCutHz);
                if (w[(size_t) b] > 0.0f && bandUsable[(size_t) b]) ref = std::max(ref, e);
            }
            firstFrame = false;

            const float gate = smooth01((ref - p.sensitivityDb) / 12.0f);
            const float range = std::max(6.0f, p.rangeDb);
            for (int b = 0; b < kNumBands; ++b)
            {
                const float rel = env[(size_t) b] - ref;
                const float shape = smooth01(1.0f + rel / range);
                red[(size_t) b] = -(p.amount * kMaxCutDb) * gate * shape * w[(size_t) b];
            }
        }

        // ---- ganho por bin (interpolado em dB ao longo da frequencia) e pan
        const float pan = std::min(1.0f, std::max(-1.0f, p.pan));
        const float w0 = pan <= 0.0f ? 1.0f : 1.0f - pan;
        const float w1 = pan >= 0.0f ? 1.0f : 1.0f + pan;
        for (int k = 0; k <= H; ++k)
        {
            const int i0 = binBand[(size_t) k];
            const float r = red[(size_t) i0] + (red[(size_t) std::min(i0 + 1, kNumBands - 1)] - red[(size_t) i0]) * binFrac[(size_t) k];
            g0[(size_t) k] = std::exp(r * w0 * 0.11512925f); // dB -> linear (ln10/20)
            g1[(size_t) k] = std::exp(r * w1 * 0.11512925f);
        }

        // ---- aplica: separa os dois canais empacotados, multiplica, reempacota
        for (int k = 0; k <= H; ++k)
        {
            const int kk = (N - k) & (N - 1);
            const float xr = mre[k], xi = mim[k], yr = mre[kk], yi = mim[kk];
            const float Ar = 0.5f * (xr + yr), Ai = 0.5f * (xi - yi);      // (Xk + conj(Xkk)) / 2
            const float Br = 0.5f * (xi + yi), Bi = -0.5f * (xr - yr);     // (Xk - conj(Xkk)) / (2i)
            const float a0 = g0[(size_t) k], a1 = g1[(size_t) k];
            // Yk = g0*A + i*g1*B
            const float Ykr = a0 * Ar - a1 * Bi, Yki = a0 * Ai + a1 * Br;
            // Ykk = g0*conj(A) + i*g1*conj(B)
            const float Yqr = a0 * Ar + a1 * Bi, Yqi = -a0 * Ai + a1 * Br;
            mre[k] = Ykr; mim[k] = Yki;
            if (kk != k) { mre[kk] = Yqr; mim[kk] = Yqi; }
        }
        fft.inverse(mre, mim);
        for (int k = 0; k < N; ++k)
        {
            const size_t idx = (size_t) ((pos + k) & (N - 1));
            outA[idx] += mre[k] * sqrtH[(size_t) k] * 0.5f;
            outB[idx] += mim[k] * sqrtH[(size_t) k] * 0.5f;
        }
    }

    FFT fft;
    double fs = 48000.0;

    std::array<float, kFFTSize> hann {}, sqrtH {};
    std::array<float, kFFTSize> inA {}, inB {}, inSL {}, inSR {};
    std::array<float, kFFTSize> outA {}, outB {}, outSL {}, outSR {};
    std::array<float, kFFTSize> dryL {}, dryR {};
    std::array<float, kFFTSize> mainRe {}, mainIm {}, scRe {}, scIm {};
    std::array<float, kFFTSize / 2 + 1> binPow {}, g0 {}, g1 {}, binFrac {}, binFreq {};
    std::array<int,   kFFTSize / 2 + 1> binBand {};

    std::array<float, kNumBands> bandCenter {}, bandFrac {}, env {}, red {}, levels {};
    std::array<int,   kNumBands> bandK0 {}, bandK1 {};
    std::array<bool,  kNumBands> bandInterp {}, bandUsable {};

    int  pos = 0, hopCount = 0, dryPos = 0;
    bool firstFrame = true;
};
} // namespace silverspace
