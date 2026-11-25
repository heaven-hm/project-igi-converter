#include "pch.h"
#include "cmd_graph.h"
#include "graph_parser.h"

static void print_usage()
{
    std::cerr <<
        "Usage:\n"
        "  igi1conv graph export <input.dat> -o <output.json>\n"
        "  igi1conv graph table  <input.dat> -o <output.md>\n"
        "  igi1conv graph info   <input.dat>\n"
        "  igi1conv graph dump   <input.dat>\n";
}

// Return value of named option, or nullptr.  Accepts both `-o` and
// `--out` (and `-out`) so the GUI can pass either form.
static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

// Convenience: look up the output path under any of the common
// spellings (-o, --out, -out).  Returns nullptr if none were given.
static const char* out_opt(int argc, char** argv)
{
    const char* p = opt_val(argc, argv, "-o");
    if (!p) p = opt_val(argc, argv, "--out");
    if (!p) p = opt_val(argc, argv, "-out");
    if (!p) p = opt_val(argc, argv, "--output");
    return p;
}

// Material mapping (0..23) - same table the IGI engine and the
// `igi-graph-editor` use.  Kept in the order the engine documents
// the AI navigation graph.  A sentinel string is returned for
// unknown / out-of-range material indices so the dump never
// crashes and the user always sees something useful.
static const char* material_name(int m)
{
    static const char* names[] = {
        "Air", "Ground", "Water", "Wood", "Carton",
        "StrongMetal", "NormalMetal", "SoftMetal", "Flesh",
        "BloodyFlesh", "Textiles", "Concrete", "Runway", "Rug",
        "Glass", "Plastic", "Porcelain", "Rubber", "Fence",
        "Gravel", "Snow", "HumanCollision", "MetalLadder",
        "MetalFence",
    };
    constexpr int N = (int)(sizeof(names) / sizeof(names[0]));
    if (m < 0 || m >= N) return "Unknown";
    return names[m];
}

// Minimal JSON string escape
static std::string json_str(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s)
    {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    out += '"';
    return out;
}

// Pretty-print a JSON key/number pair (no trailing comma).
static std::string json_kv(const char* key, long long v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "\"%s\": %lld", key, v);
    return std::string(buf);
}
static std::string json_kv_double(const char* key, double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "\"%s\": %.6f", key, v);
    return std::string(buf);
}
static std::string json_kv_str(const char* key, const std::string& v) {
    return std::string("\"") + key + "\": " + json_str(v);
}

static void write_graph_json(std::ostream& os, const GraphFile& gf)
{
    if (!gf.valid)
    {
        os << "{\"error\": " << json_str(gf.error) << "}\n";
        return;
    }

    os << "{\n";
    // ── File metadata ──
    os << "  " << json_kv("valid", (long long)gf.valid) << ",\n";
    os << "  " << json_kv("is_legacy", (long long)gf.is_legacy) << ",\n";
    os << "  " << json_kv("max_nodes", (long long)gf.max_nodes) << ",\n";
    os << "  " << json_kv("node_count", (long long)gf.nodes.size()) << ",\n";
    os << "  " << json_kv("edge_count", (long long)gf.edges.size()) << ",\n";

    // ── Nodes ──
    os << "  \"nodes\": [\n";
    for (size_t i = 0; i < gf.nodes.size(); ++i)
    {
        const GraphNode& n = gf.nodes[i];
        if (i) os << ",\n";
        os << "    {\n";
        os << "      " << json_kv("id", (long long)n.id) << ",\n";
        os << "      " << json_kv_double("x", n.x) << ",\n";
        os << "      " << json_kv_double("y", n.y) << ",\n";
        os << "      " << json_kv_double("z", n.z) << ",\n";
        os << "      " << json_kv_double("gamma", n.gamma) << ",\n";
        os << "      " << json_kv_double("radius", n.radius) << ",\n";
        os << "      " << json_kv("material", (long long)n.material) << ",\n";
        os << "      " << json_kv_str("material_name", material_name(n.material)) << ",\n";
        os << "      " << json_kv_str("criteria", n.criteria) << ",\n";
        os << "      " << json_kv("link1", (long long)n.link1) << ",\n";
        os << "      " << json_kv("link2", (long long)n.link2) << ",\n";
        os << "      \"legacy_link_targets\": [";
        for (size_t j = 0; j < n.legacy_link_targets.size(); ++j) {
            if (j) os << ", ";
            os << n.legacy_link_targets[j];
        }
        os << "],\n";
        os << "      \"legacy_link_types\": [";
        for (size_t j = 0; j < n.legacy_link_types.size(); ++j) {
            if (j) os << ", ";
            os << n.legacy_link_types[j];
        }
        os << "],\n";
        os << "      " << json_kv("legacy_graph_link", (long long)n.legacy_graph_link) << ",\n";
        os << "      " << json_kv("legacy_graph_link_tgt", (long long)n.legacy_graph_link_tgt) << ",\n";
        os << "      " << json_kv("legacy_graph_link_typ", (long long)n.legacy_graph_link_typ) << "\n";
        os << "    }";
    }
    os << "\n  ],\n";

    // ── Edges ──
    os << "  \"edges\": [\n";
    for (size_t i = 0; i < gf.edges.size(); ++i)
    {
        const GraphEdge& e = gf.edges[i];
        if (i) os << ",\n";
        os << "    {"
           << json_kv("from", (long long)e.node1) << ", "
           << json_kv("to", (long long)e.node2) << ", "
           << json_kv("link_type", (long long)e.link_type)
           << "}";
    }
    os << "\n  ]\n";
    os << "}\n";
}

