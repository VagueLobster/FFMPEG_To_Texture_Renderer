#include "nzpch.h"
#include "VideoRenderer.h"

#include "Nutcrackz/Video/VideoTexture.h"

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"
#include "UniformBuffer.h"

namespace Nutcrackz {

	bool VideoRenderer::m_HasInitializedTimer = false;
	double VideoRenderer::m_RestartPointFromPause = 0.0;
	double VideoRenderer::m_PresentationTimeInSeconds = 0.0;
	double VideoRenderer::m_VideoDuration = 0.0;
	bool VideoRenderer::m_IsRenderingVideo = false;
	int64_t VideoRenderer::m_FramePosition = 0;
	bool VideoRenderer::m_SeekAudio = false;

	struct VideoVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		glm::vec2 TilingFactor;
		float TexIndex;

		// Editor-only
		int EntityID;
	};

	struct VideoRendererData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO: RenderCaps

		Ref<VertexArray> VideoVertexArray;
		Ref<VertexBuffer> VideoVertexBuffer;
		Ref<Shader> VideoShader;

		uint32_t VideoIndexCount = 0;
		VideoVertex* VideoVertexBufferBase = nullptr;
		VideoVertex* VideoVertexBufferPtr = nullptr;

		std::array<Ref<VideoTexture>, MaxTextureSlots> VideoTextureSlots;

		Ref<VideoTexture> WhiteVideoTexture;
		uint32_t VideoTextureSlotIndex = 1; // 0 = white texture

		glm::vec4 QuadVertexPositions[4];

		struct CameraData
		{
			glm::mat4 ViewProjection;
		};

		CameraData CameraBuffer;
		Ref<UniformBuffer> CameraUniformBuffer;
	};

	static VideoRendererData s_VideoData;

	void VideoRenderer::Init()
	{
		s_VideoData.VideoVertexArray = VertexArray::Create();

		s_VideoData.VideoVertexBuffer = VertexBuffer::Create(s_VideoData.MaxVertices * sizeof(VideoVertex));
		s_VideoData.VideoVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position"              },
			{ ShaderDataType::Float4, "a_Color"                 },
			{ ShaderDataType::Float2, "a_TexCoord"              },
			{ ShaderDataType::Float2, "a_TilingFactor"          },
			{ ShaderDataType::Float,  "a_TexIndex"              },
			{ ShaderDataType::Int,    "a_EntityID"              }
		});
		s_VideoData.VideoVertexArray->AddVertexBuffer(s_VideoData.VideoVertexBuffer);

		s_VideoData.VideoVertexBufferBase = new VideoVertex[s_VideoData.MaxVertices];

		uint32_t* quadIndices = new uint32_t[s_VideoData.MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_VideoData.MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices, s_VideoData.MaxIndices);
		s_VideoData.VideoVertexArray->SetIndexBuffer(quadIB);
		delete[] quadIndices;

		s_VideoData.WhiteVideoTexture = VideoTexture::Create(TextureSpecification());
		uint32_t whiteVideoTextureData = 0xffffffff;
		s_VideoData.WhiteVideoTexture->SetData(&whiteVideoTextureData, sizeof(uint32_t));

		s_VideoData.VideoShader = Shader::Create("assets/shaders/Renderer2D_Quad.glsl");

		// Set first texture slot to 0
		s_VideoData.VideoTextureSlots[0] = s_VideoData.WhiteVideoTexture;

		s_VideoData.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_VideoData.QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		s_VideoData.QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		s_VideoData.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_VideoData.CameraUniformBuffer = UniformBuffer::Create(sizeof(VideoRendererData::CameraData), 0);
	}

	void VideoRenderer::Shutdown()
	{
		//NZ_PROFILE_FUNCTION();
		 
		delete[] s_VideoData.VideoVertexBufferBase;
	}

	void VideoRenderer::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		//NZ_PROFILE_FUNCTION();

		s_VideoData.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_VideoData.CameraUniformBuffer->SetData(&s_VideoData.CameraBuffer, sizeof(VideoRendererData::CameraData));

		StartBatch();
	}

	void VideoRenderer::BeginScene(const EditorCamera& camera)
	{
		//NZ_PROFILE_FUNCTION();

		s_VideoData.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_VideoData.CameraUniformBuffer->SetData(&s_VideoData.CameraBuffer, sizeof(VideoRendererData::CameraData));

		StartBatch();
	}

	void VideoRenderer::EndScene()
	{
		//NZ_PROFILE_FUNCTION();

		Flush();
	}

	void VideoRenderer::Flush()
	{
		if (s_VideoData.VideoIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_VideoData.VideoVertexBufferPtr - (uint8_t*)s_VideoData.VideoVertexBufferBase);
			s_VideoData.VideoVertexBuffer->SetData(s_VideoData.VideoVertexBufferBase, dataSize);

			// Bind textures
			for (uint32_t i = 0; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (s_VideoData.VideoTextureSlots[i])
					s_VideoData.VideoTextureSlots[i]->Bind(i);
			}

			s_VideoData.VideoShader->Bind();
			RenderCommand::DrawIndexed(s_VideoData.VideoVertexArray, s_VideoData.VideoIndexCount);
		}
	}

	void VideoRenderer::StartBatch()
	{
		s_VideoData.VideoIndexCount = 0;
		s_VideoData.VideoVertexBufferPtr = s_VideoData.VideoVertexBufferBase;

		s_VideoData.VideoTextureSlotIndex = 1;
	}

	void VideoRenderer::NextBatch()
	{
		Flush();
		StartBatch();
	}

