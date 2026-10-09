#include "pch.h"
#include "cmd_mef.h"
#include "mef_native.h"
#include "mef_exporter.h"
#include "mef_parser.h"
#include "mef_compiler.h"
#include "dat_parser.h"
#include "tex_parser.h"
#include <set>

namespace fs = std::filesystem;

static void print_mef_help()
{
    std::cout <<
        "Usage: igi2conv mef <subcommand> [options]\n"
        "\n"
        "Subcommands:\n"
        "  export  <input.mef|mex> -o <output.obj> [--dat <file.dat> --texdir <dir>]\n"
        "  export  <folder/> -o <output_dir> --batch [--dat <file.dat> --texdir <dir>]\n"
        "  dump    <input.mef> [-o <output.txt>]\n"
        "  info    <input.mef>\n"
        "  bundle  <input.mef> -o <outdir> --dat <file.dat> --texdir <dir>\n"
        "  to-text <input.mef> -o <output.json>  (native IGI 2 model JSON)\n"
        "  compile <input.json> -o <output.mef> (native IGI 2 JSON -> MEF)\n"
        "\n"
        "Exit codes: 0=success 1=bad args 2=file not found 3=parse error 4=write error\n";
}

static int do_mef_export(const std::string& input, const std::string& outpath,
                         const std::string& datPath = {}, const std::string& texDir = {})
{
    if (!fs::exists(input))
    {
        std::cerr << "mef: file not found: " << input << "\n";
        return 2;
    }

    // Detect binary vs text format
    bool isBinary = false;
    {
        std::ifstream probe(input, std::ios::binary);
        char magic[4] = {0};
        probe.read(magic, 4);
        isBinary = (std::memcmp(magic, "ILFF", 4) == 0);
    }

    if (isBinary)
    {
        ParsedGeometry geo;
        try { geo = ParseMefFile(input); }
        catch (const std::exception& e) {
            std::cerr << "mef: parse error: " << e.what() << "\n";
            return 3;
        }

        if (!MefExporter::ExportToObj(geo, outpath, datPath)) {
            std::cerr << "mef: failed to write OBJ: " << outpath << "\n";
            return 4;
        }
    }
    else
    {
        MEFParser parser;
        std::vector<MEFObject> objects;
        try { objects = parser.parse_file(input); }
        catch (const std::exception& e) {
            std::cerr << "mef: parse error: " << e.what() << "\n";
            return 3;
        }
        if (objects.empty() || objects[0].faces.empty()) {
            std::cerr << "mef: text MEF has no geometry\n";
            return 3;
        }
        if (!MefExporter::ExportTextMefToObj(objects, outpath)) {
            std::cerr << "mef: failed to write OBJ: " << outpath << "\n";
            return 4;
        }
    }

    std::cout << "mef: exported to " << outpath << "\n";
    return 0;
}

