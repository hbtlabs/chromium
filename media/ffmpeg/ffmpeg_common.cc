// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg/ffmpeg_common.h"

#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_util.h"
#include "media/media_features.h"

namespace media {

namespace {

EncryptionScheme GetEncryptionScheme(const AVStream* stream) {
  AVDictionaryEntry* key =
      av_dict_get(stream->metadata, "enc_key_id", nullptr, 0);
  return key ? AesCtrEncryptionScheme() : Unencrypted();
}

}  // namespace

// Why FF_INPUT_BUFFER_PADDING_SIZE? FFmpeg assumes all input buffers are
// padded. Check here to ensure FFmpeg only receives data padded to its
// specifications.
static_assert(DecoderBuffer::kPaddingSize >= FF_INPUT_BUFFER_PADDING_SIZE,
              "DecoderBuffer padding size does not fit ffmpeg requirement");

// Alignment requirement by FFmpeg for input and output buffers. This need to
// be updated to match FFmpeg when it changes.
#if defined(ARCH_CPU_ARM_FAMILY)
static const int kFFmpegBufferAddressAlignment = 16;
#else
static const int kFFmpegBufferAddressAlignment = 32;
#endif

// Check here to ensure FFmpeg only receives data aligned to its specifications.
static_assert(
    DecoderBuffer::kAlignmentSize >= kFFmpegBufferAddressAlignment &&
    DecoderBuffer::kAlignmentSize % kFFmpegBufferAddressAlignment == 0,
    "DecoderBuffer alignment size does not fit ffmpeg requirement");

// Allows faster SIMD YUV convert. Also, FFmpeg overreads/-writes occasionally.
// See video_get_buffer() in libavcodec/utils.c.
static const int kFFmpegOutputBufferPaddingSize = 16;

static_assert(VideoFrame::kFrameSizePadding >= kFFmpegOutputBufferPaddingSize,
              "VideoFrame padding size does not fit ffmpeg requirement");

static_assert(
    VideoFrame::kFrameAddressAlignment >= kFFmpegBufferAddressAlignment &&
    VideoFrame::kFrameAddressAlignment % kFFmpegBufferAddressAlignment == 0,
    "VideoFrame frame address alignment does not fit ffmpeg requirement");

static const AVRational kMicrosBase = { 1, base::Time::kMicrosecondsPerSecond };

base::TimeDelta ConvertFromTimeBase(const AVRational& time_base,
                                    int64_t timestamp) {
  int64_t microseconds = av_rescale_q(timestamp, time_base, kMicrosBase);
  return base::TimeDelta::FromMicroseconds(microseconds);
}

int64_t ConvertToTimeBase(const AVRational& time_base,
                          const base::TimeDelta& timestamp) {
  return av_rescale_q(timestamp.InMicroseconds(), kMicrosBase, time_base);
}

AudioCodec CodecIDToAudioCodec(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_AAC:
      return kCodecAAC;
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    case AV_CODEC_ID_AC3:
      return kCodecAC3;
    case AV_CODEC_ID_EAC3:
      return kCodecEAC3;
#endif
    case AV_CODEC_ID_MP3:
      return kCodecMP3;
    case AV_CODEC_ID_VORBIS:
      return kCodecVorbis;
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_F32LE:
      return kCodecPCM;
    case AV_CODEC_ID_PCM_S16BE:
      return kCodecPCM_S16BE;
    case AV_CODEC_ID_PCM_S24BE:
      return kCodecPCM_S24BE;
    case AV_CODEC_ID_FLAC:
      return kCodecFLAC;
    case AV_CODEC_ID_AMR_NB:
      return kCodecAMR_NB;
    case AV_CODEC_ID_AMR_WB:
      return kCodecAMR_WB;
    case AV_CODEC_ID_GSM_MS:
      return kCodecGSM_MS;
    case AV_CODEC_ID_PCM_ALAW:
      return kCodecPCM_ALAW;
    case AV_CODEC_ID_PCM_MULAW:
      return kCodecPCM_MULAW;
    case AV_CODEC_ID_OPUS:
      return kCodecOpus;
    case AV_CODEC_ID_ALAC:
      return kCodecALAC;
    default:
      DVLOG(1) << "Unknown audio CodecID: " << codec_id;
  }
  return kUnknownAudioCodec;
}