#pragma region FromGLFW

	// Because of the way ImGui uses GLFW's SetTime(),
	// i cannot use it in here, unless i copied the entire Timer structure that GLFW uses
	// in order to make it work! So that's what i did!
	struct VideoTimer
	{
		uint64_t Offset;

		struct VideoTimerWin32
		{
			bool HasPC;
			uint64_t Frequency;

		} win32;

	} timer;

	void InitTimerWin32()
	{
		uint64_t frequency;

		if (QueryPerformanceFrequency((LARGE_INTEGER*)&frequency))
		{
			timer.win32.HasPC = true;
			timer.win32.Frequency = frequency;
		}
		else
		{
			timer.win32.HasPC = false;
			timer.win32.Frequency = 1000;
		}
	}

	uint64_t PlatformGetTimerValue()
	{
		if (timer.win32.HasPC)
		{
			uint64_t value;
			QueryPerformanceCounter((LARGE_INTEGER*)&value);
			return value;
		}
		else
			return (uint64_t)timeGetTime();
	}

	uint64_t PlatformGetTimerFrequency()
	{
		return timer.win32.Frequency;
	}

	double GetTime()
	{
		return (double)(PlatformGetTimerValue() - timer.Offset) / PlatformGetTimerFrequency();
	}

	void SetTime(double time)
	{
		if (time != time || time < 0.0 || time > 18446744073.0)
		{
			NZ_CORE_TRACE("Invalid time: {0}", time);
			return;
		}

		timer.Offset = PlatformGetTimerValue() - (uint64_t)(time * PlatformGetTimerFrequency());
	}