static int do_mef_dump(const std::string& input, const std::string& outpath)
{
    if (!fs::exists(input))
    {
        std::cerr << "mef: file not found: " << input << "\n";
        return 2;
    }

    std::ostream* out_ptr = &std::cout;
    std::ofstream fout;
    if (!outpath.empty())
    {
        fout.open(outpath);
        if (!fout.is_open())
        {
            std::cerr << "mef: cannot open output file: " << outpath << "\n";
            return 4;
        }
        out_ptr = &fout;
    }
    std::ostream& out = *out_ptr;

    // Check if it's an ASCII or Binary MEF
    std::ifstream file(input, std::ios::binary);
    char magic[4] = {0};
    file.read(magic, 4);
    file.close();

    if (std::memcmp(magic, "ILFF", 4) == 0)
    {
        // Binary MEF
        ParsedGeometry geo;
        try { geo = ParseMefFile(input); }
        catch (const std::exception& e) {
            std::cerr << "mef: binary parse error: " << e.what() << "\n";
            return 3;
        }

        out << "file: " << input << " (Binary MEF)\n";
        out << "materials: " << geo.renderBlocks.size() << "\n";
        for (size_t i = 0; i < geo.renderBlocks.size(); ++i) {
            out << "  material[" << i << "] mat_slot=" << geo.renderBlocks[i].materialSlot << "\n";
        }

        out << "attachments: " << geo.attachments.size() << "\n";
        for (size_t i = 0; i < geo.attachments.size(); ++i) {
            const auto& atta = geo.attachments[i];
            out << "  ATTA[" << i << "] bone=" << atta.boneId << " offset=(" 
                << atta.pos.x << ", " << atta.pos.y << ", " << atta.pos.z << ") "
                << "name=\"" << atta.name << "\"\n";
        }

        if (!geo.rawChunks.empty()) {
            out << "\nchunks: " << geo.rawChunks.size() << "\n";
            for (const auto& rc : geo.rawChunks) {
                std::string cc(rc.fourcc, 4);
                out << "  " << cc << " (size: " << rc.data.size() << ")\n";
            }
        }
        return 0;
    }

    // ASCII MEF fallback
    MEFParser parser;
    std::vector<MEFObject> objects;
    try { objects = parser.parse_file(input); }
    catch (const std::exception& e) {
        std::cerr << "mef: parse error: " << e.what() << "\n";
        return 3;
    }

    out << "file: " << input << " (ASCII MEF)\n";
    out << "objects: " << objects.size() << "\n\n";

    for (size_t oi = 0; oi < objects.size(); ++oi)
    {
        const MEFObject& obj = objects[oi];
        out << "--- object[" << oi << "]: " << obj.name << " ---\n";
        out << "  vertices:  " << obj.vertices.size() << "\n";
        out << "  normals:   " << obj.normals.size() << "\n";
        out << "  faces:     " << obj.faces.size() << "\n";
        out << "  uvs:       " << obj.uvs.size() << "\n";
        out << "  materials: " << obj.materials.size() << "\n";
        for (size_t mi = 0; mi < obj.materials.size(); ++mi)
        {
            const MEFMaterial& mat = obj.materials[mi];
            out << "    mat[" << mi << "] index=" << mat.index
                << " name=\"" << mat.name << "\""
                << " shininess=" << mat.shininess
                << (mat.has_collision ? " [collision]" : "") << "\n";
        }
        if (!obj.parse_errors.empty())
        {
            out << "  parse_errors: " << obj.parse_errors.size() << "\n";
            for (const auto& err : obj.parse_errors)
                out << "    " << err << "\n";
        }
        out << "\n";
    }

    return 0;
}

static int do_mef_info(const std::string& input)
{
    if (!fs::exists(input))
    {
        std::cerr << "mef: file not found: " << input << "\n";
        return 2;
    }

    ParsedGeometry geo;
    try
    {
        geo = ParseMefFile(input);
    }
    catch (const std::exception& e)
    {
        std::cerr << "mef: parse error: " << e.what() << "\n";
        return 3;
    }

    std::cout << "file:            " << input << "\n";
    std::cout << "layout:          " << geo.renderLayout << "\n";
    std::cout << "model_type:      " << geo.modelType << "\n";
    std::cout << "vertices:        " << geo.vertices.size() << "\n";
    std::cout << "triangles:       " << geo.triangles.size() << "\n";
    std::cout << "render_blocks:   " << geo.renderBlocks.size() << "\n";
    std::cout << "bones:           " << geo.bones.size() << "\n";
    std::cout << "attachments:     " << geo.attachments.size() << "\n";
    std::cout << "collision_verts: " << geo.collisionVertexCount << "\n";
    std::cout << "collision_faces: " << geo.collisionFaceCount << "\n";

    if (!geo.bones.empty())
    {
        std::cout << "\nbones:\n";
        for (size_t i = 0; i < geo.bones.size(); ++i)
        {
            const BoneInfo& b = geo.bones[i];
            std::cout << "  [" << i << "] " << b.name
                      << " parent=" << b.parent << "\n";
        }
    }
    return 0;
}

