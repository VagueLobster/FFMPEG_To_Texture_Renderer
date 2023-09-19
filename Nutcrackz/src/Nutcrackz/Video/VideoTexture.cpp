#include "nzpch.h"
#include "VideoTexture.h"

#include <glad/glad.h>

namespace Nutcrackz {

	namespace Utils {

		static AVPixelFormat CorrectForDeprecatedPixelFormat(AVPixelFormat pix_fmt)
		{
			// Fix swscaler deprecated pixel format warning
			// (YUVJ has been deprecated, change pixel format to regular YUV)
			switch (pix_fmt)
			{
				case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
				case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
				case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
				case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
			}

			return pix_fmt;
		}

		std::string GetAVError(int errnum)
		{
			char buffer[AV_ERROR_MAX_STRING_SIZE];
			std::string str = av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, errnum);
			return str;
		}

		ma_format FromFFmpegAudioToMiniAudioFormat(AVSampleFormat format)
		{
			switch (format)
			{
				case AV_SAMPLE_FMT_U8: return ma_format_u8;
				case AV_SAMPLE_FMT_U8P: return ma_format_u8;
				case AV_SAMPLE_FMT_S16: return ma_format_s16;
				case AV_SAMPLE_FMT_S16P: return ma_format_s16;
				case AV_SAMPLE_FMT_S32: return ma_format_s32;
				case AV_SAMPLE_FMT_S32P: return ma_format_s32;
				case AV_SAMPLE_FMT_S64: return ma_format_s32;
				case AV_SAMPLE_FMT_S64P: return ma_format_s32;
				case AV_SAMPLE_FMT_FLT: return ma_format_f32;
				case AV_SAMPLE_FMT_FLTP: return ma_format_f32;
				case AV_SAMPLE_FMT_DBL: return ma_format_f32;
				case AV_SAMPLE_FMT_DBLP: return ma_format_f32;
				case AV_SAMPLE_FMT_NONE: return ma_format_unknown;
			}

			return ma_format_unknown;
		}

