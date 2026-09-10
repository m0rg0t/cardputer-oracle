#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace oracle {
enum class Sound { None, Click, Scroll, Cancel, Ritual, Reveal, Error };
constexpr std::uint32_t kSoundRate = 16000;

// Narration starts only after the effect channel drained, plus a short DMA tail.
class NarrationGate {
public:
    void request() { pending_=true; quiet_=false; }
    void cancel() { pending_=quiet_=false; }
    bool update(std::uint32_t now,bool effectsBusy,bool allowed) {
        if(!allowed) { cancel(); return false; }
        if(!pending_) return false;
        if(effectsBusy) { quiet_=false; return false; }
        if(!quiet_) { quiet_=true; quietSince_=now; }
        if(std::uint32_t(now-quietSince_)<70) return false;
        cancel(); return true;
    }
private:
    bool pending_=false, quiet_=false;
    std::uint32_t quietSince_=0;
};

// Bounded additive bell synth: phase accumulators + a small sine table.
// No file I/O, allocations or trigonometry in the per-sample render loop.
class SoundSynth {
public:
    SoundSynth();
    void start(Sound sound);
    void stop() { position_=length_=0; }
    bool active() const { return position_<length_; }
    std::size_t render(std::int16_t* out, std::size_t capacity);
private:
    struct Bell {
        std::uint32_t start=0, duration=0, step=0, overtoneStep=0, phase=0, overtone=0;
        float level=0, decay=0, envelope=1;
    };
    std::array<float,256> sine_{};
    std::array<Bell,8> bells_{};
    unsigned count_=0;
    std::uint32_t position_=0, length_=0;
    void bell(float hz, unsigned startMs, unsigned durationMs, float level);
};

// Output owns one dedicated virtual speaker channel and implements
// ready(), queued(), stop(), submit(samples,count). Stop is asynchronous.
// Three rotating buffers follow M5Unified's runtime playRaw contract.
template<class Output> class SoundPlayer {
public:
    explicit SoundPlayer(Output& output) : output_(output) {}
    bool busy() const { return waiting_!=Sound::None || synth_.active() || pending_ || output_.queued(); }
    bool play(Sound sound) {
        if(muted_ || sound==Sound::None) return false;
        // Navigation cannot replace the ritual/reveal or build up a backlog.
        if((sound==Sound::Click || sound==Sound::Scroll) && busy()) return false;
        stop();
        waiting_=sound;
        return true;
    }
    void stop() {
        output_.stop(); synth_.stop(); pending_=0; waiting_=Sound::None;
        draining_=true;
    }
    void mute(bool muted) { muted_=muted; if(muted) stop(); }
    void update() {
        if(muted_) return;
        if(draining_) {
            // Never overwrite a PCM buffer while the audio task still reads it.
            if(output_.queued()) return;
            draining_=false; next_=0;
        }
        if(waiting_!=Sound::None) {
            if(!output_.ready()) { waiting_=Sound::None; return; }
            synth_.start(waiting_); waiting_=Sound::None;
        }
        if(output_.queued()>=2) return;
        if(!pending_) pending_=synth_.render(buffers_[next_].data(),buffers_[next_].size());
        if(pending_ && output_.submit(buffers_[next_].data(),pending_)) {
            pending_=0; next_=(next_+1)%buffers_.size();
        }
    }
private:
    Output& output_;
    SoundSynth synth_;
    std::array<std::array<std::int16_t,512>,3> buffers_{};
    std::size_t pending_=0, next_=0;
    Sound waiting_=Sound::None;
    bool muted_=false, draining_=false;
};
}