AVCodecID AudioCodecToCodecID(AudioCodec audio_codec,
                              SampleFormat sample_format) {
  switch (audio_codec) {
    case kCodecAAC:
      return AV_CODEC_ID_AAC;
    case kCodecALAC:
      return AV_CODEC_ID_ALAC;
    case kCodecMP3:
      return AV_CODEC_ID_MP3;
    case kCodecPCM:
      switch (sample_format) {
        case kSampleFormatU8:
          return AV_CODEC_ID_PCM_U8;
        case kSampleFormatS16:
          return AV_CODEC_ID_PCM_S16LE;
        case kSampleFormatS24:
          return AV_CODEC_ID_PCM_S24LE;
        case kSampleFormatS32:
          return AV_CODEC_ID_PCM_S32LE;
        case kSampleFormatF32:
          return AV_CODEC_ID_PCM_F32LE;
        default:
          DVLOG(1) << "Unsupported sample format: " << sample_format;
      }
      break;
    case kCodecPCM_S16BE:
      return AV_CODEC_ID_PCM_S16BE;
    case kCodecPCM_S24BE:
      return AV_CODEC_ID_PCM_S24BE;
    case kCodecVorbis:
      return AV_CODEC_ID_VORBIS;
    case kCodecFLAC:
      return AV_CODEC_ID_FLAC;
    case kCodecAMR_NB:
      return AV_CODEC_ID_AMR_NB;
    case kCodecAMR_WB:
      return AV_CODEC_ID_AMR_WB;
    case kCodecGSM_MS:
      return AV_CODEC_ID_GSM_MS;
    case kCodecPCM_ALAW:
      return AV_CODEC_ID_PCM_ALAW;
    case kCodecPCM_MULAW:
      return AV_CODEC_ID_PCM_MULAW;
    case kCodecOpus:
      return AV_CODEC_ID_OPUS;
    default:
      DVLOG(1) << "Unknown AudioCodec: " << audio_codec;
  }
  return AV_CODEC_ID_NONE;
}

// Converts an FFmpeg video codec ID into its corresponding supported codec id.
static VideoCodec CodecIDToVideoCodec(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      return kCodecH264;
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case AV_CODEC_ID_HEVC:
      return kCodecHEVC;
#endif
    case AV_CODEC_ID_THEORA:
      return kCodecTheora;
    case AV_CODEC_ID_MPEG4:
      return kCodecMPEG4;
    case AV_CODEC_ID_VP8:
      return kCodecVP8;
    case AV_CODEC_ID_VP9:
      return kCodecVP9;
    default:
      DVLOG(1) << "Unknown video CodecID: " << codec_id;
  }
  return kUnknownVideoCodec;
}

AVCodecID VideoCodecToCodecID(VideoCodec video_codec) {
  switch (video_codec) {
    case kCodecH264:
      return AV_CODEC_ID_H264;
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case kCodecHEVC:
      return AV_CODEC_ID_HEVC;
#endif
    case kCodecTheora:
      return AV_CODEC_ID_THEORA;
    case kCodecMPEG4:
      return AV_CODEC_ID_MPEG4;
    case kCodecVP8:
      return AV_CODEC_ID_VP8;
    case kCodecVP9:
      return AV_CODEC_ID_VP9;
    default:
      DVLOG(1) << "Unknown VideoCodec: " << video_codec;
  }
  return AV_CODEC_ID_NONE;
}

