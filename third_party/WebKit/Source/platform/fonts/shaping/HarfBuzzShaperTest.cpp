// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "platform/text/TextRun.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include <unicode/uscript.h>

namespace blink {

class HarfBuzzShaperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fontDescription.setComputedSize(12.0);
    font = Font(fontDescription);
    font.update(nullptr);
  }

  void TearDown() override {}

  FontCachePurgePreventer fontCachePurgePreventer;
  FontDescription fontDescription;
  Font font;
  unsigned startIndex = 0;
  unsigned numGlyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

static inline ShapeResultTestInfo* testInfo(RefPtr<ShapeResult>& result) {
  return static_cast<ShapeResultTestInfo*>(result.get());
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLatin) {
  TextRun latinCommon(reinterpret_cast<const LChar*>("ABC DEF."), 8);
  HarfBuzzShaper shaper(latinCommon);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);

  ASSERT_EQ(1u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(8u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLeadingCommon) {
  TextRun leadingCommon(reinterpret_cast<const LChar*>("... test"), 8);
  HarfBuzzShaper shaper(leadingCommon);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);

  ASSERT_EQ(1u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(8u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsUnicodeVariants) {
  struct {
    const char* name;
    UChar string[4];
    hb_script_t script;
  } testlist[] = {
      {"Standard Variants text style", {0x30, 0xFE0E}, HB_SCRIPT_COMMON},
      {"Standard Variants emoji style", {0x203C, 0xFE0F}, HB_SCRIPT_COMMON},
      {"Standard Variants of Ideograph", {0x4FAE, 0xFE00}, HB_SCRIPT_HAN},
      {"Ideographic Variants", {0x3402, 0xDB40, 0xDD00}, HB_SCRIPT_HAN},
      {"Not-defined Variants", {0x41, 0xDB40, 0xDDEF}, HB_SCRIPT_LATIN},
  };
  for (auto& test : testlist) {
    String str(test.string);
    TextRun run(str);
    HarfBuzzShaper shaper(run);
    RefPtr<ShapeResult> result = shaper.shapeResult(&font);

    EXPECT_EQ(1u, testInfo(result)->numberOfRunsForTesting()) << test.name;
    ASSERT_TRUE(
        testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script))
        << test.name;
    EXPECT_EQ(0u, startIndex) << test.name;
    if (numGlyphs == 2) {
// If the specified VS is not in the font, it's mapped to .notdef.
// then hb_ot_hide_default_ignorables() swaps it to a space with zero-advance.
// http://lists.freedesktop.org/archives/harfbuzz/2015-May/004888.html
// OpenType recommends Glyph ID 3 for a space; not a hard requirement though.
// https://www.microsoft.com/typography/otspec/recom.htm
#if !OS(MACOSX)
      EXPECT_EQ(3u, testInfo(result)->glyphForTesting(0, 1)) << test.name;
#endif
      EXPECT_EQ(0.f, testInfo(result)->advanceForTesting(0, 1)) << test.name;
    } else {
      EXPECT_EQ(1u, numGlyphs) << test.name;
    }
    EXPECT_EQ(test.script, script) << test.name;
  }
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommon) {
  UChar devanagariCommonString[] = {0x915, 0x94d, 0x930, 0x28, 0x20, 0x29};
  TextRun devanagariCommonLatin(devanagariCommonString, 6);
  HarfBuzzShaper shaper(devanagariCommonLatin);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);

  ASSERT_EQ(2u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(1, startIndex, numGlyphs, script));
  EXPECT_EQ(3u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommonLatinCommon) {
  UChar devanagariCommonLatinString[] = {0x915, 0x94d, 0x930, 0x20,
                                         0x61,  0x62,  0x2E};
  TextRun devanagariCommonLatin(devanagariCommonLatinString, 7);
  HarfBuzzShaper shaper(devanagariCommonLatin);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);

  ASSERT_EQ(3u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(1, startIndex, numGlyphs, script));
  EXPECT_EQ(3u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(2, startIndex, numGlyphs, script));
  EXPECT_EQ(4u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatin) {
  UChar mixedString[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  TextRun mixed(mixedString, 6);
  HarfBuzzShaper shaper(mixed);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);

  ASSERT_EQ(4u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(1, startIndex, numGlyphs, script));
  EXPECT_EQ(3u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_THAI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(2, startIndex, numGlyphs, script));
  EXPECT_EQ(4u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_HAN, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(3, startIndex, numGlyphs, script));
  EXPECT_EQ(5u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatinTwice) {
  UChar mixedString[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  TextRun mixed(mixedString, 6);
  HarfBuzzShaper shaper(mixed);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);
  ASSERT_EQ(4u, testInfo(result)->numberOfRunsForTesting());

  // Shape again on the same shape object and check the number of runs.
  // Should be equal if no state was retained between shape calls.
  RefPtr<ShapeResult> result2 = shaper.shapeResult(&font);
  ASSERT_EQ(4u, testInfo(result2)->numberOfRunsForTesting());
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabic) {
  UChar arabicString[] = {0x628, 0x64A, 0x629};
  TextRun arabic(arabicString, 3);
  HarfBuzzShaper shaper(arabic);
  RefPtr<ShapeResult> result = shaper.shapeResult(&font);

  ASSERT_EQ(1u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script);
}

}  // namespace blink
