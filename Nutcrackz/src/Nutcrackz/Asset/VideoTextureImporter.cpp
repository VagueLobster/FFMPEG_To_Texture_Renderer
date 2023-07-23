#include "nzpch.h"
#include "TextureImporter.h"

#include "Nutcrackz/Project/Project.h"

#include <stb_image.h>

namespace Nutcrackz {

	Ref<VideoTexture> TextureImporter::ImportVideoTexture(AssetHandle handle, const AssetMetadata& metadata)
	{
		NZ_PROFILE_FUNCTION();

		std::filesystem::path fullPath = Project::GetAssetDirectory() / metadata.FilePath;
		Buffer data;
		VideoReaderState videoState;

		{
			NZ_PROFILE_SCOPE("stbi_load - TextureImporter::ImportVideoTexture");

			if (!VideoTexture::VideoReaderOpen(&videoState, fullPath))
			{
				NZ_CORE_ERROR("TextureImporter::ImportVideoTexture - Could not video from filepath: {}", fullPath.string());
				return nullptr;
			}

			if (!VideoTexture::AudioReaderOpen(&videoState, fullPath))
				NZ_CORE_ERROR("TextureImporter::ImportVideoTexture - Could not audio in video from filepath: {}", fullPath.string());
		}

		TextureSpecification spec;
		spec.Width = videoState.Width;
		spec.Height = videoState.Height;
		spec.Format = ImageFormat::RGBA8;

		data.Data = new uint8_t[videoState.Width * videoState.Height * 4];
		data.Size = videoState.Width * videoState.Height * 4;

		if (data.Data == nullptr)
		{
			NZ_CORE_ERROR("TextureImporter::ImportVideoTexture - Could not load video from filepath: {}", fullPath.string());
			return nullptr;
		}

		videoState.FilePath = fullPath;

		Ref<VideoTexture> texture = VideoTexture::Create(spec, data, videoState);
		data.Release();
		return texture;
	}

}