#pragma once

#include "core/core.h"

enum GLESChunkType
{
    DEVICE_INIT = FIRST_CHUNK_ID,

    CLEAR,
    CLEAR_COLOR,
    VIEWPORT,

    CONTEXT_CAPTURE_FOOTER,

    NUM_GLES_CHUNKS,
};