#pragma endregion

	void VideoRenderer::RenderVideo(const glm::mat4& transform, VideoRendererComponent& src, int entityID)
	{
		if (!m_HasInitializedTimer)
		{
			InitTimerWin32();
			m_HasInitializedTimer = true;
		}

		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }, };

		float textureIndex = 0.0f;
		const glm::vec2 tilingFactor(1.0f);

		// This if statement is very much required!
		// Because without it, this function creates a big memory leak!
		if (src.VideoRendererID)
			src.Video->DeleteRendererID(src.VideoRendererID);

		if (src.Video)
		{
			if (src.UseVideoAudio)
			{
				src.Video->ReadAndPlayAudio(&src.Video->GetVideoState(), m_FramePosition, m_SeekAudio, src.PauseVideo);
			}

			if (!m_IsRenderingVideo)
			{
				SetTime(0.0);
				m_IsRenderingVideo = true;
			}

			if (m_FramePosition != 0)
			{
				SetTime(src.FramePosition / src.Video->GetVideoState().Framerate);

				if (!src.Video->VideoReaderSeekFrame(&src.Video->GetVideoState(), m_FramePosition))
				{
					NZ_CORE_WARN("Could not seek video back to start frame!");
					return;
				}

				src.PresentationTimeStamp = m_FramePosition;
				m_FramePosition = 0;
			}

			src.VideoRendererID = src.Video->GetIDFromTexture(src.VideoFrameData, &src.PresentationTimeStamp, src.PauseVideo);
			src.Video->SetRendererID(src.VideoRendererID);

			if (src.PauseVideo)
			{
				if (src.UseVideoAudio)
				{
					if (!src.Video->AVReaderSeekFrame(&src.Video->GetVideoState(), src.PresentationTimeStamp))
					{
						NZ_CORE_WARN("Could not seek a/v back to start frame!");
						return;
					}
				}
				else
				{
					if (!src.Video->VideoReaderSeekFrame(&src.Video->GetVideoState(), src.PresentationTimeStamp))
					{
						NZ_CORE_WARN("Could not seek video back to start frame!");
						return;
					}
				}

				SetTime(0.0);
			}
			else if (!src.PauseVideo)
			{
				if (m_RestartPointFromPause > GetTime())
					SetTime(m_RestartPointFromPause);

				if (m_RestartPointFromPause < GetTime())
					m_RestartPointFromPause = GetTime();

				m_PresentationTimeInSeconds = src.PresentationTimeStamp * ((double)src.Video->GetVideoState().TimeBase.num / (double)src.Video->GetVideoState().TimeBase.den);
				
				int hoursToSeconds = src.Video->GetVideoState().Hours * 3600;
				int minutesToSeconds = src.Video->GetVideoState().Mins * 60;
				
				if (m_VideoDuration != hoursToSeconds + minutesToSeconds + src.Video->GetVideoState().Secs + (0.01 * ((100 * src.Video->GetVideoState().Us) / AV_TIME_BASE)))
					m_VideoDuration = hoursToSeconds + minutesToSeconds + src.Video->GetVideoState().Secs + (0.01 * ((100 * src.Video->GetVideoState().Us) / AV_TIME_BASE));

				if (src.Hours != src.Video->GetVideoState().Hours)
					src.Hours = src.Video->GetVideoState().Hours;

				if (src.Minutes != src.Video->GetVideoState().Mins)
					src.Minutes = src.Video->GetVideoState().Mins;

				if (src.Seconds != src.Video->GetVideoState().Secs)
					src.Seconds = src.Video->GetVideoState().Secs;

				if (src.Milliseconds != src.Video->GetVideoState().Us)
					src.Milliseconds = src.Video->GetVideoState().Us;

				NZ_CORE_WARN("Presentation timestamp = {0}, Timebase num/den = {1}", src.PresentationTimeStamp, ((double)src.Video->GetVideoState().TimeBase.num / (double)src.Video->GetVideoState().TimeBase.den));
				
				while (m_PresentationTimeInSeconds > GetTime())
				{
					Sleep(m_PresentationTimeInSeconds - GetTime());
				}

				if (src.RepeatVideo && GetTime() > m_VideoDuration)
				{
					if (src.UseVideoAudio)
					{
						src.Video->CloseAudio(&src.Video->GetVideoState());

						if (!src.Video->AVReaderSeekFrame(&src.Video->GetVideoState(), 0, true))
						{
							NZ_CORE_WARN("Could not seek a/v back to start frame!");
							return;
						}
					}
					else
					{
						if (!src.Video->VideoReaderSeekFrame(&src.Video->GetVideoState(), 0))
						{
							NZ_CORE_WARN("Could not seek video back to start frame!");
							return;
						}
					}

					src.PresentationTimeStamp = 0;
					src.FramePosition = 0;
					SetTime(0.0);
					m_RestartPointFromPause = 0.0;
				}
			}

			if (s_VideoData.VideoIndexCount >= VideoRendererData::MaxIndices)
				NextBatch();

			for (uint32_t i = 1; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (*s_VideoData.VideoTextureSlots[i] == *src.Video)
				{
					textureIndex = (float)i;
					break;
				}
			}

			if (textureIndex == 0.0f)
			{
				if (s_VideoData.VideoTextureSlotIndex >= VideoRendererData::MaxTextureSlots)
					NextBatch();

				textureIndex = (float)s_VideoData.VideoTextureSlotIndex;

				if (src.Video)
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = src.Video;
				else
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = s_VideoData.WhiteVideoTexture;

				s_VideoData.VideoTextureSlotIndex++;
			}
		}
		
		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_VideoData.VideoVertexBufferPtr->Position = transform * s_VideoData.QuadVertexPositions[i];
			s_VideoData.VideoVertexBufferPtr->Color = src.Color;
			s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
			s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
			s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
			s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
			s_VideoData.VideoVertexBufferPtr++;
		}

		s_VideoData.VideoIndexCount += 6;
	}

	void VideoRenderer::RenderFrame(const glm::mat4& transform, VideoRendererComponent& src, int entityID)
	{
		constexpr size_t videoVertexCount = 4;
		float textureIndex = 0.0f;

		const glm::vec2 tilingFactor(1.0f);

		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		if (src.Video)
		{
			if (m_FramePosition != src.FramePosition * src.Video->GetVideoState().VideoPacketDuration)
			{
				m_FramePosition = src.FramePosition * src.Video->GetVideoState().VideoPacketDuration;
			}

			if (m_IsRenderingVideo)
			{
				if (src.UseVideoAudio)
				{
					if (!src.Video->AVReaderSeekFrame(&src.Video->GetVideoState(), 0, true))
					{
						NZ_CORE_WARN("Could not seek a/v back to start frame!");
						return;
					}

					src.Video->CloseAudio(&src.Video->GetVideoState());
				}
				else
				{
					if (!src.Video->VideoReaderSeekFrame(&src.Video->GetVideoState(), 0))
					{
						NZ_CORE_WARN("Could not seek video back to start frame!");
						return;
					}
				}

				src.Video->DeleteRendererID(src.VideoRendererID);
				src.Video->CloseVideo(&src.Video->GetVideoState());
				src.VideoRendererID = src.Video->GetIDFromTexture(src.VideoFrameData, &src.PresentationTimeStamp, src.PauseVideo);
				src.Video->SetRendererID(src.VideoRendererID);

				m_SeekAudio = true;
				src.PresentationTimeStamp = 0;
				SetTime(0.0);
				m_RestartPointFromPause = 0.0;
				m_IsRenderingVideo = false;
			}

			if (src.NumberOfFrames != src.Video->GetVideoState().NumberOfFrames)
				src.NumberOfFrames = src.Video->GetVideoState().NumberOfFrames;

			if (src.Hours != src.Video->GetVideoState().Hours)
				src.Hours = src.Video->GetVideoState().Hours;

			if (src.Minutes != src.Video->GetVideoState().Mins)
				src.Minutes = src.Video->GetVideoState().Mins;

			if (src.Seconds != src.Video->GetVideoState().Secs)
				src.Seconds = src.Video->GetVideoState().Secs;

			if (src.Milliseconds != src.Video->GetVideoState().Us)
				src.Milliseconds = src.Video->GetVideoState().Us;

			if (s_VideoData.VideoIndexCount >= VideoRendererData::MaxIndices)
				NextBatch();

			for (uint32_t i = 1; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (*s_VideoData.VideoTextureSlots[i] == *src.Video)
				{
					textureIndex = (float)i;
					break;
				}
			}

			if (textureIndex == 0.0f)
			{
				if (s_VideoData.VideoTextureSlotIndex >= VideoRendererData::MaxTextureSlots)
					NextBatch();

				textureIndex = (float)s_VideoData.VideoTextureSlotIndex;

				if (src.Video)
				{
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = src.Video;
				}
				else
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = s_VideoData.WhiteVideoTexture;

				s_VideoData.VideoTextureSlotIndex++;
			}
		}

		for (size_t i = 0; i < videoVertexCount; i++)
		{
			s_VideoData.VideoVertexBufferPtr->Position = transform * s_VideoData.QuadVertexPositions[i];
			s_VideoData.VideoVertexBufferPtr->Color = src.Color;
			s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
			s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
			s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
			s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
			s_VideoData.VideoVertexBufferPtr++;
		}

		s_VideoData.VideoIndexCount += 6;
	}

	void VideoRenderer::RenderCertainFrame(const glm::mat4& transform, VideoRendererComponent& src, int entityID)
	{
		constexpr size_t videoVertexCount = 4;
		float textureIndex = 0.0f;

		const glm::vec2 tilingFactor(1.0f);

		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		if (src.Video)
		{
			if (src.NumberOfFrames != src.Video->GetVideoState().NumberOfFrames)
				src.NumberOfFrames = src.Video->GetVideoState().NumberOfFrames;

			if (src.Hours != src.Video->GetVideoState().Hours)
				src.Hours = src.Video->GetVideoState().Hours;

			if (src.Minutes != src.Video->GetVideoState().Mins)
				src.Minutes = src.Video->GetVideoState().Mins;

			if (src.Seconds != src.Video->GetVideoState().Secs)
				src.Seconds = src.Video->GetVideoState().Secs;

			if (src.Milliseconds != src.Video->GetVideoState().Us)
				src.Milliseconds = src.Video->GetVideoState().Us;

			if (m_FramePosition != src.FramePosition * src.Video->GetVideoState().VideoPacketDuration)
			{
				if (m_IsRenderingVideo)
				{
					if (src.UseVideoAudio)
					{
						src.Video->CloseAudio(&src.Video->GetVideoState());

						if (!src.Video->AVReaderSeekFrame(&src.Video->GetVideoState(), 0, true))
						{
							NZ_CORE_WARN("Could not seek a/v back to start frame!");
							return;
						}
					}

					m_SeekAudio = true;
					SetTime(0.0);
					m_RestartPointFromPause = 0.0;
					m_IsRenderingVideo = false;
				}

				m_FramePosition = src.FramePosition * src.Video->GetVideoState().VideoPacketDuration;

				if (!src.Video->VideoReaderSeekFrame(&src.Video->GetVideoState(), m_FramePosition))
				{
					NZ_CORE_WARN("Could not seek video back to start frame!");
					return;
				}

				src.PresentationTimeStamp = m_FramePosition;

				src.Video->DeleteRendererID(src.VideoRendererID);
				src.VideoRendererID = src.Video->GetIDFromTexture(src.VideoFrameData, &src.PresentationTimeStamp, src.PauseVideo);
				src.Video->SetRendererID(src.VideoRendererID);
			}

			if (s_VideoData.VideoIndexCount >= VideoRendererData::MaxIndices)
				NextBatch();

			for (uint32_t i = 1; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (*s_VideoData.VideoTextureSlots[i] == *src.Video)
				{
					textureIndex = (float)i;
					break;
				}
			}

			if (textureIndex == 0.0f)
			{
				if (s_VideoData.VideoTextureSlotIndex >= VideoRendererData::MaxTextureSlots)
					NextBatch();

				textureIndex = (float)s_VideoData.VideoTextureSlotIndex;

				if (src.Video)
				{
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = src.Video;
				}
				else
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = s_VideoData.WhiteVideoTexture;

				s_VideoData.VideoTextureSlotIndex++;
			}
		}

		for (size_t i = 0; i < videoVertexCount; i++)
		{
			s_VideoData.VideoVertexBufferPtr->Position = transform * s_VideoData.QuadVertexPositions[i];
			s_VideoData.VideoVertexBufferPtr->Color = src.Color;
			s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
			s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
			s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
			s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
			s_VideoData.VideoVertexBufferPtr++;
		}

		s_VideoData.VideoIndexCount += 6;
	}

	void VideoRenderer::DrawVideoSprite(const glm::mat4& transform, VideoRendererComponent& src, int entityID)
	{
		if (src.PlayVideo)
			RenderVideo(transform, src, entityID);
		else if (!src.PlayVideo && src.FramePosition == 0)
			RenderFrame(transform, src, entityID);
		else if (!src.PlayVideo && src.FramePosition != 0)
			RenderCertainFrame(transform, src, entityID);
	}

	void VideoRenderer::ResetPacketDuration(VideoRendererComponent& src)
	{
		src.Video->ResetAudioPacketDuration(&src.Video->GetVideoState());
	}

}