static int do_mef_to_text(const std::string& input, const std::string& outpath)
{
    if (!fs::exists(input))
    {
        std::cerr << "mef to-text: file not found: " << input << "\n";
        return 2;
    }

    // Only binary MEF (ILFF magic) is accepted as input
    {
        std::ifstream probe(input, std::ios::binary);
        char magic[4] = {0};
        probe.read(magic, 4);
        if (std::memcmp(magic, "ILFF", 4) != 0)
        {
            std::cerr << "mef to-text: input is not a binary MEF (expected ILFF magic): " << input << "\n";
            return 3;
        }
    }

    ParsedGeometry geo;
    try { geo = ParseMefFile(input); }
    catch (const std::exception& e)
    {
        std::cerr << "mef to-text: parse error: " << e.what() << "\n";
        return 3;
    }

    if (!MefExporter::ExportToMefAscii(geo, outpath))
    {
        std::cerr << "mef to-text: failed to write text MEF: " << outpath << "\n";
        return 4;
    }

    // Write sidecar with all original ILFF chunks so mef compile can restore them.
    std::string sidecarPath = outpath;
    size_t extPos = sidecarPath.find_last_of(".");
    std::string ext = (extPos != std::string::npos) ? sidecarPath.substr(extPos) : "";
    if (ext == ".mex") {
        sidecarPath += ".extra";
    } else {
        if (extPos != std::string::npos) sidecarPath = sidecarPath.substr(0, extPos) + ".mex";
        else sidecarPath += ".mex";
    }

    if (!geo.rawChunks.empty())
        MefExporter::WriteMefSidecar(geo.rawChunks, sidecarPath);

    std::cout << "mef to-text: exported to " << outpath << "\n";
    if (!geo.rawChunks.empty())
        std::cout << "mef to-text: sidecar written to " << sidecarPath << "\n";
    return 0;
}

static int do_mef_compile(const std::string& input, const std::string& outpath)
{
    if (!fs::exists(input))
    {
        std::cerr << "mef compile: file not found: " << input << "\n";
        return 2;
    }

    if (!MefCompiler::Compile(input, outpath))
    {
        std::cerr << "mef compile: compilation failed for: " << input << "\n";
        return 3;
    }

    std::cout << "mef compile: compiled to " << outpath << "\n";
    return 0;
}

struct MergeSource {
    std::string stem;     // filename stem used for DAT lookup
    std::string dir;      // directory of the source MEF (for local fallback)
    uint32_t    mOffset;  // global material slot offset for this model
    uint32_t    mCount;   // number of material slots from this model
};

// Resolve textures for all merged sources: DAT+TEX first, then local mat_N.* fallback.
// Writes TGA files to bundleDir and returns the texNames vector (size = totalSlots).
static std::vector<std::string> ResolveAndExportTextures(
    const std::vector<MergeSource>& sources,
    const std::string& datPath,
    const std::string& texDir,
    const std::string& bundleDir,
    uint32_t totalSlots)
{
    std::vector<std::string> texNames(totalSlots);
    for (uint32_t i = 0; i < totalSlots; ++i)
        texNames[i] = "mat_" + std::to_string(i) + ".tga";

    DATFile dat;
    if (!datPath.empty()) dat = DAT_Parse(datPath);

    static const char* kExts[] = { ".tga", ".TGA", ".tex", ".TEX", ".bmp", ".BMP", nullptr };

    for (const auto& src : sources) {
        std::vector<std::string> datTextures;
        for (const auto& entry : dat.models) {
            if (entry.modelName == src.stem ||
                entry.modelName.find(src.stem) != std::string::npos) {
                datTextures = entry.textures;
                break;
            }
        }

        for (uint32_t local = 0; local < src.mCount; ++local) {
            uint32_t global = src.mOffset + local;
            std::string tgaName = "mat_" + std::to_string(global) + ".tga";
            std::string tgaPath = bundleDir + "/" + tgaName;
            bool written = false;

            if (local < datTextures.size() && !texDir.empty()) {
                std::string texFile = texDir + "/" + datTextures[local] + ".tex";
                if (fs::exists(texFile)) {
                    TEXFile tex = TEX_Parse(texFile);
                    if (tex.valid && !tex.images.empty())
                        written = TEX_WriteTGA(tgaPath, tex.images[0]);
                    if (written)
                        std::cout << "  texture: " << datTextures[local] << ".tex -> " << tgaName << "\n";
                }
            }

            if (!written) {
                for (int ei = 0; kExts[ei]; ++ei) {
                    std::string localFile = src.dir + "/mat_" + std::to_string(local) + kExts[ei];
                    if (!fs::exists(localFile)) continue;
                    try {
                        fs::copy_file(localFile, tgaPath, fs::copy_options::overwrite_existing);
                        std::cout << "  texture (local): " << localFile << " -> " << tgaName << "\n";
                        written = true;
                    } catch (...) {}
                    break;
                }
            }

            if (!written)
                std::cerr << "  warning: texture slot " << global << " (" << src.stem << " mat_" << local << ") not found\n";
        }
    }
    return texNames;
}

