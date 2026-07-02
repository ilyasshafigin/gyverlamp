Import("env")

# Include toolchain paths so clangd/LSP sees Arduino/ESP8266 headers
env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)

# Place compile_commands.json in project root for LSP discovery
env.Replace(COMPILATIONDB_PATH="compile_commands.json")