// Markdown table export.  Every field from the GraphNode/GraphEdge
// structs is laid out in a structured, human-readable table so the
// user can see all XYZ, links, criteria, radius, gamma, material
// at a glance.
static void write_graph_table(std::ostream& os, const GraphFile& gf)
{
    if (!gf.valid) {
        os << "Error: " << gf.error << "\n";
        return;
    }

    os << "# IGI Graph: " << gf.max_nodes << " max nodes, "
       << gf.nodes.size() << " nodes, " << gf.edges.size() << " edges\n";
    if (gf.is_legacy) os << "*Format: legacy (tagged)*\n";
    os << "\n";

    // ── Nodes table ──
    os << "## Nodes (" << gf.nodes.size() << ")\n\n";
    if (gf.nodes.empty()) {
        os << "_No nodes._\n\n";
    } else {
        os << "| ID | X | Y | Z | Gamma | Radius | Material (name) | Criteria | Link 1 | Link 2 | Legacy Link Targets | Legacy Link Types | Legacy Graph Link | Graph Link Tgt | Graph Link Typ |\n";
        os << "|----:|---:|---:|---:|---:|---:|---:|---|---:|---:|---|---|---:|---:|---:|\n";
        for (const auto& n : gf.nodes) {
        os << "| " << n.id
           << " | " << n.x
           << " | " << n.y
           << " | " << n.z
           << " | " << n.gamma
           << " | " << n.radius
           << " | " << n.material
           << " (" << material_name(n.material) << ")"
           << " | " << n.criteria
           << " | " << n.link1
           << " | " << n.link2
           << " | ";
            for (size_t j = 0; j < n.legacy_link_targets.size(); ++j) {
                if (j) os << ", ";
                os << n.legacy_link_targets[j];
            }
            os << " | ";
            for (size_t j = 0; j < n.legacy_link_types.size(); ++j) {
                if (j) os << ", ";
                os << n.legacy_link_types[j];
            }
            os << " | " << n.legacy_graph_link
               << " | " << n.legacy_graph_link_tgt
               << " | " << n.legacy_graph_link_typ
               << " |\n";
        }
        os << "\n";
    }

    // ── Edges table ──
    os << "## Edges / Links (" << gf.edges.size() << ")\n\n";
    if (gf.edges.empty()) {
        os << "_No edges._\n";
    } else {
        os << "| From | To | Link Type |\n";
        os << "|----:|----:|---:|\n";
        for (const auto& e : gf.edges) {
            os << "| " << e.node1 << " | " << e.node2
               << " | " << e.link_type << " |\n";
        }
    }
}