static VideoCodecProfile ProfileIDToVideoCodecProfile(int profile) {
  // Clear out the CONSTRAINED & INTRA flags which are strict subsets of the
  // corresponding profiles with which they're used.
  profile &= ~FF_PROFILE_H264_CONSTRAINED;
  profile &= ~FF_PROFILE_H264_INTRA;
  switch (profile) {
    case FF_PROFILE_H264_BASELINE:
      return H264PROFILE_BASELINE;
    case FF_PROFILE_H264_MAIN:
      return H264PROFILE_MAIN;
    case FF_PROFILE_H264_EXTENDED:
      return H264PROFILE_EXTENDED;
    case FF_PROFILE_H264_HIGH:
      return H264PROFILE_HIGH;
    case FF_PROFILE_H264_HIGH_10:
      return H264PROFILE_HIGH10PROFILE;
    case FF_PROFILE_H264_HIGH_422:
      return H264PROFILE_HIGH422PROFILE;
    case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    default:
      DVLOG(1) << "Unknown profile id: " << profile;
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

static int VideoCodecProfileToProfileID(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return FF_PROFILE_H264_BASELINE;
    case H264PROFILE_MAIN:
      return FF_PROFILE_H264_MAIN;
    case H264PROFILE_EXTENDED:
      return FF_PROFILE_H264_EXTENDED;
    case H264PROFILE_HIGH:
      return FF_PROFILE_H264_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return FF_PROFILE_H264_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return FF_PROFILE_H264_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return FF_PROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      DVLOG(1) << "Unknown VideoCodecProfile: " << profile;
  }
  return FF_PROFILE_UNKNOWN;
}

SampleFormat AVSampleFormatToSampleFormat(AVSampleFormat sample_format,
                                          AVCodecID codec_id) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
      return kSampleFormatU8;
    case AV_SAMPLE_FMT_S16:
      return kSampleFormatS16;
    case AV_SAMPLE_FMT_S32:
      if (codec_id == AV_CODEC_ID_PCM_S24LE)
        return kSampleFormatS24;
      else
        return kSampleFormatS32;
    case AV_SAMPLE_FMT_FLT:
      return kSampleFormatF32;
    case AV_SAMPLE_FMT_S16P:
      return kSampleFormatPlanarS16;
    case  AV_SAMPLE_FMT_S32P:
      return kSampleFormatPlanarS32;
    case AV_SAMPLE_FMT_FLTP:
      return kSampleFormatPlanarF32;
    default:
      DVLOG(1) << "Unknown AVSampleFormat: " << sample_format;
  }
  return kUnknownSampleFormat;
}

static AVSampleFormat SampleFormatToAVSampleFormat(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatU8:
      return AV_SAMPLE_FMT_U8;
    case kSampleFormatS16:
      return AV_SAMPLE_FMT_S16;
    // pcm_s24le is treated as a codec with sample format s32 in ffmpeg
    case kSampleFormatS24:
    case kSampleFormatS32:
      return AV_SAMPLE_FMT_S32;
    case kSampleFormatF32:
      return AV_SAMPLE_FMT_FLT;
    case kSampleFormatPlanarS16:
      return AV_SAMPLE_FMT_S16P;
    case kSampleFormatPlanarF32:
      return AV_SAMPLE_FMT_FLTP;
    default:
      DVLOG(1) << "Unknown SampleFormat: " << sample_format;
  }
  return AV_SAMPLE_FMT_NONE;
}

