#include "pch.h"
#include "cmd_terrain.h"
#include "terrain_files.h"
#include "common.h"   // pics_s, pic_s, Pic_FreePics

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  igi1conv terrain export-lmp <input.lmp> -o <output.pgm>\n"
        "  igi1conv terrain import-lmp <input.pgm> -o <output.lmp>\n"
        "  igi1conv terrain export-ctr <input.ctr> -o <output.json>\n"
        "  igi1conv terrain info <input.lmp|.ctr>\n";
}

// Return value of named option, or nullptr.
static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

// Write a 16-bit binary PGM (P5) from a single 8-bit greyscale pic_s.
// The LMP format stores luma in channel 0 of each RGBA pixel.
static bool write_pgm16(const char* out_path, const pic_s* pic)
{
    std::ofstream ofs(out_path, std::ios::binary);
    if (!ofs)
        return false;

    int w = pic->width_;
    int h = pic->height_;

    // PGM P5 header
    ofs << "P5\n" << w << " " << h << "\n65535\n";

    // Each pixel: luma value from channel 0, scaled 8-bit -> 16-bit (big-endian)
    for (int i = 0; i < w * h; ++i)
    {
        uint8_t luma = pic->pixels_[i * 4];             // R == G == B == luma
        uint16_t val16 = static_cast<uint16_t>(luma) * 257u; // 0x00->0x0000, 0xFF->0xFFFF
        uint8_t hi = static_cast<uint8_t>(val16 >> 8);
        uint8_t lo = static_cast<uint8_t>(val16 & 0xFF);
        ofs.put(static_cast<char>(hi));
        ofs.put(static_cast<char>(lo));
    }
    return ofs.good();
}

// Write CTR items as a JSON array.
static bool write_ctr_json(const char* out_path, const ctr_s& ctr)
{
    std::ofstream ofs(out_path);
    if (!ofs)
        return false;

    if (ctr.num_item_ == 0)
    {
        ofs << "[]\n";
        return ofs.good();
    }

    ofs << "[\n";
    for (int i = 0; i < ctr.num_item_; ++i)
    {
        const ctr_item_s& item = ctr.head_[i];
        if (i) ofs << ",\n";
        ofs << "  {\n";
        ofs << "    \"index\": " << i << ",\n";

        // children array (8 entries)
        ofs << "    \"children\": [";
        for (int c = 0; c < 8; ++c)
        {
            if (c) ofs << ",";
            ofs << item.children_[c];
        }
        ofs << "],\n";

        // cmd_transform array (8 entries)
        ofs << "    \"cmd_transform\": [";
        for (int c = 0; c < 8; ++c)
        {
            if (c) ofs << ",";
            ofs << static_cast<int>(item.cmd_transform_[c]);
        }
        ofs << "],\n";

        ofs << "    \"children_mask\": " << static_cast<unsigned>(item.children_mask_) << ",\n";
        ofs << "    \"cmd_offset\": "    << item.cmd_offset_ << "\n";
        ofs << "  }";
    }
    ofs << "\n]\n";
    return ofs.good();
}

