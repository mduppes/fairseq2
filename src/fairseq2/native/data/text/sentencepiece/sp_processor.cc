// Copyright (c) Meta Platforms, Inc. and affiliates.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include "fairseq2/native/data/text/sentencepiece/sp_processor.h"

#include <stdexcept>

#include <fmt/format.h>
#include <sentencepiece/src/builtin_pb/sentencepiece_model.pb.h>

#include "fairseq2/native/utils/cast.h"

using sentencepiece::ImmutableSentencePieceText;
using sentencepiece::ModelProto;
using sentencepiece::ModelProto_SentencePiece;
using sentencepiece::SentencePieceProcessor;

namespace fairseq2::detail {

class sp_model_loader {
public:
    explicit
    sp_model_loader(std::string_view pathname, sp_model_options &&opts) noexcept
        : pathname_{pathname}, opts_{std::move(opts)}
    {}

    std::unique_ptr<SentencePieceProcessor> &&
    load() &&;

private:
    void
    load_proto();

    void
    add_control_tokens();

    void
    add_piece(std::string &&piece);

    void
    load_processor();

    void
    set_encoder_extras();

    void
    set_decoder_extras();

private:
    std::string_view pathname_;
    sp_model_options opts_;
    std::unique_ptr<ModelProto> proto_{};
    std::unique_ptr<SentencePieceProcessor> processor_{};
};

std::unique_ptr<SentencePieceProcessor> &&
sp_model_loader::load() &&
{
    load_proto();

    add_control_tokens();

    load_processor();

    return std::move(processor_);
}

void
sp_model_loader::load_proto()
{
    proto_ = std::make_unique<ModelProto>();

    auto st = sentencepiece::io::LoadModelProto(pathname_, proto_.get());
    if (st.ok())
        return;

    if (st.code() == sentencepiece::util::StatusCode::kNotFound)
        throw std::system_error{
            std::make_error_code(std::errc::no_such_file_or_directory)};

    if (st.code() == sentencepiece::util::StatusCode::kPermissionDenied)
        throw std::system_error{
            std::make_error_code(std::errc::permission_denied)};

    throw std::runtime_error{st.message()};
}

void
sp_model_loader::add_control_tokens()
{
    for (std::string &token : opts_.control_tokens()) {
        if (token.empty())
            continue;

        if (token == "<pad>" || token == "<pad>@0") {
            proto_->mutable_trainer_spec()->set_pad_piece("<pad>");

            add_piece("<pad>");

            // This is a workaround for SentencePiece models that, for legacy
            // reasons, do not have a pad token, but expected to have one at
            // index 0 (e.g. NLLB models).
            if (token == "<pad>@0") {
                auto *pieces = proto_->mutable_pieces();

                // RepeatedPtrField does not offer an insert method, so we move
                // our pad token from the end to the beginning of the list.
                for (int i = pieces->size() - 1; i > 0; --i)
                    pieces->SwapElements(i, i - 1);
            }
        } else
            add_piece(std::move(token));
    }
}

void
sp_model_loader::add_piece(std::string &&piece)
{
    ModelProto_SentencePiece *sp = proto_->add_pieces();

    sp->set_piece(std::move(piece));

    sp->set_type(ModelProto_SentencePiece::CONTROL);
}

void
sp_model_loader::load_processor()
{
    processor_ = std::make_unique<SentencePieceProcessor>();

    auto st = processor_->Load(std::move(proto_));
    if (!st.ok())
        throw std::runtime_error{st.message()};
}

sp_processor::sp_processor(std::string_view model_pathname, sp_model_options &&opts)
{
    sp_model_loader loader{model_pathname, std::move(opts)};

    native_ = std::move(loader).load();

    unk_idx = conditional_cast<std::int32_t>(native_->unk_id());
    bos_idx = conditional_cast<std::int32_t>(native_->bos_id());
    eos_idx = conditional_cast<std::int32_t>(native_->eos_id());
    pad_idx = conditional_cast<std::int32_t>(native_->pad_id());

    if (pad_idx < 0)
        throw std::runtime_error{"The model has no padding token specified."};

    vocab_size = conditional_cast<std::size_t>(native_->GetPieceSize());
}

ImmutableSentencePieceText
sp_processor::encode(std::string_view text) const
{
    ImmutableSentencePieceText spt{};

    auto st = native_->Encode(text, spt.mutable_proto());
    if (!st.ok())
        throw std::runtime_error{st.message()};

    return spt;
}

ImmutableSentencePieceText
sp_processor::sample(std::string_view text, std::int32_t nbest_size, float alpha) const
{
    ImmutableSentencePieceText spt{};

    auto st = native_->SampleEncode(text, nbest_size, alpha, spt.mutable_proto());
    if (!st.ok())
        throw std::runtime_error{st.message()};

    return spt;
}

std::string
sp_processor::decode(const std::vector<std::string_view> &tokens) const
{
    std::string text{};

    auto st = native_->Decode(tokens, &text);
    if (!st.ok())
        throw std::runtime_error{st.message()};

    return text;
}

std::int32_t
sp_processor::token_to_index(std::string_view token) const
{
    return conditional_cast<std::int32_t>(native_->PieceToId(token));
}

std::string_view
sp_processor::index_to_token(std::int32_t idx) const
{
    if (static_cast<std::size_t>(idx) >= vocab_size)
        throw std::domain_error{"The specified index is out of range."};

    return native_->IdToPiece(conditional_cast<int>(idx));
}

}  // namespace fairseq2::detail