bool AVCodecContextToAudioDecoderConfig(
    const AVCodecContext* codec_context,
    const EncryptionScheme& encryption_scheme,
    AudioDecoderConfig* config) {
  DCHECK_EQ(codec_context->codec_type, AVMEDIA_TYPE_AUDIO);

  AudioCodec codec = CodecIDToAudioCodec(codec_context->codec_id);

  SampleFormat sample_format = AVSampleFormatToSampleFormat(
      codec_context->sample_fmt, codec_context->codec_id);

  ChannelLayout channel_layout = ChannelLayoutToChromeChannelLayout(
      codec_context->channel_layout, codec_context->channels);

  int sample_rate = codec_context->sample_rate;
  switch (codec) {
    case kCodecOpus:
      // |codec_context->sample_fmt| is not set by FFmpeg because Opus decoding
      // is not enabled in FFmpeg.  It doesn't matter what value is set here, so
      // long as it's valid, the true sample format is selected inside the
      // decoder.
      sample_format = kSampleFormatF32;

      // Always use 48kHz for OPUS.  Technically we should match to the highest
      // supported hardware sample rate among [8, 12, 16, 24, 48] kHz, but we
      // don't know the hardware sample rate at this point and those rates are
      // rarely used for output.  See the "Input Sample Rate" section of the
      // spec: http://tools.ietf.org/html/draft-terriberry-oggopus-01#page-11
      sample_rate = 48000;
      break;

    // For AC3/EAC3 we enable only demuxing, but not decoding, so FFmpeg does
    // not fill |sample_fmt|.
    case kCodecAC3:
    case kCodecEAC3:
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
      // The spec for AC3/EAC3 audio is ETSI TS 102 366. According to sections
      // F.3.1 and F.5.1 in that spec the sample_format for AC3/EAC3 must be 16.
      sample_format = kSampleFormatS16;
#else
      NOTREACHED();
#endif
      break;

    default:
      break;
  }

  base::TimeDelta seek_preroll;
  if (codec_context->seek_preroll > 0) {
    seek_preroll = base::TimeDelta::FromMicroseconds(
        codec_context->seek_preroll * 1000000.0 / codec_context->sample_rate);
  }

  // AVStream occasionally has invalid extra data. See http://crbug.com/517163
  if ((codec_context->extradata_size == 0) !=
      (codec_context->extradata == nullptr)) {
    LOG(ERROR) << __func__
               << (codec_context->extradata == nullptr ? " NULL" : " Non-NULL")
               << " extra data cannot have size of "
               << codec_context->extradata_size << ".";
    return false;
  }

  std::vector<uint8_t> extra_data;
  if (codec_context->extradata_size > 0) {
    extra_data.assign(codec_context->extradata,
                      codec_context->extradata + codec_context->extradata_size);
  }

  config->Initialize(codec, sample_format, channel_layout, sample_rate,
                     extra_data, encryption_scheme, seek_preroll,
                     codec_context->delay);

  // Verify that AudioConfig.bits_per_channel was calculated correctly for
  // codecs that have |sample_fmt| set by FFmpeg.
  switch (codec) {
    case kCodecOpus:
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    case kCodecAC3:
    case kCodecEAC3:
#endif
      break;
    default:
      DCHECK_EQ(av_get_bytes_per_sample(codec_context->sample_fmt) * 8,
                config->bits_per_channel());
      break;
  }

  return true;
}

std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext>
AVStreamToAVCodecContext(const AVStream* stream) {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      avcodec_alloc_context3(nullptr));
  if (avcodec_parameters_to_context(codec_context.get(), stream->codecpar) <
      0) {
    return nullptr;
  }

  return codec_context;
}

bool AVStreamToAudioDecoderConfig(const AVStream* stream,
                                  AudioDecoderConfig* config) {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      AVStreamToAVCodecContext(stream));
  if (!codec_context)
    return false;

  return AVCodecContextToAudioDecoderConfig(
      codec_context.get(), GetEncryptionScheme(stream), config);
}

