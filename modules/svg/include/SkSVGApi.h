#ifndef SkSVGAPI_DEFINED
#define SkSVGAPI_DEFINED

#include <string>
#include <vector>

class __attribute__((visibility("default"))) SkSVGApi {
 public:
    static bool registerFonts(const std::vector<std::string>& fontFilePaths);
    static bool registerFonts(const std::vector<std::pair<std::string, std::string>>& fontFilePaths);

    static bool renderSvg(const std::string& svg, const std::string& outputFilePath);
};

#endif // SkSVGAPI_DEFINED