static void MergeGeometryRecursive(ParsedGeometry& baseGeo, const std::string& currentPath,
                                    glm::mat4 transform, int depth,
                                    std::vector<MergeSource>& sources) {
    if (depth > 20) return;

    ParsedGeometry child;
    try { child = ParseMefFile(currentPath); } catch (...) { return; }

    uint32_t vOffset = static_cast<uint32_t>(baseGeo.vertices.size());
    uint32_t fOffset = static_cast<uint32_t>(baseGeo.triangles.size());
    uint32_t mOffset = static_cast<uint32_t>(baseGeo.tamcRecords.size());

    for (const auto& tamc : child.tamcRecords) baseGeo.tamcRecords.push_back(tamc);
    for (const auto& tex  : child.pmtlTextures) baseGeo.pmtlTextures.push_back(tex);

    // Track this model as a texture source so we can resolve textures later
    if (!child.tamcRecords.empty() || !child.pmtlTextures.empty()) {
        MergeSource src;
        src.stem    = fs::path(currentPath).stem().string();
        src.dir     = fs::path(currentPath).parent_path().string();
        src.mOffset = mOffset;
        src.mCount  = static_cast<uint32_t>(child.tamcRecords.size());
        sources.push_back(src);
    }

    bool isBoneModel = (child.renderLayout.find("type1") != std::string::npos);
    bool hasRawPos = false;
    for (const auto& v : child.vertices) {
        if (v.rawPos.x != 0 || v.rawPos.y != 0 || v.rawPos.z != 0) { hasRawPos = true; break; }
    }
    for (auto v : child.vertices) {
        // Bone models: use v.pos (bone world offsets baked in). Others: use rawPos if available.
        glm::vec3 p = (isBoneModel || !hasRawPos) ? (v.pos * 40.96f) : v.rawPos;
        glm::vec4 tp = transform * glm::vec4(p, 1.0f);
        v.rawPos = glm::vec3(tp);
        v.pos = v.rawPos / 40.96f;
        glm::vec4 tn = transform * glm::vec4(v.normal, 0.0f);
        v.normal = glm::normalize(glm::vec3(tn));
        baseGeo.vertices.push_back(v);
    }

    for (auto tri : child.triangles) {
        baseGeo.triangles.push_back({tri[0] + vOffset, tri[1] + vOffset, tri[2] + vOffset});
    }

    for (auto rb : child.renderBlocks) {
        rb.triangleStart += fOffset;
        rb.materialSlot  += static_cast<int>(mOffset);
        baseGeo.renderBlocks.push_back(rb);
    }

    for (auto port : child.portals) {
        port.materialId += mOffset;
        for (auto& pv : port.verts) {
            glm::vec4 tp = transform * glm::vec4(pv, 1.0f);
            pv = glm::vec3(tp);
        }
        baseGeo.portals.push_back(port);
    }

    // Recurse into ATTA children — do NOT push ATTA records into baseGeo since
    // their geometry is inlined; a complete rigid model needs no external references.
    for (const auto& atta : child.mefAttachments) {
        glm::mat4 aMat(1.0f);
        aMat[0][0] = atta.r00; aMat[1][0] = atta.r01; aMat[2][0] = atta.r02;
        aMat[0][1] = atta.r03; aMat[1][1] = atta.r04; aMat[2][1] = atta.r05;
        aMat[0][2] = atta.r06; aMat[1][2] = atta.r07; aMat[2][2] = atta.r08;
        aMat[3][0] = atta.px;  aMat[3][1] = atta.py;  aMat[3][2] = atta.pz;

        glm::mat4 childTransform = transform * aMat;

        std::string childName(atta.name, strnlen(atta.name, 16));
        std::string dir = fs::path(currentPath).parent_path().string();
        std::string path1 = dir + "/" + childName + ".mef";
        std::string path2 = dir + "/" + childName + ".MEF";
        std::string path3 = dir + "/" + childName + ".mex";
        std::string path4 = dir + "/" + childName + ".MEX";

        if      (fs::exists(path1)) MergeGeometryRecursive(baseGeo, path1, childTransform, depth + 1, sources);
        else if (fs::exists(path2)) MergeGeometryRecursive(baseGeo, path2, childTransform, depth + 1, sources);
        else if (fs::exists(path3)) MergeGeometryRecursive(baseGeo, path3, childTransform, depth + 1, sources);
        else if (fs::exists(path4)) MergeGeometryRecursive(baseGeo, path4, childTransform, depth + 1, sources);
    }
}