		int FromFFmpegAudioToIntFormat(AVSampleFormat format)
		{
			switch (format)
			{
				case AV_SAMPLE_FMT_U8: return 0;
				case AV_SAMPLE_FMT_U8P: return 0;
				case AV_SAMPLE_FMT_S16: return 1;
				case AV_SAMPLE_FMT_S16P: return 1;
				case AV_SAMPLE_FMT_S32: return 2;
				case AV_SAMPLE_FMT_S32P: return 2;
				case AV_SAMPLE_FMT_S64: return 10;
				case AV_SAMPLE_FMT_S64P: return 10;
				case AV_SAMPLE_FMT_FLT: return 3;
				case AV_SAMPLE_FMT_FLTP: return 3;
				case AV_SAMPLE_FMT_DBL: return 4;
				case AV_SAMPLE_FMT_DBLP: return 4;
				case AV_SAMPLE_FMT_NONE: return -1;
			}

			return -1;
		}

	}

	VideoTexture::VideoTexture(const TextureSpecification& specification, Buffer data, const VideoReaderState& state)
		: m_Specification(specification), m_Width(m_Specification.Width), m_Height(m_Specification.Height), m_VideoState(state)
	{
		//NZ_PROFILE_FUNCTION();

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, GL_RGBA8, m_Width, m_Height);

		if (!m_Specification.UseLinear)
		{
			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else
		{
			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		if (data)
		{
			m_IsVideoLoaded = true;

			int64_t pts;
			if (!VideoReaderReadFrame(&m_VideoState, data.Data, &pts, false))
			{
				NZ_CORE_WARN("Couldn't load video frame!");
				return;
			}

			SetData(data);
		}
	}

	VideoTexture::~VideoTexture()
	{
		glDeleteTextures(1, &m_RendererID);

		if (m_IsVideoLoaded)
		{
			CloseVideo(&m_VideoState);
		}

		if (m_HasLoadedAudio)
		{
			CloseAudio(&m_VideoState);
		}
	}

	uint32_t VideoTexture::GetIDFromTexture(uint8_t* frameData, int64_t* pts, bool isPaused, const std::filesystem::path& filepath)
	{
		uint32_t rendererID = 0;

		if (!m_IsVideoLoaded)
		{
			if (!VideoReaderOpen(&m_VideoState, filepath))
			{
				NZ_CORE_WARN("Couldn't load video file!");
				return 0;
			}

			m_IsVideoLoaded = true;
		}

		if (m_IsVideoLoaded)
		{
			const int frameWidth = m_Width;
			const int frameHeight = m_Height;
			frameData = new uint8_t[frameWidth * frameHeight * 4];

			if (!VideoReaderReadFrame(&m_VideoState, frameData, pts, isPaused))
			{
				NZ_CORE_WARN("Couldn't load video frame!");
				return 0;
			}

			if (frameData)
			{
				glCreateTextures(GL_TEXTURE_2D, 1, &rendererID);
				glTextureStorage2D(rendererID, 1, GL_RGBA8, frameWidth, frameHeight);

				if (!m_Specification.UseLinear)
				{
					glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				}
				else
				{
					glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}

				glTextureParameteri(rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTextureParameteri(rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

				glTextureSubImage2D(rendererID, 0, 0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, frameData);
			}

			delete[] frameData;
			frameData = nullptr;

			m_RendererID = rendererID;
		}

		return rendererID;
	}

	void VideoTexture::DeleteRendererID(const uint32_t& rendererID)
	{
		glDeleteTextures(1, &rendererID);
	}

	bool VideoTexture::VideoReaderOpen(VideoReaderState* state, const std::filesystem::path& filepath)
	{
		// Unpack members of state
		auto& width = state->Width;
		auto& height = state->Height;
		auto& timeBase = state->TimeBase;
		auto& avFormatContext = state->VideoFormatContext;
		auto& avVideoCodecContext = state->VideoCodecContext;
		auto& videoStreamIndex = state->VideoStreamIndex;
		auto& videoStream = state->VideoStream;
		auto& avFrame = state->VideoFrame;
		auto& avPacket = state->VideoPacket;
		
		avFormatContext = avformat_alloc_context();

		if (!avFormatContext)
		{
			NZ_CORE_ERROR("Could not create AVFormatContext!");
			return false;
		}

		if (avformat_open_input(&avFormatContext, filepath.string().c_str(), NULL, NULL) < 0)
		{
			NZ_CORE_ERROR("Could not open video file: {0}", filepath.string().c_str());
			return false;
		}

		if (avformat_find_stream_info(avFormatContext, NULL) < 0)
		{
			NZ_CORE_ERROR("Could not find stream info!");
			return false;
		}

		if (avFormatContext->duration != AV_NOPTS_VALUE)
		{
			state->Duration = avFormatContext->duration + 5000;
			state->Secs = state->Duration / AV_TIME_BASE;
			state->Us = (int64_t)state->Duration % AV_TIME_BASE;
			state->Mins = state->Secs / 60;
			state->Secs %= 60;
			state->Hours = state->Mins / 60;
			state->Mins %= 60;
		}

		// Find the first valid video stream inside file!
		videoStreamIndex = -1;
		AVCodecParameters* avVideoCodecParams = nullptr;
		AVCodec* avVideoCodec = nullptr;

		for (unsigned int i = 0; i < avFormatContext->nb_streams; ++i)
		{
			avVideoCodecParams = avFormatContext->streams[i]->codecpar;
			avVideoCodec = (AVCodec*)avcodec_find_decoder(avVideoCodecParams->codec_id);

			if (!avVideoCodec)
			{
				continue;
			}

			if (avVideoCodecParams->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0)
			{
				videoStreamIndex = i;
				width = avVideoCodecParams->width;
				height = avVideoCodecParams->height;
				timeBase = avFormatContext->streams[i]->time_base;
				break;
			}
		}

		if (videoStreamIndex < 0)
		{
			NZ_CORE_ERROR("Could not find valid video stream inside file!");
			return false;
		}

		state->Framerate = av_q2d(avFormatContext->streams[videoStreamIndex]->r_frame_rate);

		int hoursToSeconds = state->Hours * 3600;
		int minutesToSeconds = state->Mins * 60;

		state->Duration = hoursToSeconds + minutesToSeconds + state->Secs + (0.01 * ((100 * state->Us) / AV_TIME_BASE));
		state->NumberOfFrames = state->Framerate * state->Duration;

		int numerator = avFormatContext->streams[videoStreamIndex]->r_frame_rate.num;
		int denominator = avFormatContext->streams[videoStreamIndex]->r_frame_rate.den;
		int avgNumerator = avFormatContext->streams[videoStreamIndex]->avg_frame_rate.num;
		int avgDenominator = avFormatContext->streams[videoStreamIndex]->avg_frame_rate.den;

		videoStream = avFormatContext->streams[videoStreamIndex];

		state->VideoPacketDuration = 0;

		// Set-up codec context for the decoder
		avVideoCodecContext = avcodec_alloc_context3(avVideoCodec);

		if (!avVideoCodecContext)
		{
			NZ_CORE_ERROR("Could not create avVideoCodecContext!");
			return false;
		}

		if (avcodec_parameters_to_context(avVideoCodecContext, avVideoCodecParams) < 0)
		{
			NZ_CORE_ERROR("Could not initialize avVideoCodecContext!");
			return false;
		}

		if (avcodec_open2(avVideoCodecContext, avVideoCodec, NULL) < 0)
		{
			NZ_CORE_ERROR("Could not open codec!");
			return false;
		}

		avFrame = av_frame_alloc();

		if (!avFrame)
		{
			NZ_CORE_ERROR("Could not allocate AVFrame!");
			return false;
		}

		state->VideoPacket = av_packet_alloc();

		if (!state->VideoPacket)
		{
			NZ_CORE_ERROR("Could not allocate AVPacket!");
			return false;
		}

		return true;
	}

	bool VideoTexture::VideoReaderReadFrame(VideoReaderState* state, uint8_t* frameBuffer, int64_t* pts, bool isPaused)
	{
		// Unpack members of state
		auto& width = state->Width;
		auto& height = state->Height;
		auto& avFormatContext = state->VideoFormatContext;
		auto& avCodecContext = state->VideoCodecContext;
		auto& videoStreamIndex = state->VideoStreamIndex;
		auto& videoStream = state->VideoStream;
		auto& avFrame = state->VideoFrame;
		auto& avPacket = state->VideoPacket;
		auto& timeBase = state->TimeBase;

		// Decode a single frame
		int response;
		if (avFormatContext != nullptr)
		{
			while (av_read_frame(avFormatContext, avPacket) >= 0)
			{
				av_packet_rescale_ts(avPacket, timeBase, timeBase);

				if (avPacket->stream_index != videoStreamIndex)
				{
					av_packet_unref(avPacket);
					continue;
				}

				response = avcodec_send_packet(avCodecContext, avPacket);

				if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode AVPacket: {0}!", Utils::GetAVError(response));
					return false;
				}

				response = avcodec_receive_frame(avCodecContext, avFrame);

				if (state->VideoPacketDuration != avFrame->duration)
					state->VideoPacketDuration = avFrame->duration;

				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					av_packet_unref(avPacket);
					continue;
				}
				else if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode AVPacket: {0}!", Utils::GetAVError(response));
					return false;
				}

				av_packet_unref(avPacket);
				break;
			}
		}

		if (!isPaused)
		{
			*pts = avFrame->pts;
		}

		SwsContext* swsScalerContext;
		auto srcPixelFormat = Utils::CorrectForDeprecatedPixelFormat(avCodecContext->pix_fmt);
		swsScalerContext = sws_getContext(width, height, srcPixelFormat, width, height, AV_PIX_FMT_RGB0, SWS_BILINEAR, NULL, NULL, NULL);

		if (!swsScalerContext)
		{
			NZ_CORE_ERROR("Could not initialize SW Scaler!");
			return false;
		}

		uint8_t* dstBuffer[4] = { frameBuffer, NULL, NULL, NULL };
		int dstLineSize[4] = { width * 4, 0, 0, 0 };
		sws_scale(swsScalerContext, avFrame->data, avFrame->linesize, 0, avFrame->height, dstBuffer, dstLineSize);

		sws_freeContext(swsScalerContext);

		return true;
	}

	bool VideoTexture::VideoReaderSeekFrame(VideoReaderState* state, int64_t ts)
	{
		// Unpack members of state
		auto& avFormatContext = state->VideoFormatContext;
		auto& avCodecContext = state->VideoCodecContext;
		auto& videoStreamIndex = state->VideoStreamIndex;
		auto& avPacket = state->VideoPacket;
		auto& avFrame = state->VideoFrame;
		auto& videoStream = state->VideoStream;
		auto& timeBase = state->TimeBase;

		int64_t videoPts = av_rescale_q(ts, timeBase, videoStream->time_base);
		av_seek_frame(avFormatContext, videoStreamIndex, videoPts, AVSEEK_FLAG_BACKWARD);

		avcodec_flush_buffers(avCodecContext);

		// av_seek_frame takes effect after one frame, so I'm decoding one here
		// so that the next call to video_reader_read_frame() will give the correct frame
		int response;
		if (avFormatContext != nullptr)
		{
			while (av_read_frame(avFormatContext, avPacket) >= 0)
			{
				if (avPacket->stream_index != videoStreamIndex)
				{
					av_packet_unref(avPacket);
					continue;
				}

				response = avcodec_send_packet(avCodecContext, avPacket);


				if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode AVPacket: {0}!", Utils::GetAVError(response));
					return false;
				}

				response = avcodec_receive_frame(avCodecContext, avFrame);

				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					av_packet_unref(avPacket);
					continue;
				}
				else if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode AVPacket: {0}!", Utils::GetAVError(response));
					return false;
				}

				av_packet_unref(avPacket);
				break;
			}
		}

		return true;
	}

	bool m_PauseAudio = false;
	void ffmpeg_to_miniaudio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
	{
		if (!m_PauseAudio)
		{
			AVAudioFifo* fifo = reinterpret_cast<AVAudioFifo*>(pDevice->pUserData);
			av_audio_fifo_read(fifo, &pOutput, frameCount);
		}
		else
		{
			size_t len = pDevice->playback.channels * frameCount;
			switch (pDevice->playback.format)
			{
				case ma_format_unknown: 0; break;
				case ma_format_u8: memset(pOutput, 127, len * 1); break;
				case ma_format_s16: memset(pOutput, 0, len * 2); break;
				case ma_format_s24: memset(pOutput, 0, len * 3); break;
				case ma_format_s32: memset(pOutput, 0, len * 4); break;
				case ma_format_f32: memset(pOutput, 0, len * 4); break;
			};
		}

		(void)pInput;
	}
	bool VideoTexture::AudioReaderOpen(VideoReaderState* state, const std::filesystem::path& filepath)
	{
		// Unpack members of state
		auto& avFormatContext = state->AudioFormatContext;
		auto& avCodecContext = state->AudioCodecContext;
		auto& audioStreamIndex = state->AudioStreamIndex;
		auto& avFrame = state->AudioFrame;
		auto& avPacket = state->AudioPacket;
		auto& audioStream = state->AudioStream;

		avFormatContext = avformat_alloc_context();

		if (!avFormatContext)
		{
			NZ_CORE_ERROR("Could not create AVFormatContext!");
			return false;
		}

		if (avformat_open_input(&avFormatContext, filepath.string().c_str(), NULL, NULL) < 0)
		{
			NZ_CORE_ERROR("Could not open video file: {0}", filepath.string().c_str());
			return false;
		}

		if (avformat_find_stream_info(avFormatContext, NULL) < 0)
		{
			NZ_CORE_ERROR("Could not find stream info!");
			return false;
		}

		// Find the first valid video stream inside file!
		audioStreamIndex = -1;
		AVCodecParameters* avAudioCodecParams = nullptr;
		AVCodec* avAudioCodec = nullptr;

		for (unsigned int i = 0; i < avFormatContext->nb_streams; ++i)
		{
			avAudioCodecParams = avFormatContext->streams[i]->codecpar;
			avAudioCodec = (AVCodec*)avcodec_find_decoder(avAudioCodecParams->codec_id);

			if (!avAudioCodec)
			{
				continue;
			}

			if (avAudioCodecParams->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0)
			{
				audioStreamIndex = i;
				break;
			}
		}

		if (audioStreamIndex < 0)
		{
			NZ_CORE_ERROR("Could not find valid audio stream inside file!");
			return false;
		}

		audioStream = avFormatContext->streams[audioStreamIndex];

		avCodecContext = avcodec_alloc_context3(avAudioCodec);

		if (!avCodecContext)
		{
			NZ_CORE_ERROR("Could not create avAudioCodecContext!");
			return false;
		}

		if (avcodec_parameters_to_context(avCodecContext, avAudioCodecParams) < 0)
		{
			NZ_CORE_ERROR("Could not initialize avAudioCodecContext!");
			return false;
		}

		if (avcodec_open2(avCodecContext, avAudioCodec, NULL) < 0)
		{
			NZ_CORE_ERROR("Could not open codec!");
			return false;
		}

		avFrame = av_frame_alloc();

		if (!avFrame)
		{
			NZ_CORE_ERROR("Could not allocate AVFrame!");
			return false;
		}

		state->AudioPacket = av_packet_alloc();

		if (!state->AudioPacket)
		{
			NZ_CORE_ERROR("Could not allocate AVPacket!");
			return false;
		}

		return true;
	}

	bool VideoTexture::AudioReaderReadFrame(VideoReaderState* state, bool isPaused)
	{
		// Unpack members of state
		auto& avFormatContext = state->AudioFormatContext;
		auto& avCodecContext = state->AudioCodecContext;
		auto& audioStreamIndex = state->AudioStreamIndex;
		auto& avFrame = state->AudioFrame;
		auto& avPacket = state->AudioPacket;
		auto& audioStream = state->AudioStream;
		auto& audioFifo = state->AudioFifo;

		AVSampleFormat sampleFormat = (AVSampleFormat)audioStream->codecpar->format;
		int outSampleFormat = Utils::FromFFmpegAudioToIntFormat(sampleFormat);

		// Initialize SwrContext
		SwrContext* swrContext = swr_alloc_set_opts(nullptr, audioStream->codecpar->channel_layout, (AVSampleFormat)outSampleFormat, audioStream->codecpar->sample_rate,
			audioStream->codecpar->channel_layout, sampleFormat, audioStream->codecpar->sample_rate, 0, nullptr);

		if (!swrContext)
		{
			NZ_CORE_ERROR("Could not create SwrContext.");
			return false;
		}

		if (swr_init(swrContext) < 0)
		{
			NZ_CORE_ERROR("Could not initialize SwrContext.");
			return false;
		}

		audioFifo = av_audio_fifo_alloc((AVSampleFormat)outSampleFormat, audioStream->codecpar->channels, 1);

		// Decode the video frame data
		int response;
		while (av_read_frame(avFormatContext, avPacket) >= 0)
		{
			if (avPacket->stream_index != audioStreamIndex)
			{
				av_packet_unref(avPacket);
				continue;
			}

			response = avcodec_send_packet(avCodecContext, avPacket);

			if (state->AudioPacketDuration != avPacket->duration)
				state->AudioPacketDuration = avPacket->duration;

			if (response < 0)
			{
				if (response != AVERROR(EAGAIN))
				{
					NZ_CORE_ERROR("Could not decode packet.");
					av_packet_unref(avPacket);
					continue;
				}
			}

			while ((response = avcodec_receive_frame(avCodecContext, avFrame)) == 0)
			{
				AVFrame* resampledFrame = av_frame_alloc();
				resampledFrame->sample_rate = avFrame->sample_rate;
				resampledFrame->channel_layout = avFrame->channel_layout;
				resampledFrame->channels = avFrame->channels;
				resampledFrame->format = outSampleFormat;

				response = swr_convert_frame(swrContext, resampledFrame, avFrame);

				av_frame_unref(avFrame);
				av_audio_fifo_write(audioFifo, (void**)resampledFrame->data, resampledFrame->nb_samples);
				av_frame_free(&resampledFrame);
			}
		}

		if (m_InitializedAudio)
		{
			ma_device_config deviceConfig;
			deviceConfig = ma_device_config_init(ma_device_type_playback);
			deviceConfig.playback.format = Utils::FromFFmpegAudioToMiniAudioFormat(sampleFormat);
			deviceConfig.playback.channels = audioStream->codecpar->channels;
			deviceConfig.sampleRate = audioStream->codecpar->sample_rate;
			deviceConfig.dataCallback = ffmpeg_to_miniaudio_callback;
			deviceConfig.pUserData = audioFifo;

			if (ma_device_init(NULL, &deviceConfig, &m_AudioDevice) != MA_SUCCESS)
			{
				NZ_CORE_ERROR("Failed to open playback device!");
				return false;
			}

			if (ma_device_start(&m_AudioDevice) != MA_SUCCESS)
			{
				NZ_CORE_ERROR("Failed to start playback device!");
				ma_device_uninit(&m_AudioDevice);
				return false;
			}

			m_AudioDevice.masterVolumeFactor = m_Volume / 100.0f;

			swr_free(&swrContext);

			m_InitializedAudio = false;
		}

		return true;
	}

	bool VideoTexture::AudioReaderSeekFrame(VideoReaderState* state, int64_t ts, bool resetAudio)
	{
		// Unpack members of state
		auto& audioFormatContext = state->AudioFormatContext;
		auto& audioCodecContext = state->AudioCodecContext;
		auto& audioStreamIndex = state->AudioStreamIndex;
		auto& audioPacket = state->AudioPacket;
		auto& audioFrame = state->AudioFrame;
		auto& audioStream = state->AudioStream;
		auto& timeBase = state->TimeBase;

		int64_t pts = av_rescale_q(ts, timeBase, audioStream->time_base);
		av_seek_frame(audioFormatContext, audioStreamIndex, pts, AVSEEK_FLAG_BACKWARD);

		avcodec_flush_buffers(audioCodecContext);

		// av_seek_frame takes effect after one frame, so I'm decoding one here
		// so that the next call to video_reader_read_frame() will give the correct frame
		int response;
		if (audioFormatContext != nullptr)
		{
			while (av_read_frame(audioFormatContext, audioPacket) >= 0)
			{
				if (audioPacket->stream_index != audioStreamIndex)
				{
					av_packet_unref(audioPacket);
					continue;
				}

				response = avcodec_send_packet(audioCodecContext, audioPacket);
				if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode audio packet: {0}!", Utils::GetAVError(response));
					return false;
				}

				response = avcodec_receive_frame(audioCodecContext, audioFrame);

				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					av_packet_unref(audioPacket);
					continue;
				}
				else if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode audio packet: {0}!", Utils::GetAVError(response));
					return false;
				}

				av_packet_unref(audioPacket);
				break;
			}

			if (resetAudio && !m_InitializedAudio)
				m_InitializedAudio = true;
		}

		return true;
	}

	bool VideoTexture::AVReaderSeekFrame(VideoReaderState* state, int64_t ts, bool resetAudio)
	{
		// Unpack video members of state
		auto& videoFormatContext = state->VideoFormatContext;
		auto& videoCodecContext = state->VideoCodecContext;
		auto& videoStreamIndex = state->VideoStreamIndex;
		auto& videoPacket = state->VideoPacket;
		auto& videoFrame = state->VideoFrame;
		auto& videoStream = state->VideoStream;

		// Unpack members of state
		auto& audioFormatContext = state->AudioFormatContext;
		auto& audioCodecContext = state->AudioCodecContext;
		auto& audioStreamIndex = state->AudioStreamIndex;
		auto& audioPacket = state->AudioPacket;
		auto& audioFrame = state->AudioFrame;
		auto& audioStream = state->AudioStream;
		auto& timeBase = state->TimeBase;

		int64_t videoPts = av_rescale_q(ts, timeBase, videoStream->time_base);
		av_seek_frame(videoFormatContext, videoStreamIndex, videoPts, AVSEEK_FLAG_BACKWARD);

		avcodec_flush_buffers(videoCodecContext);

		// av_seek_frame takes effect after one frame, so I'm decoding one here
		// so that the next call to video_reader_read_frame() will give the correct frame
		int response;
		if (videoFormatContext != nullptr)
		{
			while (av_read_frame(videoFormatContext, videoPacket) >= 0)
			{
				if (videoPacket->stream_index != videoStreamIndex)
				{
					av_packet_unref(videoPacket);
					continue;
				}

				response = avcodec_send_packet(videoCodecContext, videoPacket);
				if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode video packet: {0}!", Utils::GetAVError(response));
					return false;
				}

				response = avcodec_receive_frame(videoCodecContext, videoFrame);
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					av_packet_unref(videoPacket);
					continue;
				}
				else if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode video packet: {0}!", Utils::GetAVError(response));
					return false;
				}

				av_packet_unref(videoPacket);
				break;
			}
		}

		int64_t audioPts = av_rescale_q(ts, timeBase, audioStream->time_base);
		av_seek_frame(audioFormatContext, audioStreamIndex, audioPts, AVSEEK_FLAG_BACKWARD);

		avcodec_flush_buffers(audioCodecContext);

		// av_seek_frame takes effect after one frame, so I'm decoding one here
		// so that the next call to video_reader_read_frame() will give the correct frame
		if (audioFormatContext != nullptr)
		{
			while (av_read_frame(audioFormatContext, audioPacket) >= 0)
			{
				if (audioPacket->stream_index != audioStreamIndex)
				{
					av_packet_unref(audioPacket);
					continue;
				}

				response = avcodec_send_packet(audioCodecContext, audioPacket);
				if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode audio packet: {0}!", Utils::GetAVError(response));
					return false;
				}

				response = avcodec_receive_frame(audioCodecContext, audioFrame);

				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					av_packet_unref(audioPacket);
					continue;
				}
				else if (response < 0)
				{
					NZ_CORE_ERROR("Failed to decode audio packet: {0}!", Utils::GetAVError(response));
					return false;
				}

				av_packet_unref(audioPacket);
				break;
			}

			if (resetAudio && !m_InitializedAudio)
				m_InitializedAudio = true;
		}

		return true;
	}

	void VideoTexture::PauseAudio(bool isPaused)
	{
		if (m_PauseAudio != isPaused)
			m_PauseAudio = isPaused;
	}

	void VideoTexture::CloseVideo(VideoReaderState* state)
	{
		if (m_IsVideoLoaded)
		{
			if (state->VideoFormatContext)
				avformat_close_input(&state->VideoFormatContext);

			if (state->VideoFormatContext)
				avformat_free_context(state->VideoFormatContext);

			if (state->VideoFrame)
				av_frame_free(&state->VideoFrame);

			if (state->VideoPacket)
				av_packet_free(&state->VideoPacket);

			if (state->VideoCodecContext)
				avcodec_free_context(&state->VideoCodecContext);

			state->VideoStreamIndex = -1;

			delete state->VideoFormatContext;
			state->VideoFormatContext = nullptr;
			
			delete state->VideoFrame;
			state->VideoFrame = nullptr;
			
			delete state->VideoPacket;
			state->VideoPacket = nullptr;

			delete state->VideoCodecContext;
			state->VideoCodecContext = nullptr;

			state->VideoStream = nullptr;

			m_IsVideoLoaded = false;
		}
	}

	void VideoTexture::CloseAudio(VideoReaderState* state)
	{
		if (ma_device_stop(&m_AudioDevice) != MA_SUCCESS)
		{
			NZ_CORE_ERROR("Failed to stop playback device!");
			ma_device_uninit(&m_AudioDevice);
			return;
		}

		if (m_HasLoadedAudio)
		{
			avformat_close_input(&state->AudioFormatContext);
			av_frame_free(&state->AudioFrame);
			av_packet_free(&state->AudioPacket);
			avcodec_free_context(&state->AudioCodecContext);

			av_audio_fifo_free(state->AudioFifo);
			ma_device_uninit(&m_AudioDevice);

			state->AudioStreamIndex = -1;

			delete state->AudioFormatContext;
			state->AudioFormatContext = nullptr;

			delete state->AudioFrame;
			state->AudioFrame = nullptr;

			delete state->AudioPacket;
			state->AudioPacket = nullptr;

			delete state->AudioCodecContext;
			state->AudioCodecContext = nullptr;

			state->AudioStream = nullptr;
			state->AudioFifo = nullptr;

			m_HasLoadedAudio = false;
		}
	}

	void VideoTexture::ReadAndPlayAudio(VideoReaderState* state, int64_t ts, bool seek, bool isPaused, const std::filesystem::path& filepath)
	{
		if (!m_HasLoadedAudio)
		{
			if (!AudioReaderOpen(&m_VideoState, filepath))
			{
				NZ_CORE_WARN("Couldn't load video file!");
				return;
			}

			m_InitializedAudio = true;
			m_HasLoadedAudio = true;
		}

		if (m_InitializedAudio)
		{
			if (seek)
			{
				if (!AudioReaderSeekFrame(&m_VideoState, ts))
				{
					NZ_CORE_WARN("Could not seek the audio back to start frame!");
					return;
				}

				seek = false;
			}

			if (!AudioReaderReadFrame(&m_VideoState, isPaused))
			{
				NZ_CORE_WARN("Couldn't load audio frame!");
				return;
			}
		}

		PauseAudio(isPaused);
	}

	void VideoTexture::ResetAudioPacketDuration(VideoReaderState* state)
	{
		state->AudioPacketDuration = 0;
	}

	void VideoTexture::SetVolumeFactor(float volume)
	{
		m_Volume = volume;
		m_AudioDevice.masterVolumeFactor = m_Volume / 100.0f;
	}

	void VideoTexture::SetWidth(uint32_t width)
	{
		m_Width = width;
	}

	void VideoTexture::SetHeight(uint32_t height)
	{
		m_Height = height;
	}

	void VideoTexture::SetRendererID(uint32_t id)
	{
		m_RendererID = id;
	}

	void VideoTexture::SetData(Buffer data)
	{
		//NZ_PROFILE_FUNCTION();

		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, data.Data);
	}

	void VideoTexture::Bind(uint32_t slot, bool isPlayingVideo)// const
	{
		//NZ_PROFILE_FUNCTION();

		glBindTextureUnit(slot, m_RendererID);
	}

	Ref<VideoTexture> VideoTexture::Create(const TextureSpecification& specification, Buffer data, const VideoReaderState& state)
	{
		return CreateRef<VideoTexture>(specification, data, state);
	}

}