# Native converter review context

This branch builds one C++17 CLI for its game. IGI 1 and IGI 2 header/opcode/mesh/terrain differences must remain explicit. Check source-output aliases, archive traversal, binary bounds, integer range validation and unchanged template payloads. Verify template packing protects both template and bitmap inputs.

Representative real-format verification is mandatory; missing fixtures fail. Codec losses and opaque copy-only operations must be stated, never counted as exact transcoding. Game installers/executables/PDBs and fixture assets must not be committed or included in converter release packages.

Vendored nlohmann/json 3.11.3 is upstream dependency code with its original MIT license. CLI test/packaging subprocess calls intentionally run the selected local built binary, use argument arrays and do not invoke a shell. The supported macos-15-intel GitHub runner label may be absent from older actionlint versions; verify it against GitHub's current official documentation.

Only OpenRouter free models may be launched. Use the documented agent fallback when OpenQodex cannot start a permitted independent reviewer; mark the report as same-agent when applicable.
