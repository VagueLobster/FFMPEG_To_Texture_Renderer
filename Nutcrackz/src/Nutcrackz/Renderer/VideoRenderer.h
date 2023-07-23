#pragma once

#include "Nutcrackz/Scene/Components.h"

namespace Nutcrackz {

	class VideoRenderer
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void BeginScene(const EditorCamera& camera);

		static void EndScene();
		static void Flush();

		static void DrawVideoSprite(TransformComponent& transform, VideoRendererComponent& src, VideoData& data, int entityID = -1);

		static void ResetPacketDuration(VideoRendererComponent& src);

	private:
		static void StartBatch();
		static void NextBatch();

		static void RenderVideo(TransformComponent& transform, const Ref<VideoTexture>& texture, VideoRendererComponent& src, VideoData& data, int entityID);
		static void RenderFrame(TransformComponent& transform, const Ref<VideoTexture>& texture, VideoRendererComponent& src, VideoData& data, int entityID);
		static void RenderCertainFrame(TransformComponent& transform, const Ref<VideoTexture>& texture, VideoRendererComponent& src, VideoData& data, int entityID);
	};

};