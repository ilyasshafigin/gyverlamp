Import("env")

# Include toolchain paths so clangd/LSP sees Arduino/ESP8266 headers
env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)

# Place compile_commands.json in project root for LSP discovery
env.Replace(COMPILATIONDB_PATH="compile_commands.json")

# The pinned ESP32 Arduino package omits release optimization in its PlatformIO build.
if env.PioPlatform().name == "espressif32" and env.GetBuildType() == "release":
    env.AppendUnique(CCFLAGS=["-Os"])

# Four TX blocks prevent audio-capture flicker on S3; reserves one RMT output.
if env.PioPlatform().name == "espressif32" and env.BoardConfig().get("build.mcu") == "esp32s3":
    env.AppendUnique(CPPDEFINES=[("LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS_MULTIPLIER", 4)])
