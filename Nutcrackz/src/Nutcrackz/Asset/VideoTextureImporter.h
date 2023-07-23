#pragma once

#include "Asset.h"
#include "AssetMetadata.h"

#include "Nutcrackz/Renderer/Texture.h"
#include "Nutcrackz/Video/VideoTexture.h"

namespace Nutcrackz {

	class TextureImporter
	{
	public:
		// AssetMetadata filepath is relative to project asset directory
		static Ref<VideoTexture> ImportVideoTexture(AssetHandle handle, const AssetMetadata& metadata);
	};

}