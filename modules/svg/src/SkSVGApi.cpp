#include "modules/svg/include/SkSVGApi.h"

#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"

#include "include/core/SkSurface.h"
#include "include/encode/SkPngEncoder.h"
#include "modules/svg/include/SkSVGDOM.h"

#include <iostream>

namespace {

sk_sp<skia::textlayout::TypefaceFontProvider> fontProvider = sk_make_sp<skia::textlayout::TypefaceFontProvider>();

constexpr int maxSize = 398;

} // namespace

bool SkSVGApi::registerFonts(const std::vector<std::string>& fontFilePaths) {
    for (const auto& fontFilePath : fontFilePaths) {
        fontProvider->registerTypeface(SkTypeface::MakeFromFile(fontFilePath.data()));
    }

    return true;
}

bool SkSVGApi::renderSvg(const std::string& svg, const std::string& outputFilePath) {
    auto memStream = SkMemoryStream::MakeDirect(svg.data(), svg.size());

    auto svgDom = SkSVGDOM::Builder()
                        .setFontManager(fontProvider)
                        .make(*memStream);
    if (!svgDom) {
        std::cerr << "Could not parse svg\n";
        return false;
    }

    auto surface = SkSurface::MakeRasterN32Premul(maxSize, maxSize);

    svgDom->setContainerSize(SkSize::Make(maxSize, maxSize));
    svgDom->render(surface->getCanvas());

    SkPixmap pixmap;
    surface->peekPixels(&pixmap);

    SkFILEWStream out(outputFilePath.data());
    if (!out.isValid()) {
        std::cerr << "Could not open " << outputFilePath << " for writing.\n";
        return false;
    }

    // Use default encoding options.
    SkPngEncoder::Options pngOptions;

    if (!SkPngEncoder::Encode(&out, pixmap, pngOptions)) {
        std::cerr << "PNG encoding failed.\n";
        return false;
    }

    return true;
}
