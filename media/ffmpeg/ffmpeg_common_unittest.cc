// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstring>

#include "base/bind.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FFmpegCommonTest : public testing::Test {
 public:
  FFmpegCommonTest() {
    FFmpegGlue::InitializeFFmpeg();
  }
  ~FFmpegCommonTest() override{};
};

uint8_t kExtraData[5] = {0x00, 0x01, 0x02, 0x03, 0x04};

template <typename T>
void TestConfigConvertExtraData(
    AVStream* stream,
    T* decoder_config,
    const base::Callback<bool(const AVStream*, T*)>& converter_fn) {
  // Should initially convert.
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));

  // Store orig to let FFmpeg free whatever it allocated.
  AVCodecParameters* codec_parameters = stream->codecpar;
  uint8_t* orig_extradata = codec_parameters->extradata;
  int orig_extradata_size = codec_parameters->extradata_size;

  // Valid combination: extra_data = nullptr && size = 0.
  codec_parameters->extradata = nullptr;
  codec_parameters->extradata_size = 0;
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));
  EXPECT_EQ(static_cast<size_t>(codec_parameters->extradata_size),
            decoder_config->extra_data().size());

  // Valid combination: extra_data = non-nullptr && size > 0.
  codec_parameters->extradata = &kExtraData[0];
  codec_parameters->extradata_size = arraysize(kExtraData);
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));
  EXPECT_EQ(static_cast<size_t>(codec_parameters->extradata_size),
            decoder_config->extra_data().size());
  EXPECT_EQ(
      0, memcmp(codec_parameters->extradata, &decoder_config->extra_data()[0],
                decoder_config->extra_data().size()));

  // Possible combination: extra_data = nullptr && size != 0, but the converter
  // function considers this valid and having no extra_data, due to behavior of
  // avcodec_parameters_to_context().
  codec_parameters->extradata = nullptr;
  codec_parameters->extradata_size = 10;
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));
  EXPECT_EQ(0UL, decoder_config->extra_data().size());

  // Invalid combination: extra_data = non-nullptr && size = 0.
  codec_parameters->extradata = &kExtraData[0];
  codec_parameters->extradata_size = 0;
  EXPECT_FALSE(converter_fn.Run(stream, decoder_config));

  // Restore orig values for sane cleanup.
  codec_parameters->extradata = orig_extradata;
  codec_parameters->extradata_size = orig_extradata_size;
}

TEST_F(FFmpegCommonTest, AVStreamToDecoderConfig) {
  // Open a file to get a real AVStreams from FFmpeg.
  base::MemoryMappedFile file;
  file.Initialize(GetTestDataFilePath("bear-320x240.webm"));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());
  AVFormatContext* format_context = glue.format_context();

  // Find the audio and video streams and test valid and invalid combinations
  // for extradata and extradata_size.
  bool found_audio = false;
  bool found_video = false;
  for (size_t i = 0;
       i < format_context->nb_streams && (!found_audio || !found_video);
       ++i) {
    AVStream* stream = format_context->streams[i];
    AVCodecParameters* codec_parameters = stream->codecpar;
    AVMediaType codec_type = codec_parameters->codec_type;

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (found_audio)
        continue;
      found_audio = true;
      AudioDecoderConfig audio_config;
      TestConfigConvertExtraData(stream, &audio_config,
                                 base::Bind(&AVStreamToAudioDecoderConfig));
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if (found_video)
        continue;
      found_video = true;
      VideoDecoderConfig video_config;
      TestConfigConvertExtraData(stream, &video_config,
                                 base::Bind(&AVStreamToVideoDecoderConfig));
    } else {
      // Only process audio/video.
      continue;
    }
  }

  ASSERT_TRUE(found_audio);
  ASSERT_TRUE(found_video);
}

