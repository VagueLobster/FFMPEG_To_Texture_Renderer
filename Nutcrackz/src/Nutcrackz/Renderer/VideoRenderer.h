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

		static void DrawVideoSprite(const glm::mat4& transform, VideoRendererComponent& src, int entityID = -1);

		static void ResetPacketDuration(VideoRendererComponent& src);

	private:
		static void StartBatch();
		static void NextBatch();

		static void RenderVideo(const glm::mat4& transform, VideoRendererComponent& src, int entityID);
		static void RenderFrame(const glm::mat4& transform, VideoRendererComponent& src, int entityID);
		static void RenderCertainFrame(const glm::mat4& transform, VideoRendererComponent& src, int entityID);

	private:
		static bool m_HasInitializedTimer;
		static double m_RestartPointFromPause;
		static double m_PresentationTimeInSeconds;
		static double m_VideoDuration;
		static bool m_IsRenderingVideo;
		static int64_t m_FramePosition;
		static bool m_SeekAudio;
	};

};