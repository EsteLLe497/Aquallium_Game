#pragma once
#include <d3d11.h>
#include <filesystem>
#include <wrl/client.h>

namespace story {
struct StoryTexture {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    unsigned width=0,height=0;
    float alphaCoverage=0.f;
    bool Load(ID3D11Device* device,const std::filesystem::path& path);
};
}