static int do_mef_build_rigid(const std::string& input, const std::string& outpath,
                               const std::string& datPath = {}, const std::string& texDir = {}) {
    if (!fs::exists(input)) {
        std::cerr << "mef build-rigid: file not found: " << input << "\n";
        return 2;
    }

    ParsedGeometry baseGeo;
    baseGeo.modelType = 0;
    glm::mat4 identity(1.0f);
    std::vector<MergeSource> sources;
    MergeGeometryRecursive(baseGeo, input, identity, 0, sources);
    baseGeo.mefAttachments.clear();

    if (!MefCompiler::BuildRigidFromParsedGeometry(baseGeo, outpath)) {
        std::cerr << "mef build-rigid: failed to build rigid model\n";
        return 4;
    }
    std::cout << "mef build-rigid: exported to " << outpath << "\n";

    uint32_t totalSlots = static_cast<uint32_t>(baseGeo.tamcRecords.size());
    if (totalSlots > 0) {
        std::string outDir = fs::path(outpath).parent_path().string();
        if (outDir.empty()) outDir = ".";
        std::cout << "mef build-rigid: resolving " << totalSlots << " texture slot(s)...\n";
        ResolveAndExportTextures(sources, datPath, texDir, outDir, totalSlots);
    }

    std::cout << "mef build-rigid: complete — " << baseGeo.vertices.size() << " verts, "
              << baseGeo.triangles.size() << " tris, "
              << totalSlots << " materials, "
              << baseGeo.portals.size() << " portals\n";
    return 0;
}

