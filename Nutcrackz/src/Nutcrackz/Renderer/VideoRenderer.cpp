#include "nzpch.h"
#include "VideoRenderer.h"

#include "Nutcrackz/Video/VideoTexture.h"

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"
#include "UniformBuffer.h"

namespace Nutcrackz {

	struct VideoVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		glm::vec2 TilingFactor;
		float TexIndex;
		float Saturation;

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

		glm::mat4 m_CameraView;
		CameraData CameraBuffer;
		Ref<UniformBuffer> CameraUniformBuffer;

		bool UseBillboard = false;
	};

	static VideoRendererData s_VideoData;

	void VideoRenderer::Init()
	{
		s_VideoData.VideoVertexArray = VertexArray::Create();

		s_VideoData.VideoVertexBuffer = VertexBuffer::Create(s_VideoData.MaxVertices * sizeof(VideoVertex));
		s_VideoData.VideoVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position"     },
			{ ShaderDataType::Float4, "a_Color"        },
			{ ShaderDataType::Float2, "a_TexCoord"     },
			{ ShaderDataType::Float2, "a_TilingFactor" },
			{ ShaderDataType::Float,  "a_TexIndex"     },
			{ ShaderDataType::Float,  "a_Saturation"   },
			{ ShaderDataType::Int,    "a_EntityID"     }
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
		s_VideoData.WhiteVideoTexture->SetData(Buffer(&whiteVideoTextureData, sizeof(uint32_t)));

		//s_VideoData.VideoShader = Shader::Create("assets/shaders/Renderer2D_Quad.glsl");
		s_VideoData.VideoShader = Shader::Create("assets/shaders/Renderer2D_Quad_BlackWhite.glsl");

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

		s_VideoData.m_CameraView = glm::inverse(transform);
		s_VideoData.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_VideoData.CameraUniformBuffer->SetData(&s_VideoData.CameraBuffer, sizeof(VideoRendererData::CameraData));

		StartBatch();
	}

	void VideoRenderer::BeginScene(const EditorCamera& camera)
	{
		//NZ_PROFILE_FUNCTION();

		s_VideoData.m_CameraView = camera.GetViewMatrix();
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
			RenderCommand::DrawIndexed(s_VideoData.VideoVertexArray, s_VideoData.VideoIndexCount, s_VideoData.UseBillboard);
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

	void VideoRenderer::RenderVideo(TransformComponent& transform, const Ref<VideoTexture>& texture, VideoRendererComponent& src, VideoData& data, int entityID)
	{
		NZ_CORE_VERIFY(texture);

		if (!data.HasInitializedTimer)
		{
			InitTimerWin32();
			data.HasInitializedTimer = true;
		}

		constexpr size_t videoVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }, };

		float textureIndex = 0.0f;
		const glm::vec2 tilingFactor(1.0f);

		// This if statement is very much required!
		// Without it, this function creates a big memory leak!
		if (data.VideoRendererID)
			texture->DeleteRendererID(data.VideoRendererID);

		if (texture)
		{
			if (!data.UseExternalAudio)
			{
				texture->ReadAndPlayAudio(&texture->GetVideoState(), data.FramePosition, data.SeekAudio, data.PauseVideo);
			}

			if (!data.IsRenderingVideo)
			{
				SetTime(0.0);
				data.IsRenderingVideo = true;
			}

			if (data.FramePosition != 0)
			{
				SetTime(data.FramePosition / texture->GetVideoState().Framerate);

				if (!texture->VideoReaderSeekFrame(&texture->GetVideoState(), data.FramePosition))
				{
					NZ_CORE_WARN("Could not seek video back to start frame!");
					return;
				}

				data.PresentationTimeStamp = data.FramePosition;
				data.FramePosition = 0;
			}

			data.VideoRendererID = texture->GetIDFromTexture(data.VideoFrameData, &data.PresentationTimeStamp, data.PauseVideo);
			texture->SetRendererID(data.VideoRendererID);

			if (data.PauseVideo)
			{
				if (!data.UseExternalAudio)
				{
					if (!texture->AVReaderSeekFrame(&texture->GetVideoState(), data.PresentationTimeStamp))
					{
						NZ_CORE_WARN("Could not seek a/v back to start frame!");
						return;
					}
				}
				else
				{
					if (!texture->VideoReaderSeekFrame(&texture->GetVideoState(), data.PresentationTimeStamp))
					{
						NZ_CORE_WARN("Could not seek video back to start frame!");
						return;
					}
				}

				SetTime(0.0);
			}
			else if (!data.PauseVideo)
			{
				if (data.RestartPointFromPause > GetTime())
					SetTime(data.RestartPointFromPause);

				if (data.RestartPointFromPause < GetTime())
					data.RestartPointFromPause = GetTime();

				data.PresentationTimeInSeconds = data.PresentationTimeStamp * ((double)texture->GetVideoState().TimeBase.num / (double)texture->GetVideoState().TimeBase.den);

				int hoursToSeconds = texture->GetVideoState().Hours * 3600;
				int minutesToSeconds = texture->GetVideoState().Mins * 60;

				if (data.VideoDuration != hoursToSeconds + minutesToSeconds + texture->GetVideoState().Secs + (0.01 * ((100 * texture->GetVideoState().Us) / AV_TIME_BASE)))
					data.VideoDuration = hoursToSeconds + minutesToSeconds + texture->GetVideoState().Secs + (0.01 * ((100 * texture->GetVideoState().Us) / AV_TIME_BASE));

				if (data.Hours != texture->GetVideoState().Hours)
					data.Hours = texture->GetVideoState().Hours;

				if (data.Minutes != texture->GetVideoState().Mins)
					data.Minutes = texture->GetVideoState().Mins;

				if (data.Seconds != texture->GetVideoState().Secs)
					data.Seconds = texture->GetVideoState().Secs;

				if (data.Milliseconds != texture->GetVideoState().Us)
					data.Milliseconds = texture->GetVideoState().Us;

				while (data.PresentationTimeInSeconds > GetTime())
				{
					Sleep(data.PresentationTimeInSeconds - GetTime());
				}

				if (data.RepeatVideo && GetTime() >= data.VideoDuration)
				{
					if (!data.UseExternalAudio)
					{
						if (!texture->AVReaderSeekFrame(&texture->GetVideoState(), 0, true))
						{
							NZ_CORE_WARN("Could not seek audio/video back to start frame!");
							return;
						}

						texture->CloseAudio(&texture->GetVideoState());
					}
					else
					{
						if (!texture->VideoReaderSeekFrame(&texture->GetVideoState(), 0))
						{
							NZ_CORE_WARN("Could not seek video back to start frame!");
							return;
						}
					}

					data.SeekAudio = true;
					texture->DeleteRendererID(data.VideoRendererID);
					texture->CloseVideo(&texture->GetVideoState());
					data.VideoRendererID = texture->GetIDFromTexture(data.VideoFrameData, &data.PresentationTimeStamp, data.PauseVideo);
					texture->SetRendererID(data.VideoRendererID);
					data.PresentationTimeStamp = 0;
					SetTime(0.0);
					data.RestartPointFromPause = 0.0;
				}
			}

			if (s_VideoData.VideoIndexCount >= VideoRendererData::MaxIndices)
				NextBatch();

			for (uint32_t i = 1; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (*s_VideoData.VideoTextureSlots[i] == *texture)
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

				if (texture)
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = texture;
				else
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = s_VideoData.WhiteVideoTexture;

				s_VideoData.VideoTextureSlotIndex++;
			}
		}

		if (data.UseBillboard)
		{
			glm::vec3 camRightWS = { s_VideoData.m_CameraView[0][0], s_VideoData.m_CameraView[1][0], s_VideoData.m_CameraView[2][0] };
			glm::vec3 camUpWS = { s_VideoData.m_CameraView[0][1], s_VideoData.m_CameraView[1][1], s_VideoData.m_CameraView[2][1] };
			glm::vec3 position = { transform.Translation.x, transform.Translation.y, transform.Translation.z };

			for (size_t i = 0; i < videoVertexCount; i++)
			{
				s_VideoData.VideoVertexBufferPtr->Position = position + camRightWS * (s_VideoData.QuadVertexPositions[i].x) * transform.Scale.x + camUpWS * s_VideoData.QuadVertexPositions[i].y * transform.Scale.y;
				s_VideoData.VideoVertexBufferPtr->Color = src.Color;
				s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
				s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
				s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
				s_VideoData.VideoVertexBufferPtr->Saturation = src.Saturation;
				s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
				s_VideoData.VideoVertexBufferPtr++;
			}
		}
		else
		{
			for (size_t i = 0; i < videoVertexCount; i++)
			{
				s_VideoData.VideoVertexBufferPtr->Position = transform.GetTransform() * s_VideoData.QuadVertexPositions[i];
				s_VideoData.VideoVertexBufferPtr->Color = src.Color;
				s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
				s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
				s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
				s_VideoData.VideoVertexBufferPtr->Saturation = src.Saturation;
				s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
				s_VideoData.VideoVertexBufferPtr++;
			}
		}

		s_VideoData.VideoIndexCount += 6;
	}

	void VideoRenderer::RenderFrame(TransformComponent& transform, const Ref<VideoTexture>& texture, VideoRendererComponent& src, VideoData& data, int entityID)
	{
		constexpr size_t videoVertexCount = 4;
		float textureIndex = 0.0f;

		const glm::vec2 tilingFactor(1.0f);

		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		if (texture)
		{
			if (data.FramePosition != data.FramePosition * texture->GetVideoState().VideoPacketDuration)
				data.FramePosition = data.FramePosition * texture->GetVideoState().VideoPacketDuration;

			if (texture->GetHasLoadedAudio())
			{
				if (!data.UseExternalAudio)
				{
					if (!texture->AVReaderSeekFrame(&texture->GetVideoState(), 0, true))
					{
						NZ_CORE_WARN("Could not seek a/v back to start frame!");
						return;
					}

					texture->CloseAudio(&texture->GetVideoState());
				}
				else
				{
					if (!texture->VideoReaderSeekFrame(&texture->GetVideoState(), 0))
					{
						NZ_CORE_WARN("Could not seek video back to start frame!");
						return;
					}
				}

				data.SeekAudio = true;
				texture->DeleteRendererID(data.VideoRendererID);
				texture->CloseVideo(&texture->GetVideoState());
				data.VideoRendererID = texture->GetIDFromTexture(data.VideoFrameData, &data.PresentationTimeStamp, data.PauseVideo);
				texture->SetRendererID(data.VideoRendererID);
				data.PresentationTimeStamp = 0;
				SetTime(0.0);
				data.RestartPointFromPause = 0.0;
			}

			if (data.IsRenderingVideo)
			{
				texture->DeleteRendererID(data.VideoRendererID);
				texture->CloseVideo(&texture->GetVideoState());
				data.VideoRendererID = texture->GetIDFromTexture(data.VideoFrameData, &data.PresentationTimeStamp, data.PauseVideo);
				texture->SetRendererID(data.VideoRendererID);
				data.PresentationTimeStamp = 0;
				SetTime(0.0);
				data.RestartPointFromPause = 0.0;
				data.IsRenderingVideo = false;
			}

			if (data.NumberOfFrames != texture->GetVideoState().NumberOfFrames)
				data.NumberOfFrames = texture->GetVideoState().NumberOfFrames;

			if (data.Hours != texture->GetVideoState().Hours)
				data.Hours = texture->GetVideoState().Hours;

			if (data.Minutes != texture->GetVideoState().Mins)
				data.Minutes = texture->GetVideoState().Mins;

			if (data.Seconds != texture->GetVideoState().Secs)
				data.Seconds = texture->GetVideoState().Secs;

			if (data.Milliseconds != texture->GetVideoState().Us)
				data.Milliseconds = texture->GetVideoState().Us;

			if (s_VideoData.VideoIndexCount >= VideoRendererData::MaxIndices)
				NextBatch();

			for (uint32_t i = 1; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (*s_VideoData.VideoTextureSlots[i] == *texture)
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

				if (texture)
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = texture;
				else
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = s_VideoData.WhiteVideoTexture;

				s_VideoData.VideoTextureSlotIndex++;
			}
		}

		if (data.UseBillboard)
		{
			glm::vec3 camRightWS = { s_VideoData.m_CameraView[0][0], s_VideoData.m_CameraView[1][0], s_VideoData.m_CameraView[2][0] };
			glm::vec3 camUpWS = { s_VideoData.m_CameraView[0][1], s_VideoData.m_CameraView[1][1], s_VideoData.m_CameraView[2][1] };
			glm::vec3 position = { transform.Translation.x, transform.Translation.y, transform.Translation.z };

			for (size_t i = 0; i < videoVertexCount; i++)
			{
				s_VideoData.VideoVertexBufferPtr->Position = position + camRightWS * (s_VideoData.QuadVertexPositions[i].x) * transform.Scale.x + camUpWS * s_VideoData.QuadVertexPositions[i].y * transform.Scale.y;
				s_VideoData.VideoVertexBufferPtr->Color = src.Color;
				s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
				s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
				s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
				s_VideoData.VideoVertexBufferPtr->Saturation = src.Saturation;
				s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
				s_VideoData.VideoVertexBufferPtr++;
			}
		}
		else
		{
			for (size_t i = 0; i < videoVertexCount; i++)
			{
				s_VideoData.VideoVertexBufferPtr->Position = transform.GetTransform() * s_VideoData.QuadVertexPositions[i];
				s_VideoData.VideoVertexBufferPtr->Color = src.Color;
				s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
				s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
				s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
				s_VideoData.VideoVertexBufferPtr->Saturation = src.Saturation;
				s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
				s_VideoData.VideoVertexBufferPtr++;
			}
		}

		s_VideoData.VideoIndexCount += 6;
	}

	void VideoRenderer::RenderCertainFrame(TransformComponent& transform, const Ref<VideoTexture>& texture, VideoRendererComponent& src, VideoData& data, int entityID)
	{
		NZ_CORE_VERIFY(texture);

		constexpr size_t videoVertexCount = 4;
		float textureIndex = 0.0f;

		const glm::vec2 tilingFactor(1.0f);

		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		if (texture)
		{
			if (data.NumberOfFrames != texture->GetVideoState().NumberOfFrames)
				data.NumberOfFrames = texture->GetVideoState().NumberOfFrames;

			if (data.Hours != texture->GetVideoState().Hours)
				data.Hours = texture->GetVideoState().Hours;

			if (data.Minutes != texture->GetVideoState().Mins)
				data.Minutes = texture->GetVideoState().Mins;

			if (data.Seconds != texture->GetVideoState().Secs)
				data.Seconds = texture->GetVideoState().Secs;

			if (data.Milliseconds != texture->GetVideoState().Us)
				data.Milliseconds = texture->GetVideoState().Us;

			if (data.FramePosition != data.FramePosition * texture->GetVideoState().VideoPacketDuration)
			{
				if (texture->GetHasLoadedAudio())
				{
					if (!data.UseExternalAudio)
					{
						texture->CloseAudio(&texture->GetVideoState());

						if (!texture->AVReaderSeekFrame(&texture->GetVideoState(), 0, true))
						{
							NZ_CORE_WARN("Could not seek a/v back to start frame!");
							return;
						}
					}

					data.SeekAudio = true;
					SetTime(0.0);
					data.RestartPointFromPause = 0.0;
				}

				if (data.IsRenderingVideo)
				{
					SetTime(0.0);
					data.RestartPointFromPause = 0.0;
					data.IsRenderingVideo = false;
				}

				data.FramePosition = data.FramePosition * texture->GetVideoState().VideoPacketDuration;

				if (!texture->VideoReaderSeekFrame(&texture->GetVideoState(), data.FramePosition))
				{
					NZ_CORE_WARN("Could not seek video back to start frame!");
					return;
				}

				data.PresentationTimeStamp = data.FramePosition;
				texture->DeleteRendererID(data.VideoRendererID);
				data.VideoRendererID = texture->GetIDFromTexture(data.VideoFrameData, &data.PresentationTimeStamp, data.PauseVideo);
				texture->SetRendererID(data.VideoRendererID);
			}

			if (s_VideoData.VideoIndexCount >= VideoRendererData::MaxIndices)
				NextBatch();

			for (uint32_t i = 1; i < s_VideoData.VideoTextureSlotIndex; i++)
			{
				if (*s_VideoData.VideoTextureSlots[i] == *texture)
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

				if (texture)
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = texture;
				else
					s_VideoData.VideoTextureSlots[s_VideoData.VideoTextureSlotIndex] = s_VideoData.WhiteVideoTexture;

				s_VideoData.VideoTextureSlotIndex++;
			}
		}

		if (data.UseBillboard)
		{
			glm::vec3 camRightWS = { s_VideoData.m_CameraView[0][0], s_VideoData.m_CameraView[1][0], s_VideoData.m_CameraView[2][0] };
			glm::vec3 camUpWS = { s_VideoData.m_CameraView[0][1], s_VideoData.m_CameraView[1][1], s_VideoData.m_CameraView[2][1] };
			glm::vec3 position = { transform.Translation.x, transform.Translation.y, transform.Translation.z };

			for (size_t i = 0; i < videoVertexCount; i++)
			{
				s_VideoData.VideoVertexBufferPtr->Position = position + camRightWS * (s_VideoData.QuadVertexPositions[i].x) * transform.Scale.x + camUpWS * s_VideoData.QuadVertexPositions[i].y * transform.Scale.y;
				s_VideoData.VideoVertexBufferPtr->Color = src.Color;
				s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
				s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
				s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
				s_VideoData.VideoVertexBufferPtr->Saturation = src.Saturation;
				s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
				s_VideoData.VideoVertexBufferPtr++;
			}
		}
		else
		{
			for (size_t i = 0; i < videoVertexCount; i++)
			{
				s_VideoData.VideoVertexBufferPtr->Position = transform.GetTransform() * s_VideoData.QuadVertexPositions[i];
				s_VideoData.VideoVertexBufferPtr->Color = src.Color;
				s_VideoData.VideoVertexBufferPtr->TexCoord = textureCoords[i];
				s_VideoData.VideoVertexBufferPtr->TilingFactor = tilingFactor;
				s_VideoData.VideoVertexBufferPtr->TexIndex = textureIndex;
				s_VideoData.VideoVertexBufferPtr->Saturation = src.Saturation;
				s_VideoData.VideoVertexBufferPtr->EntityID = entityID;
				s_VideoData.VideoVertexBufferPtr++;
			}
		}

		s_VideoData.VideoIndexCount += 6;
	}

	void VideoRenderer::DrawVideoSprite(TransformComponent& transform, VideoRendererComponent& src, VideoData& data, int entityID)
	{
		Ref<VideoTexture> texture = AssetManager::GetAsset<VideoTexture>(src.Video);
		if (data.PlayVideo)
			RenderVideo(transform, texture, src, data, entityID);
		else if (!data.PlayVideo && data.FramePosition == 0)
			RenderFrame(transform, texture, src, data, entityID);
		else if (!data.PlayVideo && data.FramePosition != 0)
			RenderCertainFrame(transform, texture, src, data, entityID);
	}

	void VideoRenderer::ResetPacketDuration(VideoRendererComponent& src)
	{
		Ref<VideoTexture> texture = AssetManager::GetAsset<VideoTexture>(src.Video);

		NZ_CORE_VERIFY(texture);

		texture->ResetAudioPacketDuration(&texture->GetVideoState());
	}

}