void AudioDecoderConfigToAVCodecContext(const AudioDecoderConfig& config,
                                        AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = AudioCodecToCodecID(config.codec(),
                                                config.sample_format());
  codec_context->sample_fmt = SampleFormatToAVSampleFormat(
      config.sample_format());

  // TODO(scherkus): should we set |channel_layout|? I'm not sure if FFmpeg uses
  // said information to decode.
  codec_context->channels =
      ChannelLayoutToChannelCount(config.channel_layout());
  codec_context->sample_rate = config.samples_per_second();

  if (config.extra_data().empty()) {
    codec_context->extradata = nullptr;
    codec_context->extradata_size = 0;
  } else {
    codec_context->extradata_size = config.extra_data().size();
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data().size() + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, &config.extra_data()[0],
           config.extra_data().size());
    memset(codec_context->extradata + config.extra_data().size(), '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  }
}

bool AVStreamToVideoDecoderConfig(const AVStream* stream,
                                  VideoDecoderConfig* config) {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      AVStreamToAVCodecContext(stream));
  if (!codec_context)
    return false;

  // AVStream.codec->coded_{width,height} access is deprecated in ffmpeg.
  // Use just the width and height as hints of coded size.
  gfx::Size coded_size(codec_context->width, codec_context->height);

  // TODO(vrk): This assumes decoded frame data starts at (0, 0), which is true
  // for now, but may not always be true forever. Fix this in the future.
  gfx::Rect visible_rect(codec_context->width, codec_context->height);

  AVRational aspect_ratio = { 1, 1 };
  if (stream->sample_aspect_ratio.num)
    aspect_ratio = stream->sample_aspect_ratio;
  else if (codec_context->sample_aspect_ratio.num)
    aspect_ratio = codec_context->sample_aspect_ratio;

  VideoCodec codec = CodecIDToVideoCodec(codec_context->codec_id);

  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  if (codec == kCodecVP8)
    profile = VP8PROFILE_ANY;
  else if (codec == kCodecVP9)
    // TODO(servolk): Find a way to obtain actual VP9 profile from FFmpeg.
    // crbug.com/592074
    profile = VP9PROFILE_PROFILE0;
  else
    profile = ProfileIDToVideoCodecProfile(codec_context->profile);

  // Without the FFmpeg h264 decoder, AVFormat is unable to get the profile, so
  // default to baseline and let the VDA fail later if it doesn't support the
  // real profile. This is alright because if the FFmpeg h264 decoder isn't
  // enabled, there is no fallback if the VDA fails.
#if defined(DISABLE_FFMPEG_VIDEO_DECODERS)
  if (codec == kCodecH264)
    profile = H264PROFILE_BASELINE;
#endif

  gfx::Size natural_size = GetNaturalSize(
      visible_rect.size(), aspect_ratio.num, aspect_ratio.den);

  VideoPixelFormat format =
      AVPixelFormatToVideoPixelFormat(codec_context->pix_fmt);
  // The format and coded size may be unknown if FFmpeg is compiled without
  // video decoders.
#if defined(DISABLE_FFMPEG_VIDEO_DECODERS)
  if (format == PIXEL_FORMAT_UNKNOWN)
    format = PIXEL_FORMAT_YV12;
  if (coded_size == gfx::Size(0, 0))
    coded_size = visible_rect.size();
#endif

  if (codec == kCodecVP9) {
    // TODO(tomfinegan): libavcodec doesn't know about VP9.
    format = PIXEL_FORMAT_YV12;
    coded_size = visible_rect.size();
  }

  // Pad out |coded_size| for subsampled YUV formats.
  if (format != PIXEL_FORMAT_YV24) {
    coded_size.set_width((coded_size.width() + 1) / 2 * 2);
    if (format != PIXEL_FORMAT_YV16)
      coded_size.set_height((coded_size.height() + 1) / 2 * 2);
  }

  AVDictionaryEntry* webm_alpha =
      av_dict_get(stream->metadata, "alpha_mode", nullptr, 0);
  if (webm_alpha && !strcmp(webm_alpha->value, "1")) {
    format = PIXEL_FORMAT_YV12A;
  }

  // Prefer the color space found by libavcodec if available.
  ColorSpace color_space = AVColorSpaceToColorSpace(codec_context->colorspace,
                                                    codec_context->color_range);
  if (color_space == COLOR_SPACE_UNSPECIFIED) {
    // Otherwise, assume that SD video is usually Rec.601, and HD is usually
    // Rec.709.
    color_space = (natural_size.height() < 720) ? COLOR_SPACE_SD_REC601
                                                : COLOR_SPACE_HD_REC709;
  }

  // AVCodecContext occasionally has invalid extra data. See
  // http://crbug.com/517163
  if (codec_context->extradata != nullptr &&
      codec_context->extradata_size == 0) {
    LOG(ERROR) << __func__ << " Non-Null extra data cannot have size of 0.";
    return false;
  }
  CHECK_EQ(codec_context->extradata == nullptr,
           codec_context->extradata_size == 0);

  std::vector<uint8_t> extra_data;
  if (codec_context->extradata_size > 0) {
    extra_data.assign(codec_context->extradata,
                      codec_context->extradata + codec_context->extradata_size);
  }
  config->Initialize(codec, profile, format, color_space, coded_size,
                     visible_rect, natural_size, extra_data,
                     GetEncryptionScheme(stream));
  return true;
}

