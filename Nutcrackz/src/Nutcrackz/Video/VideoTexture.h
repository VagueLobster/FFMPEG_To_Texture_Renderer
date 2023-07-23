#pragma once

#include "Nutcrackz/Core/Base.h"
#include "Nutcrackz/Renderer/Texture.h"
#include "Nutcrackz/Asset/Asset.h"

#include "miniaudio.h"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/error.h>
	#include <libavutil/avutil.h>
	#include <libswresample/swresample.h>
	#include <libavutil/audio_fifo.h>
}

#include <filesystem>

namespace Nutcrackz {

	struct VideoReaderState
	{
		int Width, Height;
		double Duration;
		int64_t VideoPacketDuration = 0;
		int64_t AudioPacketDuration = 0;
		int Hours, Mins, Secs, Us;
		double Framerate;
		int64_t NumberOfFrames;
		int VideoStreamIndex = -1;
		int AudioStreamIndex = -1;

		AVRational TimeBase;
		AVFormatContext* VideoFormatContext = nullptr;
		AVCodecContext* VideoCodecContext = nullptr;
		AVFrame* VideoFrame = nullptr;
		AVPacket* VideoPacket = nullptr;
		AVStream* VideoStream = nullptr;

		AVFormatContext* AudioFormatContext = nullptr;
		AVCodecContext* AudioCodecContext = nullptr;
		AVFrame* AudioFrame = nullptr;
		AVPacket* AudioPacket = nullptr;
		AVStream* AudioStream = nullptr;
		AVAudioFifo* AudioFifo = nullptr;
	};

	class VideoTexture : public Asset
	{
	public:
		VideoTexture(const TextureSpecification& specification);
		VideoTexture(const std::string& path, uint8_t* frameData);
		
		~VideoTexture();

		uint32_t GetIDFromTexture(uint8_t* frameData, int64_t* pts, bool isPaused);
		void DeleteRendererID(const uint32_t& rendererID);

		static bool VideoReaderOpen(VideoReaderState* state, const std::filesystem::path& filepath);
		bool VideoReaderReadFrame(VideoReaderState* state, uint8_t* frameBuffer, int64_t* pts, bool isPaused);
		bool VideoReaderSeekFrame(VideoReaderState* state, int64_t ts);
		static bool AudioReaderOpen(VideoReaderState* state, const std::filesystem::path& filepath);
		bool AudioReaderReadFrame(VideoReaderState* state, bool isPaused);
		bool AudioReaderSeekFrame(VideoReaderState* state, int64_t ts, bool resetAudio = false);
		bool AVReaderSeekFrame(VideoReaderState* state, int64_t ts, bool resetAudio = false);
		void PauseAudio(bool isPaused);
		void CloseVideo(VideoReaderState* state);
		void CloseAudio(VideoReaderState* state);

		void ReadAndPlayAudio(VideoReaderState* state, int64_t ts, bool seek, bool isPaused);
		void ResetAudioPacketDuration(VideoReaderState* state);

		static VideoReaderState GetVideoState();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		uint32_t GetRendererID() const { return m_RendererID; }

		void SetWidth(uint32_t width);
		void SetHeight(uint32_t height);
		void SetRendererID(uint32_t id);

		const std::string& GetVideoPath() const { return m_VideoPath; }
		void SetVideoPath(const std::string& path) { m_VideoPath = path; }

		bool IsLoaded() const { return m_IsLoaded; }
		void SetLinear(bool value) { m_Specification.UseLinear = value; }

		void SetData(void* data, uint32_t size);

		void Bind(uint32_t slot = 0) const;

		bool operator==(const VideoTexture& other) const
		{
			return m_RendererID == other.GetRendererID();
		}

		static Ref<VideoTexture> Create(const TextureSpecification& specification);
		static Ref<VideoTexture> Create(const std::string& path, uint8_t* frameData);

		static AssetType GetStaticType() { return AssetType::TextureVideo; }
		virtual AssetType GetType() const { return GetStaticType(); }

	private:
		TextureSpecification m_Specification;
		std::string m_VideoPath;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID = 0;
		uint32_t m_InternalFormat, m_DataFormat;

		bool m_IsLoaded = false;

		inline static VideoReaderState m_VideoState;
		inline static bool m_IsVideoLoaded = false;
		inline static bool m_HasLoadedAudio = false;

		inline static bool m_InitializedAudio = false;
		inline static bool m_AudioStopped = false;
		ma_device m_AudioDevice;
	};

}