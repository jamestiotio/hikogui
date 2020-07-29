// Copyright 2019 Pokitec
// All rights reserved.

#include "PipelineImage_TextureMap.hpp"
#include "GUIDevice.hpp"

namespace tt::PipelineImage {

void TextureMap::transitionLayout(const GUIDevice &device, vk::Format format, vk::ImageLayout nextLayout) {
    if (layout != nextLayout) {
        device.transitionLayout(image, format, layout, nextLayout);
        layout = nextLayout;
    }
}

}