void VideoDecoderConfigToAVCodecContext(
    const VideoDecoderConfig& config,
    AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context->codec_id = VideoCodecToCodecID(config.codec());
  codec_context->profile = VideoCodecProfileToProfileID(config.profile());
  codec_context->coded_width = config.coded_size().width();
  codec_context->coded_height = config.coded_size().height();
  codec_context->pix_fmt = VideoPixelFormatToAVPixelFormat(config.format());
  if (config.color_space() == COLOR_SPACE_JPEG)
    codec_context->color_range = AVCOL_RANGE_JPEG;

  if (config.extra_data().empty()) {
    codec_context->extradata = nullptr;
    codec_context->extradata_size = 0;
  } else {
    codec_context->extradata_size = config.extra_data().size();
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data().size() + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, &config.extra_data()[0],
           config.extra_data().size());
    memset(codec_context->extradata + config.extra_data().size(), '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  }
}

ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout, int channels) {
  switch (layout) {
    case AV_CH_LAYOUT_MONO:
      return CHANNEL_LAYOUT_MONO;
    case AV_CH_LAYOUT_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case AV_CH_LAYOUT_2_1:
      return CHANNEL_LAYOUT_2_1;
    case AV_CH_LAYOUT_SURROUND:
      return CHANNEL_LAYOUT_SURROUND;
    case AV_CH_LAYOUT_4POINT0:
      return CHANNEL_LAYOUT_4_0;
    case AV_CH_LAYOUT_2_2:
      return CHANNEL_LAYOUT_2_2;
    case AV_CH_LAYOUT_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case AV_CH_LAYOUT_5POINT0:
      return CHANNEL_LAYOUT_5_0;
    case AV_CH_LAYOUT_5POINT1:
      return CHANNEL_LAYOUT_5_1;
    case AV_CH_LAYOUT_5POINT0_BACK:
      return CHANNEL_LAYOUT_5_0_BACK;
    case AV_CH_LAYOUT_5POINT1_BACK:
      return CHANNEL_LAYOUT_5_1_BACK;
    case AV_CH_LAYOUT_7POINT0:
      return CHANNEL_LAYOUT_7_0;
    case AV_CH_LAYOUT_7POINT1:
      return CHANNEL_LAYOUT_7_1;
    case AV_CH_LAYOUT_7POINT1_WIDE:
      return CHANNEL_LAYOUT_7_1_WIDE;
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
      return CHANNEL_LAYOUT_STEREO_DOWNMIX;
    case AV_CH_LAYOUT_2POINT1:
      return CHANNEL_LAYOUT_2POINT1;
    case AV_CH_LAYOUT_3POINT1:
      return CHANNEL_LAYOUT_3_1;
    case AV_CH_LAYOUT_4POINT1:
      return CHANNEL_LAYOUT_4_1;
    case AV_CH_LAYOUT_6POINT0:
      return CHANNEL_LAYOUT_6_0;
    case AV_CH_LAYOUT_6POINT0_FRONT:
      return CHANNEL_LAYOUT_6_0_FRONT;
    case AV_CH_LAYOUT_HEXAGONAL:
      return CHANNEL_LAYOUT_HEXAGONAL;
    case AV_CH_LAYOUT_6POINT1:
      return CHANNEL_LAYOUT_6_1;
    case AV_CH_LAYOUT_6POINT1_BACK:
      return CHANNEL_LAYOUT_6_1_BACK;
    case AV_CH_LAYOUT_6POINT1_FRONT:
      return CHANNEL_LAYOUT_6_1_FRONT;
    case AV_CH_LAYOUT_7POINT0_FRONT:
      return CHANNEL_LAYOUT_7_0_FRONT;
#ifdef AV_CH_LAYOUT_7POINT1_WIDE_BACK
    case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
      return CHANNEL_LAYOUT_7_1_WIDE_BACK;
#endif
    case AV_CH_LAYOUT_OCTAGONAL:
      return CHANNEL_LAYOUT_OCTAGONAL;
    default:
      // FFmpeg channel_layout is 0 for .wav and .mp3.  Attempt to guess layout
      // based on the channel count.
      return GuessChannelLayout(channels);
  }
}