// Read a binary PGM (P5) into an 8-bit greyscale image.  Both 8-bit and
// 16-bit maxval variants are accepted; comments between header tokens are
// ignored as required by the PGM grammar.  LMP stores square lightmap
// chunks, so the image must be square.
static bool read_pgm8(const char* in_path, uint32_t& size, std::vector<uint8_t>& luma,
                      std::string& err)
{
    std::ifstream ifs(in_path, std::ios::binary);
    if (!ifs)
    {
        err = "cannot open input PGM";
        return false;
    }

    auto skip_space_and_comments = [&ifs]() {
        for (;;)
        {
            const int c = ifs.peek();
            if (c == '#')
            {
                while (ifs.good() && ifs.get() != '\n') {}
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                ifs.get();
                continue;
            }
            return;
        }
    };

    auto read_token = [&ifs, &skip_space_and_comments](std::string& out) -> bool {
        skip_space_and_comments();
        out.clear();
        for (;;)
        {
            const int c = ifs.peek();
            if (!ifs.good()) return false;
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return !out.empty();
            out.push_back(static_cast<char>(ifs.get()));
        }
    };

    std::string magic, w_s, h_s, max_s;
    if (!read_token(magic) || magic != "P5")
    {
        err = "not a binary PGM (expected P5 magic)";
        return false;
    }
    if (!read_token(w_s) || !read_token(h_s) || !read_token(max_s))
    {
        err = "truncated PGM header";
        return false;
    }

    auto parse_uint = [](const std::string& s, uint64_t& v) -> bool {
        if (s.empty()) return false;
        v = 0;
        for (char ch : s)
        {
            if (ch < '0' || ch > '9') return false;
            v = v * 10 + static_cast<uint64_t>(ch - '0');
            if (v > 0xFFFFFFFFull) return false;
        }
        return true;
    };

    uint64_t w = 0, h = 0, maxval = 0;
    if (!parse_uint(w_s, w) || !parse_uint(h_s, h) || !parse_uint(max_s, maxval))
    {
        err = "invalid PGM dimensions or maxval";
        return false;
    }
    if (w == 0 || h == 0 || w > 0xFFFFu || w != h)
    {
        err = "LMP lightmaps must be square and non-empty";
        return false;
    }
    if (maxval == 0 || maxval > 65535)
    {
        err = "unsupported PGM maxval (expected 1..65535)";
        return false;
    }

    // Exactly one whitespace character separates the header from raster data.
    if (ifs.get() == '\r' && ifs.peek() == '\n') ifs.get();
    const bool two_bytes = maxval > 255;
    const uint64_t count = w * h;
    luma.clear();
    luma.reserve(static_cast<size_t>(count));
    for (uint64_t i = 0; i < count; ++i)
    {
        uint64_t v = 0;
        if (two_bytes)
        {
            const int hi = ifs.get();
            const int lo = ifs.get();
            if (hi == std::char_traits<char>::eof() || lo == std::char_traits<char>::eof())
            {
                err = "truncated PGM raster data";
                return false;
            }
            v = (static_cast<uint64_t>(hi) << 8) | static_cast<uint64_t>(lo);
        }
        else
        {
            const int c = ifs.get();
            if (c == std::char_traits<char>::eof())
            {
                err = "truncated PGM raster data";
                return false;
            }
            v = static_cast<uint64_t>(c);
        }
        if (v > maxval)
        {
            err = "PGM sample exceeds maxval";
            return false;
        }
        luma.push_back(static_cast<uint8_t>((v * 255u + maxval / 2u) / maxval));
    }

    size = static_cast<uint32_t>(w);
    return true;
}

// Write one square lightmap as an LMP chunk: uint32 LE size followed by
// size*size luma bytes.  This is the exact layout LMP_Load() expects.
static bool write_lmp(const char* out_path, uint32_t size, const std::vector<uint8_t>& luma)
{
    std::ofstream ofs(out_path, std::ios::binary);
    if (!ofs) return false;
    const uint8_t header[4] = {
        static_cast<uint8_t>(size & 0xFF),
        static_cast<uint8_t>((size >> 8) & 0xFF),
        static_cast<uint8_t>((size >> 16) & 0xFF),
        static_cast<uint8_t>((size >> 24) & 0xFF),
    };
    ofs.write(reinterpret_cast<const char*>(header), 4);
    // LMP_Load() stores lightmap rows bottom-up, while P5 rasters are
    // top-down. Mirror the rows so export-lmp -> import-lmp is byte-exact.
    for (uint32_t row = size; row-- > 0; )
    {
        ofs.write(reinterpret_cast<const char*>(luma.data() + row * size),
                  static_cast<std::streamsize>(size));
    }
    return ofs.good();
}