int cmd_graph(int argc, char** argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];

    // ── export ────────────────────────────────────────────────────────────────
    if (sub == "export")
    {
        if (argc < 3)
        {
            std::cerr << "graph export: missing <input.dat>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = out_opt(argc, argv);

        if (!std::filesystem::exists(path))
        {
            std::cerr << "graph export: file not found: " << path << "\n";
            return 2;
        }

        GraphFile gf = GRAPH_Parse(path);
        if (!gf.valid)
        {
            std::cerr << "graph export: parse error: " << gf.error << "\n";
            return 3;
        }

        if (out_path)
        {
            std::ofstream ofs(out_path);
            if (!ofs)
            {
                std::cerr << "graph export: cannot write: " << out_path << "\n";
                return 4;
            }
            write_graph_json(ofs, gf);
        }
        else
        {
            write_graph_json(std::cout, gf);
        }
        return 0;
    }

    // ── info ──────────────────────────────────────────────────────────────────
    if (sub == "info")
    {
        if (argc < 3)
        {
            std::cerr << "graph info: missing <input.dat>\n";
            return 1;
        }
        std::string path = argv[2];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "graph info: file not found: " << path << "\n";
            return 2;
        }

        GraphFile gf = GRAPH_Parse(path);
        if (!gf.valid)
        {
            std::cerr << "graph info: parse error: " << gf.error << "\n";
            return 3;
        }

        std::cout << "File:       " << path << "\n";
        std::cout << "Max nodes:  " << gf.max_nodes << "\n";
        std::cout << "Nodes:      " << gf.nodes.size() << "\n";
        std::cout << "Edges:      " << gf.edges.size() << "\n";
        if (gf.is_legacy) std::cout << "Format:     legacy (tagged)\n";
        return 0;
    }

    // ── dump (text node/edge listing for the GUI) ────────────────────────────
    if (sub == "dump")
    {
        if (argc < 3)
        {
            std::cerr << "graph dump: missing <input.dat>\n";
            return 1;
        }
        std::string path = argv[2];

        if (!std::filesystem::exists(path))
        {
            std::cerr << "graph dump: file not found: " << path << "\n";
            return 2;
        }

        GraphFile gf = GRAPH_Parse(path);
        if (!gf.valid)
        {
            std::cerr << "graph dump: parse error: " << gf.error << "\n";
            return 3;
        }

        std::cout << "File:       " << path << "\n";
        std::cout << "Max nodes:  " << gf.max_nodes << "\n";
        std::cout << "Nodes:      " << gf.nodes.size() << "\n";
        std::cout << "Edges:      " << gf.edges.size() << "\n";
        if (gf.is_legacy) std::cout << "Format:     legacy (tagged)\n";
        std::cout << "\n=== Nodes ===\n";
        for (const auto& n : gf.nodes) {
            std::cout << "  [" << n.id << "] pos=(" << n.x << ", " << n.y
                      << ", " << n.z << ") gamma=" << n.gamma
                      << " radius=" << n.radius
                      << " material=" << n.material
                      << " (" << material_name(n.material) << ")"
                      << " criteria=\"" << n.criteria << "\""
                      << " link1=" << n.link1 << " link2=" << n.link2
                      << "\n";
        }
        std::cout << "\n=== Edges ===\n";
        for (const auto& e : gf.edges) {
            std::cout << "  " << e.node1 << " -> " << e.node2
                      << " (type=" << e.link_type << ")\n";
        }
        return 0;
    }

    // ── table (.md) ───────────────────────────────────────────────────────────
    if (sub == "table" || sub == "md")
    {
        if (argc < 3)
        {
            std::cerr << "graph table: missing <input.dat>\n";
            return 1;
        }
        std::string path = argv[2];
        const char* out_path = out_opt(argc, argv);

        if (!std::filesystem::exists(path))
        {
            std::cerr << "graph table: file not found: " << path << "\n";
            return 2;
        }

        GraphFile gf = GRAPH_Parse(path);
        if (!gf.valid)
        {
            std::cerr << "graph table: parse error: " << gf.error << "\n";
            return 3;
        }

        if (out_path)
        {
            std::ofstream ofs(out_path);
            if (!ofs)
            {
                std::cerr << "graph table: cannot write: " << out_path << "\n";
                return 4;
            }
            write_graph_table(ofs, gf);
        }
        else
        {
            write_graph_table(std::cout, gf);
        }
        return 0;
    }

    std::cerr << "graph: unknown subcommand '" << sub << "'\n";
    print_usage();
    return 1;
}