#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#error The code below assumes little-endianness.
#endif

VideoPixelFormat AVPixelFormatToVideoPixelFormat(AVPixelFormat pixel_format) {
  // The YUVJ alternatives are FFmpeg's (deprecated, but still in use) way to
  // specify a pixel format and full range color combination.
  switch (pixel_format) {
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUVJ422P:
      return PIXEL_FORMAT_YV16;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
      return PIXEL_FORMAT_YV24;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return PIXEL_FORMAT_YV12;
    case AV_PIX_FMT_YUVA420P:
      return PIXEL_FORMAT_YV12A;

    case AV_PIX_FMT_YUV420P9LE:
      return PIXEL_FORMAT_YUV420P9;
    case AV_PIX_FMT_YUV420P10LE:
      return PIXEL_FORMAT_YUV420P10;
    case AV_PIX_FMT_YUV420P12LE:
      return PIXEL_FORMAT_YUV420P12;

    case AV_PIX_FMT_YUV422P9LE:
      return PIXEL_FORMAT_YUV422P9;
    case AV_PIX_FMT_YUV422P10LE:
      return PIXEL_FORMAT_YUV422P10;
    case AV_PIX_FMT_YUV422P12LE:
      return PIXEL_FORMAT_YUV422P12;

    case AV_PIX_FMT_YUV444P9LE:
      return PIXEL_FORMAT_YUV444P9;
    case AV_PIX_FMT_YUV444P10LE:
      return PIXEL_FORMAT_YUV444P10;
    case AV_PIX_FMT_YUV444P12LE:
      return PIXEL_FORMAT_YUV444P12;

    default:
      DVLOG(1) << "Unsupported AVPixelFormat: " << pixel_format;
  }
  return PIXEL_FORMAT_UNKNOWN;
}

AVPixelFormat VideoPixelFormatToAVPixelFormat(VideoPixelFormat video_format) {
  switch (video_format) {
    case PIXEL_FORMAT_YV16:
      return AV_PIX_FMT_YUV422P;
    case PIXEL_FORMAT_YV12:
      return AV_PIX_FMT_YUV420P;
    case PIXEL_FORMAT_YV12A:
      return AV_PIX_FMT_YUVA420P;
    case PIXEL_FORMAT_YV24:
      return AV_PIX_FMT_YUV444P;
    case PIXEL_FORMAT_YUV420P9:
      return AV_PIX_FMT_YUV420P9LE;
    case PIXEL_FORMAT_YUV420P10:
      return AV_PIX_FMT_YUV420P10LE;
    case PIXEL_FORMAT_YUV420P12:
      return AV_PIX_FMT_YUV420P12LE;
    case PIXEL_FORMAT_YUV422P9:
      return AV_PIX_FMT_YUV422P9LE;
    case PIXEL_FORMAT_YUV422P10:
      return AV_PIX_FMT_YUV422P10LE;
    case PIXEL_FORMAT_YUV422P12:
      return AV_PIX_FMT_YUV422P12LE;
    case PIXEL_FORMAT_YUV444P9:
      return AV_PIX_FMT_YUV444P9LE;
    case PIXEL_FORMAT_YUV444P10:
      return AV_PIX_FMT_YUV444P10LE;
    case PIXEL_FORMAT_YUV444P12:
      return AV_PIX_FMT_YUV444P12LE;

    default:
      DVLOG(1) << "Unsupported Format: " << video_format;
  }
  return AV_PIX_FMT_NONE;
}

