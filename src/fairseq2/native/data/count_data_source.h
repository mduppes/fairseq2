// Copyright (c) Meta Platforms, Inc. and affiliates.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>

#include "fairseq2/native/data/data_source.h"

namespace fairseq2::detail {

class count_data_source final : public data_source {
public:
    explicit
    count_data_source(std::int64_t start) noexcept
      : start_{start}, counter_{start}
    {}

    std::optional<data>
    next() override;

    void
    reset() override;

    void
    record_position(tape &t) const override;

    void
    reload_position(tape &t) override;

private:
    std::int64_t start_;
    std::int64_t counter_;
};

}  // namespace fairseq2::detail