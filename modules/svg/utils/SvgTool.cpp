/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fstream>
#include <iostream>
#include <sstream>

#include "include/core/SkMatrix.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/encode/SkPngEncoder.h"
// #include "include/ports/SkFontMgr_directory.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "src/utils/SkOSPath.h"
#include "tools/flags/CommandLineFlags.h"

static DEFINE_string2(input , i, nullptr, "Input SVG file.");
static DEFINE_string2(output, o, nullptr, "Output PNG file.");

static DEFINE_int(width , 1024, "Output width.");
static DEFINE_int(height, 1024, "Output height.");

int main(int argc, char** argv) {
    CommandLineFlags::Parse(argc, argv);

    if (FLAGS_input.isEmpty() || FLAGS_output.isEmpty()) {
        std::cerr << "Missing required 'input' and 'output' args.\n";
        return 1;
    }

    if (FLAGS_width <= 0 || FLAGS_height <= 0) {
        std::cerr << "Invalid width/height.\n";
        return 1;
    }

    SkFILEStream in(FLAGS_input[0]);
    if (!in.isValid()) {
        std::cerr << "Could not open " << FLAGS_input[0] << "\n";
        return 1;
    }

    std::ifstream t(FLAGS_input[0]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    const std::string svgStr = buffer.str();
    auto memStream = SkMemoryStream::MakeDirect(svgStr.data(), svgStr.size());

    // auto rp = skresources::DataURIResourceProviderProxy::Make(
    //               skresources::FileResourceProvider::Make(SkOSPath::Dirname(FLAGS_input[0]),
    //                                                       /*predecode=*/true),
    //               /*predecode=*/true);

    auto fp = sk_make_sp<skia::textlayout::TypefaceFontProvider>();
    fp->registerTypeface(SkTypeface::MakeFromFile("/Users/brad.kotsopoulos/Snapchat/Dev/skia/resources/fonts/bm2_bubble-LightCondensed.otf"));
    fp->registerTypeface(SkTypeface::MakeFromFile("/Users/brad.kotsopoulos/Snapchat/Dev/skia/resources/fonts/bm2_bubble-Regular.otf"));

    // sk_sp<SkFontMgr> fm = SkFontMgr_New_Custom_Directory("/Users/brad.kotsopoulos/Snapchat/Dev/skia/resources/fonts/");
    // std::cout << "num: " << fm->countFamilies() << std::endl;
    // fm->makeFromFile("/Users/brad.kotsopoulos/Snapchat/Dev/skia/resources/fonts/bm2_bubble-LightCondensed.otf");
    // std::cout << "num: " << fm->countFamilies() << std::endl;

    auto svg_dom = SkSVGDOM::Builder()
                        .setFontManager(std::move(fp))
                        // .setFontManager(SkFontMgr::RefDefault())
                        // .setResourceProvider(std::move(rp))
                        .make(*memStream);
    if (!svg_dom) {
        std::cerr << "Could not parse " << FLAGS_input[0] << "\n";
        return 1;
    }

    auto surface = SkSurface::MakeRasterN32Premul(FLAGS_width, FLAGS_height);

    svg_dom->setContainerSize(SkSize::Make(FLAGS_width, FLAGS_height));
    svg_dom->render(surface->getCanvas());

    SkPixmap pixmap;
    surface->peekPixels(&pixmap);

    SkFILEWStream out(FLAGS_output[0]);
    if (!out.isValid()) {
        std::cerr << "Could not open " << FLAGS_output[0] << " for writing.\n";
        return 1;
    }

    // Use default encoding options.
    SkPngEncoder::Options png_options;

    if (!SkPngEncoder::Encode(&out, pixmap, png_options)) {
        std::cerr << "PNG encoding failed.\n";
        return 1;
    }

    return 0;
}