ColorSpace AVColorSpaceToColorSpace(AVColorSpace color_space,
                                    AVColorRange color_range) {
  if (color_range == AVCOL_RANGE_JPEG)
    return COLOR_SPACE_JPEG;

  switch (color_space) {
    case AVCOL_SPC_UNSPECIFIED:
      break;
    case AVCOL_SPC_BT709:
      return COLOR_SPACE_HD_REC709;
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
      return COLOR_SPACE_SD_REC601;
    default:
      DVLOG(1) << "Unknown AVColorSpace: " << color_space;
  }
  return COLOR_SPACE_UNSPECIFIED;
}

int32_t HashCodecName(const char* codec_name) {
  // Use the first 32-bits from the SHA1 hash as the identifier.
  int32_t hash;
  memcpy(&hash, base::SHA1HashString(codec_name).substr(0, 4).c_str(), 4);
  return hash;
}

#define TEST_PRIMARY(P)                                                 \
  static_assert(                                                        \
      static_cast<int>(gfx::ColorSpace::PrimaryID::P) == AVCOL_PRI_##P, \
      "gfx::ColorSpace::PrimaryID::" #P " does not match AVCOL_PRI_" #P);

#define TEST_TRANSFER(T)                                                 \
  static_assert(                                                         \
      static_cast<int>(gfx::ColorSpace::TransferID::T) == AVCOL_TRC_##T, \
      "gfx::ColorSpace::TransferID::" #T " does not match AVCOL_TRC_" #T);

#define TEST_COLORSPACE(C)                                             \
  static_assert(                                                       \
      static_cast<int>(gfx::ColorSpace::MatrixID::C) == AVCOL_SPC_##C, \
      "gfx::ColorSpace::MatrixID::" #C " does not match AVCOL_SPC_" #C);

TEST_PRIMARY(RESERVED0);
TEST_PRIMARY(BT709);
TEST_PRIMARY(UNSPECIFIED);
TEST_PRIMARY(RESERVED);
TEST_PRIMARY(BT470M);
TEST_PRIMARY(BT470BG);
TEST_PRIMARY(SMPTE170M);
TEST_PRIMARY(SMPTE240M);
TEST_PRIMARY(FILM);
TEST_PRIMARY(BT2020);
TEST_PRIMARY(SMPTEST428_1);

TEST_TRANSFER(RESERVED0);
TEST_TRANSFER(BT709);
TEST_TRANSFER(UNSPECIFIED);
TEST_TRANSFER(RESERVED);
TEST_TRANSFER(GAMMA22);
TEST_TRANSFER(GAMMA28);
TEST_TRANSFER(SMPTE170M);
TEST_TRANSFER(SMPTE240M);
TEST_TRANSFER(LINEAR);
TEST_TRANSFER(LOG);
TEST_TRANSFER(LOG_SQRT);
TEST_TRANSFER(IEC61966_2_4);
TEST_TRANSFER(BT1361_ECG);
TEST_TRANSFER(IEC61966_2_1);
TEST_TRANSFER(BT2020_10);
TEST_TRANSFER(BT2020_12);
TEST_TRANSFER(SMPTEST2084);
TEST_TRANSFER(SMPTEST428_1);

TEST_COLORSPACE(RGB);
TEST_COLORSPACE(BT709);
TEST_COLORSPACE(UNSPECIFIED);
TEST_COLORSPACE(RESERVED);
TEST_COLORSPACE(FCC);
TEST_COLORSPACE(BT470BG);
TEST_COLORSPACE(SMPTE170M);
TEST_COLORSPACE(SMPTE240M);
TEST_COLORSPACE(YCOCG);
TEST_COLORSPACE(BT2020_NCL);
TEST_COLORSPACE(BT2020_CL);

}  // namespace media
