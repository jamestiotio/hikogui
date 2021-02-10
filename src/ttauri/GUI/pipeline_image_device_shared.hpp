// Copyright Take Vos 2019-2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "pipeline_image_texture_map.hpp"
#include "pipeline_image_page.hpp"
#include "../required.hpp"
#include "../R16G16B16A16SFloat.hpp"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <mutex>

namespace tt {
class gui_device_vulkan;
template<typename T> class pixel_map;
}

namespace tt::pipeline_image {

struct Image;

struct device_shared final {
    static constexpr int atlasNrHorizontalPages = 16;
    static constexpr int atlasNrVerticalPages = 16;
    static constexpr int atlasImageWidth = atlasNrHorizontalPages * Page::widthIncludingBorder;
    static constexpr int atlasImageHeight = atlasNrVerticalPages * Page::heightIncludingBorder;
    static constexpr int atlasNrPagesPerImage = atlasNrHorizontalPages * atlasNrVerticalPages;
    static constexpr int atlasMaximumNrImages = 16;
    static constexpr int stagingImageWidth = 1024;
    static constexpr int stagingImageHeight = 1024;

    gui_device_vulkan const &device;

    vk::ShaderModule vertexShaderModule;
    vk::ShaderModule fragmentShaderModule;
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    texture_map stagingTexture;
    std::vector<texture_map> atlasTextures;

    std::array<vk::DescriptorImageInfo, atlasMaximumNrImages> atlasDescriptorImageInfos;
    vk::Sampler atlasSampler;
    vk::DescriptorImageInfo atlasSamplerDescriptorImageInfo;

    std::vector<Page> atlasFreePages;

    device_shared(gui_device_vulkan const &device);
    ~device_shared();

    device_shared(device_shared const &) = delete;
    device_shared &operator=(device_shared const &) = delete;
    device_shared(device_shared &&) = delete;
    device_shared &operator=(device_shared &&) = delete;

    /*! Deallocate vulkan resources.
    * This is called in the destructor of gui_device_vulkan, therefor we can not use our `std::weak_ptr<gui_device_vulkan> device`.
    */
    void destroy(gui_device_vulkan *vulkanDevice);

    /*! Get the coordinate in the atlas from a page index.
     * \param page number in the atlas
     * \return x, y pixel coordinate in an atlasTexture and z the atlasTextureIndex.
     */
    static i32x4 getAtlasPositionFromPage(Page page) noexcept {
        ttlet imageIndex = page.nr / atlasNrPagesPerImage;
        ttlet pageNrInsideImage = page.nr % atlasNrPagesPerImage;

        ttlet pageY = pageNrInsideImage / atlasNrVerticalPages;
        ttlet pageX = pageNrInsideImage % atlasNrVerticalPages;

        ttlet x = pageX * Page::widthIncludingBorder + Page::border;
        ttlet y = pageY * Page::heightIncludingBorder + Page::border;

        return i32x4{narrow_cast<int>(x), narrow_cast<int>(y), narrow_cast<int>(imageIndex), 1};
    }

    /** Allocate pages from the atlas.
     */
    std::vector<Page> allocatePages(int const nrPages) noexcept;

    /** Deallocate pages back to the atlas.
     */
    void freePages(std::vector<Page> const &pages) noexcept;

    /** Allocate an image in the atlas.
     * \param extent of the image.
     */
    Image makeImage(i32x4 extent) noexcept;

    void drawInCommandBuffer(vk::CommandBuffer &commandBuffer);

    tt::pixel_map<R16G16B16A16SFloat> getStagingPixelMap();

    void prepareAtlasForRendering();

private:
    tt::pixel_map<R16G16B16A16SFloat> getStagingPixelMap(i32x4 extent) {
        return getStagingPixelMap().submap({i32x4::point(0,0), extent});
    }

    void updateAtlasWithStagingPixelMap(Image const &image);

    void buildShaders();
    void teardownShaders(gui_device_vulkan *vulkanDevice);
    void addAtlasImage();
    void buildAtlas();
    void teardownAtlas(gui_device_vulkan *vulkanDevice);

    friend Image;
};

}