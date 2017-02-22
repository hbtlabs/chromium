// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_source.h"
#include "components/metrics/proto/ukm/source.pb.h"

namespace ukm {

UkmSource::UkmSource() = default;

UkmSource::~UkmSource() = default;

void UkmSource::PopulateProto(Source* proto_source) {
  proto_source->set_url(committed_url_.spec());

  proto_source->set_first_contentful_paint_msec(
      first_contentful_paint_.InMilliseconds());
}

}  // namespace ukm
