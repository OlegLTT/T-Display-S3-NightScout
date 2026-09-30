"""Keep the linker map path relative for projects in non-ASCII directories."""

from os.path import join, relpath

Import("env")

# The ESP32 Windows linker cannot open the framework's absolute Unicode path.
map_path = relpath(
    join(env.subst("$BUILD_DIR"), env.subst("${PROGNAME}.map")),
    env.subst("$PROJECT_DIR"),
).replace("\\", "/")

env.Replace(
    LINKFLAGS=[
        '-Wl,-Map="%s"' % map_path if str(flag).startswith("-Wl,-Map=") else flag
        for flag in env.get("LINKFLAGS", [])
    ]
)