int cmd_mef(int argc, char** argv)
{
    // argv[0] = "mef", argv[1] = subcommand
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        print_mef_help();
        return (argc < 2) ? 1 : 0;
    }

    std::string subcmd = argv[1];

    if (subcmd == "info")
    {
        if (argc < 3)
        {
            std::cerr << "mef info: missing input file\n";
            return 1;
        }
        return do_mef_info(argv[2]);
    }

    if (subcmd == "dump")
    {
        if (argc < 3)
        {
            std::cerr << "mef dump: missing input file\n";
            return 1;
        }

        std::string outpath;
        for (int i = 3; i < argc - 1; ++i)
        {
            if (std::string(argv[i]) == "-o")
            {
                outpath = argv[i + 1];
                break;
            }
        }
        return do_mef_dump(argv[2], outpath);
    }

    if (subcmd == "export")
    {
        if (argc < 3)
        {
            std::cerr << "mef export: missing input\n";
            return 1;
        }

        std::string outpath, datPath, texDir;
        for (int i = 3; i < argc - 1; ++i)
        {
            std::string a = argv[i];
            if (a == "-o")        { outpath = argv[i + 1]; ++i; }
            else if (a == "--dat")    { datPath = argv[i + 1]; ++i; }
            else if (a == "--texdir") { texDir  = argv[i + 1]; ++i; }
        }
        if (outpath.empty())
        {
            std::cerr << "mef export: missing -o <output.obj|output_dir>\n";
            return 1;
        }

        bool batch = false;
        for (int i = 3; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--batch") { batch = true; break; }
        }

        std::string input = argv[2];

        if (batch)
        {
            if (!fs::is_directory(input))
            {
                std::cerr << "mef export --batch: input is not a directory: " << input << "\n";
                return 2;
            }

            if (!fs::exists(outpath))
            {
                std::error_code ec;
                fs::create_directories(outpath, ec);
                if (ec)
                {
                    std::cerr << "mef: cannot create output dir: " << outpath << " (" << ec.message() << ")\n";
                    return 4;
                }
            }

            bool any_failed = false;
            for (const auto& entry : fs::directory_iterator(input))
            {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".mef" && ext != ".mex") continue;

                std::string stem = entry.path().stem().string();
                std::string obj_out = (fs::path(outpath) / (stem + ".obj")).string();

                int rc = do_mef_export(entry.path().string(), obj_out, datPath, texDir);
                if (rc != 0)
                {
                    std::cerr << "mef: error processing " << entry.path().filename().string() << " (rc=" << rc << ")\n";
                    any_failed = true;
                }
            }
            return any_failed ? 3 : 0;
        }
        else
        {
            return do_mef_export(input, outpath, datPath, texDir);
        }
    }

    if (subcmd == "bundle")
    {
        if (argc < 3)
        {
            std::cerr << "mef bundle: missing input file\n";
            return 1;
        }
        std::string input = argv[2];

        std::string outdir, dat_path, tex_dir;
        bool skipObj = false;
        for (int i = 3; i < argc; ++i)
        {
            std::string a = argv[i];
            if (a == "-o"       && i + 1 < argc) { outdir   = argv[++i]; }
            else if (a == "--dat"    && i + 1 < argc) { dat_path = argv[++i]; }
            else if (a == "--texdir" && i + 1 < argc) { tex_dir  = argv[++i]; }
            else if (a == "--no-obj") { skipObj = true; }
        }

        if (outdir.empty())
        {
            std::cerr << "mef bundle: missing -o <outdir>\n";
            return 1;
        }

        if (!fs::exists(input))
        {
            std::cerr << "mef bundle: file not found: " << input << "\n";
            return 2;
        }

        auto doBundleOne = [&](const std::string& mefPath) -> bool {
            std::string stem = fs::path(mefPath).stem().string();
            ParsedGeometry mergedGeo;
            mergedGeo.modelType = 0;
            std::vector<MergeSource> sources;
            MergeGeometryRecursive(mergedGeo, mefPath, glm::mat4(1.0f), 0, sources);
            mergedGeo.mefAttachments.clear();

            fs::path bundleDir = fs::path(outdir) / stem;
            std::error_code ec;
            fs::create_directories(bundleDir, ec);
            if (ec) {
                std::cerr << "mef bundle: cannot create dir: " << bundleDir.string() << "\n";
                return false;
            }

            uint32_t totalSlots = static_cast<uint32_t>(mergedGeo.tamcRecords.size());
            std::vector<std::string> texNames =
                ResolveAndExportTextures(sources, dat_path, tex_dir, bundleDir.string(), totalSlots);

            if (!MefExporter::ExportMergedToObjBundle(mergedGeo, stem, outdir, texNames, skipObj)) {
                std::cerr << "mef bundle: export failed for " << stem << "\n";
                return false;
            }
            std::cout << "mef bundle: exported to " << bundleDir.string()
                      << " (" << mergedGeo.vertices.size() << " verts, "
                      << totalSlots << " materials)\n";
            return true;
        };

        if (fs::is_directory(input))
        {
            bool any_failed = false;
            for (const auto& entry : fs::directory_iterator(input))
            {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".mef") continue;
                if (!doBundleOne(entry.path().string())) any_failed = true;
            }
            return any_failed ? 3 : 0;
        }
        else
        {
            return doBundleOne(input) ? 0 : 4;
        }
    }

    if (subcmd == "to-text")
    {
        if (argc < 3)
        {
            std::cerr << "mef to-text: missing input file\n";
            return 1;
        }
        std::string outpath;
        for (int i = 3; i < argc - 1; ++i)
        {
            if (std::string(argv[i]) == "-o") { outpath = argv[i + 1]; break; }
        }
        if (outpath.empty())
        {
            std::cerr << "mef to-text: missing -o <output.txt>\n";
            return 1;
        }
        return do_mef_to_text(argv[2], outpath);
    }

    if (subcmd == "compile")
    {
        if (argc < 3)
        {
            std::cerr << "mef compile: missing input file\n";
            return 1;
        }
        std::string outpath;
        for (int i = 3; i < argc - 1; ++i)
        {
            if (std::string(argv[i]) == "-o") { outpath = argv[i + 1]; break; }
        }
        if (outpath.empty())
        {
            std::cerr << "mef compile: missing -o <output.mef>\n";
            return 1;
        }
        return do_mef_compile(argv[2], outpath);
    }

    if (subcmd == "build-rigid")
    {
        if (argc < 3)
        {
            std::cerr << "mef build-rigid: missing input file\n";
            return 1;
        }
        std::string outpath, datPath, texDir;
        for (int i = 3; i < argc; ++i)
        {
            std::string a = argv[i];
            if      (a == "-o"       && i + 1 < argc) { outpath = argv[++i]; }
            else if (a == "--dat"    && i + 1 < argc) { datPath = argv[++i]; }
            else if (a == "--texdir" && i + 1 < argc) { texDir  = argv[++i]; }
        }
        if (outpath.empty()) outpath = argv[2];
        return do_mef_build_rigid(argv[2], outpath, datPath, texDir);
    }

    std::cerr << "mef: unknown subcommand '" << subcmd << "'\n";
    std::cerr << "Run 'igi1conv mef --help' for usage.\n";
    return 1;
}
