# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from pathlib import Path
from typing import Any, Final

import pytest
import torch

from fairseq2.data import MemoryBlock
from fairseq2.data.audio import AudioDecoder
from tests.common import assert_close, device

TEST_OGG_PATH: Final = Path(__file__).parent.joinpath("test.ogg")


class TestAudioDecoder:
    def test_encodes_as_expected(self) -> None:
        decoder = AudioDecoder(device=device)

        with TEST_OGG_PATH.open("rb") as fb:
            block = MemoryBlock(fb.read())

        output = decoder(block)

        assert output["format"] == 0x200060  # OGG Vorbis

        assert output["sample_rate"] == 16000

        audio = output["audio"]

        assert audio.shape == (28800, 1)

        assert audio.dtype == torch.float

        assert audio.device == device

        assert_close(audio[0][0], torch.tensor(9.0017202e-6, device=device))

        assert_close(audio.sum(), torch.tensor(-0.753374, device=device))

    @pytest.mark.parametrize(
        "value,type_name", [(None, "pyobj"), (123, "int"), ("s", "string")]
    )
    def test_raises_error_if_input_is_not_memory_block(
        self, value: Any, type_name: str
    ) -> None:
        decoder = AudioDecoder()

        with pytest.raises(
            ValueError,
            match=rf"^The input data must be of type `memory_block`, but is of type `{type_name}` instead\.$",
        ):
            decoder(value)

    def test_raises_error_if_input_is_empty(self) -> None:
        decoder = AudioDecoder()

        empty_block = MemoryBlock()

        with pytest.raises(
            ValueError,
            match=r"^The input memory block has zero length and cannot be decoded as audio\.$",
        ):
            decoder(empty_block)

    def test_raises_error_if_input_is_invalid(self) -> None:
        decoder = AudioDecoder()

        block = MemoryBlock(b"foo")

        with pytest.raises(
            ValueError,
            match=r"^The input audio cannot be decoded. See nested exception for details\.$",
        ):
            decoder(block)