int cmd_terrain(int argc, char** argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── export-lmp ────────────────────────────────────────────────────────────
    if (sub == "export-lmp")
    {
        if (argc < 3)
        {
            std::cerr << "terrain export-lmp: missing <input.lmp>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = opt_val(argc, argv, "-o");
        if (!out_path)
        {
            std::cerr << "terrain export-lmp: missing -o <output.pgm>\n";
            return 1;
        }

        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain export-lmp: file not found: " << path << "\n";
            return 2;
        }

        pics_s pics = {};
        if (!LMP_Load(path.c_str(), pics))
        {
            std::cerr << "terrain export-lmp: failed to load " << path << "\n";
            return 3;
        }

        if (pics.num_pic_ == 0 || !pics.pics_)
        {
            std::cerr << "terrain export-lmp: no pictures in file\n";
            Pic_FreePics(pics);
            return 3;
        }

        // Export all pics; if more than one, suffix the filename.
        bool any_fail = false;
        for (int i = 0; i < pics.num_pic_; ++i)
        {
            const pic_s* pic = &pics.pics_[i];
            if (!pic->pixels_) continue;

            std::string actual_path = out_path;
            if (pics.num_pic_ > 1)
            {
                // Insert index before extension: e.g. "out.pgm" -> "out_0.pgm"
                std::filesystem::path p(out_path);
                std::string stem = p.stem().string() + "_" + std::to_string(i);
                actual_path = (p.parent_path() / (stem + p.extension().string())).string();
            }

            if (!write_pgm16(actual_path.c_str(), pic))
            {
                std::cerr << "terrain export-lmp: write failed: " << actual_path << "\n";
                any_fail = true;
            }
            else
            {
                std::cout << "Wrote " << actual_path
                          << " (" << pic->width_ << "x" << pic->height_ << ")\n";
            }
        }

        Pic_FreePics(pics);
        return any_fail ? 4 : 0;
    }

    // ── import-lmp ────────────────────────────────────────────────────────────
    if (sub == "import-lmp")
    {
        if (argc < 3)
        {
            std::cerr << "terrain import-lmp: missing <input.pgm>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = opt_val(argc, argv, "-o");
        if (!out_path)
        {
            std::cerr << "terrain import-lmp: missing -o <output.lmp>\n";
            return 1;
        }
        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain import-lmp: file not found: " << path << "\n";
            return 2;
        }

        uint32_t size = 0;
        std::vector<uint8_t> luma;
        std::string err;
        if (!read_pgm8(path.c_str(), size, luma, err))
        {
            std::cerr << "terrain import-lmp: " << err << ": " << path << "\n";
            return 3;
        }
        if (!write_lmp(out_path, size, luma))
        {
            std::cerr << "terrain import-lmp: write failed: " << out_path << "\n";
            return 4;
        }
        std::cout << "Wrote " << out_path << " (" << size << "x" << size << ")\n";
        return 0;
    }

    // ── export-ctr ────────────────────────────────────────────────────────────
    if (sub == "export-ctr")
    {
        if (argc < 3)
        {
            std::cerr << "terrain export-ctr: missing <input.ctr>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = opt_val(argc, argv, "-o");
        if (!out_path)
        {
            std::cerr << "terrain export-ctr: missing -o <output.json>\n";
            return 1;
        }

        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain export-ctr: file not found: " << path << "\n";
            return 2;
        }

        ctr_s ctr = {};
        if (!CTR_Load(path.c_str(), ctr))
        {
            std::cerr << "terrain export-ctr: failed to load " << path << "\n";
            return 3;
        }

        if (!write_ctr_json(out_path, ctr))
        {
            std::cerr << "terrain export-ctr: write failed: " << out_path << "\n";
            CTR_Free(ctr);
            return 4;
        }

        std::cout << "Wrote " << out_path << " (" << ctr.num_item_ << " items)\n";
        CTR_Free(ctr);
        return 0;
    }

    // ── info ──────────────────────────────────────────────────────────────────
    if (sub == "info")
    {
        if (argc < 3)
        {
            std::cerr << "terrain info: missing <input>\n";
            return 1;
        }
        std::string path = argv[2];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "terrain info: file not found: " << path << "\n";
            return 2;
        }

        std::string ext = std::filesystem::path(path).extension().string();
        // Normalise to lowercase
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (ext == ".lmp")
        {
            pics_s pics = {};
            if (!LMP_Load(path.c_str(), pics))
            {
                std::cerr << "terrain info: failed to load " << path << "\n";
                return 3;
            }
            std::cout << "File:    " << path << "\n";
            std::cout << "Type:    LMP (lightmap)\n";
            std::cout << "Pics:    " << pics.num_pic_ << "\n";
            for (int i = 0; i < pics.num_pic_; ++i)
            {
                const pic_s* p = &pics.pics_[i];
                std::cout << "  [" << i << "] " << p->width_ << "x" << p->height_ << "\n";
            }
            Pic_FreePics(pics);
        }
        else if (ext == ".ctr")
        {
            ctr_s ctr = {};
            if (!CTR_Load(path.c_str(), ctr))
            {
                std::cerr << "terrain info: failed to load " << path << "\n";
                return 3;
            }
            std::cout << "File:    " << path << "\n";
            std::cout << "Type:    CTR (quad-tree)\n";
            std::cout << "Items:   " << ctr.num_item_ << "\n";
            CTR_Free(ctr);
        }
        else
        {
            std::cerr << "terrain info: unrecognised extension '" << ext
                      << "' (expected .lmp or .ctr)\n";
            return 1;
        }
        return 0;
    }

    std::cerr << "terrain: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
