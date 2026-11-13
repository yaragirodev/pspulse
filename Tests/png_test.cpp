// ---------------------------------------------------------------------------
//  Regression test for the self-contained PNG decoder used by Build Settings
//  (ICON0/PIC1 validation and downscaling). It carries its own inflate, so a
//  broken inflate would silently produce black XMB artwork.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Editor/Source/BuildPipeline.h"

namespace {
int gFailures = 0;
void Check(bool ok, const char* what) {
    std::printf(ok ? "  ok:   %s\n" : "  FAIL: %s\n", what);
    if (!ok) ++gFailures;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <input.png>\n", argv[0]);
        return 2;
    }
    const std::string input = argv[1];
    // Keep the scratch file next to the test binary: /tmp does not exist on Windows.
    const std::string output = std::string(argv[0]) + ".out.png";

    int width = 0, height = 0;
    Check(pspeditor::ReadPngSize(input, width, height), "PNG header parsed");
    std::printf("        source: %dx%d\n", width, height);

    std::string error;
    const bool normalized = pspeditor::NormalizePngImage(input, output, 144, 80, error);
    Check(normalized, normalized ? "normalised to 144x80" : error.c_str());
    if (normalized) {
        int outWidth = 0, outHeight = 0;
        Check(pspeditor::ReadPngSize(output, outWidth, outHeight), "output written");
        Check(outWidth == 144 && outHeight == 80, "output is exactly 144x80");
        std::remove(output.c_str());
    }

    std::printf(gFailures == 0 ? "\nRESULT: PASS\n" : "\nRESULT: FAIL (%d)\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
