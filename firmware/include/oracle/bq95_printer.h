#pragma once
#include "oracle/core.h"

namespace oracle {
class Bq95Printer final : public Printer {
public:
    bool begin();
    PrintStatus status() const override;
    bool submit(const Prediction& result) override;
    std::string notice() const;
    bool busy() const;
    bool releaseRadio(); // Only while idle, before a Wi-Fi/TLS job.
    const std::string& resultId() const { return resultId_; }
private:
    struct Impl;
    Impl* impl_=nullptr; // Application lifetime, including notification callbacks.
    std::string resultId_;
};
}