TEST_F(FFmpegCommonTest, OpusAudioDecoderConfig) {
  AVCodecContext context = {0};
  context.codec_type = AVMEDIA_TYPE_AUDIO;
  context.codec_id = AV_CODEC_ID_OPUS;
  context.channel_layout = CHANNEL_LAYOUT_STEREO;
  context.channels = 2;
  context.sample_fmt = AV_SAMPLE_FMT_FLT;

  // During conversion this sample rate should be changed to 48kHz.
  context.sample_rate = 44100;

  AudioDecoderConfig decoder_config;
  ASSERT_TRUE(AVCodecContextToAudioDecoderConfig(&context, Unencrypted(),
                                                 &decoder_config));
  EXPECT_EQ(48000, decoder_config.samples_per_second());
}

TEST_F(FFmpegCommonTest, TimeBaseConversions) {
  const int64_t test_data[][5] = {
      {1, 2, 1, 500000, 1}, {1, 3, 1, 333333, 1}, {1, 3, 2, 666667, 2},
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SCOPED_TRACE(i);

    AVRational time_base;
    time_base.num = static_cast<int>(test_data[i][0]);
    time_base.den = static_cast<int>(test_data[i][1]);

    base::TimeDelta time_delta =
        ConvertFromTimeBase(time_base, test_data[i][2]);

    EXPECT_EQ(time_delta.InMicroseconds(), test_data[i][3]);
    EXPECT_EQ(ConvertToTimeBase(time_base, time_delta), test_data[i][4]);
  }
}

TEST_F(FFmpegCommonTest, VerifyFormatSizes) {
  for (AVSampleFormat format = AV_SAMPLE_FMT_NONE;
       format < AV_SAMPLE_FMT_NB;
       format = static_cast<AVSampleFormat>(format + 1)) {
    std::vector<AVCodecID> codec_ids(1, AV_CODEC_ID_NONE);
    if (format == AV_SAMPLE_FMT_S32)
      codec_ids.push_back(AV_CODEC_ID_PCM_S24LE);
    for (const auto& codec_id : codec_ids) {
      SampleFormat sample_format =
          AVSampleFormatToSampleFormat(format, codec_id);
      if (sample_format == kUnknownSampleFormat) {
        // This format not supported, so skip it.
        continue;
      }

      // Have FFMpeg compute the size of a buffer of 1 channel / 1 frame
      // with 1 byte alignment to make sure the sizes match.
      int single_buffer_size =
          av_samples_get_buffer_size(NULL, 1, 1, format, 1);
      int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
      EXPECT_EQ(bytes_per_channel, single_buffer_size);
    }
  }
}

// Verifies there are no collisions of the codec name hashes used for UMA.  Also
// includes code for updating the histograms XML.
TEST_F(FFmpegCommonTest, VerifyUmaCodecHashes) {
  const AVCodecDescriptor* desc = avcodec_descriptor_next(nullptr);

  std::map<int32_t, const char*> sorted_hashes;
  while (desc) {
    const int32_t hash = HashCodecName(desc->name);
    // Ensure there are no collisions.
    ASSERT_TRUE(sorted_hashes.find(hash) == sorted_hashes.end());
    sorted_hashes[hash] = desc->name;

    desc = avcodec_descriptor_next(desc);
  }

  // Add a none entry for when no codec is detected.
  static const char kUnknownCodec[] = "none";
  const int32_t hash = HashCodecName(kUnknownCodec);
  ASSERT_TRUE(sorted_hashes.find(hash) == sorted_hashes.end());
  sorted_hashes[hash] = kUnknownCodec;

  // Uncomment the following lines to generate the "FFmpegCodecHashes" enum for
  // usage in the histogram metrics file.  While it regenerates *ALL* values, it
  // should only be used to *ADD* values to histograms file.  Never delete any
  // values; diff should verify.
#if 0
  printf("<enum name=\"FFmpegCodecHashes\" type=\"int\">\n");
  for (const auto& kv : sorted_hashes)
    printf("  <int value=\"%d\" label=\"%s\"/>\n", kv.first, kv.second);
  printf("</enum>\n");
#endif
}

}  // namespace media
