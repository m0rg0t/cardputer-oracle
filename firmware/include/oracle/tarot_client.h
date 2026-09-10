#pragma once
#include "oracle/core.h"
#include <memory>

namespace oracle {
class TarotClient {
public:
    bool start(const std::string& question); // Read SD on UI task, then one network job.
    void cancel();
    bool busy() const;
    bool take(Prediction& result, std::string& error); // One completion, including cancellation.
    std::string notice() const;
private:
    struct Impl;
    Impl* impl_=nullptr;
    std::string startError_;
};
}
