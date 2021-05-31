/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fstream>
#include <iostream>
#include <sstream>

#include "include/core/SkStream.h"
#include "modules/svg/include/SkSVGApi.h"
#include "tools/flags/CommandLineFlags.h"

static DEFINE_string2(input , i, nullptr, "Input SVG file.");
static DEFINE_string2(output, o, nullptr, "Output PNG file.");

int main(int argc, char** argv) {
    CommandLineFlags::Parse(argc, argv);

    if (FLAGS_input.isEmpty() || FLAGS_output.isEmpty()) {
        std::cerr << "Missing required 'input' and 'output' args.\n";
        return 1;
    }

    SkFILEStream in(FLAGS_input[0]);
    if (!in.isValid()) {
        std::cerr << "Could not open " << FLAGS_input[0] << "\n";
        return 1;
    }

    std::vector<std::pair<std::string, std::string>> fontMap = {
        {"/Users/brad.kotsopoulos/Snapchat/Dev/skia/resources/fonts/bm2_bubble-LightCondensed.otf", "bm2_bubble-LightCondensed"},
        {"/Users/brad.kotsopoulos/Snapchat/Dev/skia/resources/fonts/bm2_bubble-Regular.otf", "bm2_bubble-Regular"},
    };

    std::cout << "registerFonts: " << SkSVGApi::registerFonts(fontMap) << std::endl;

    std::ifstream t(FLAGS_input[0]);
    std::stringstream buffer;
    buffer << t.rdbuf();
    const std::string svgStr = buffer.str();

    std::cout << "renderSvg: " << SkSVGApi::renderSvg(svgStr, FLAGS_output[0]) << std::endl;

    return 0;
}
