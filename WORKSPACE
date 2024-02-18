load("@//conan/conan:dependencies.bzl", "load_conan_dependencies")

load_conan_dependencies()


load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
# Hedron's Compile Commands Extractor for Bazel
# https://github.com/hedronvision/bazel-compile-commands-extractor
http_archive(
    name = "hedron_compile_commands",
    # Replace the commit hash in both places (below) with the latest, rather than using the stale one here.
    # Even better, set up Renovate and let it do the work for you (see "Suggestion: Updates" in the README).
    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/eac41eefb5c19d9a2d2bcdd60d6989660288333d.tar.gz",
    strip_prefix = "bazel-compile-commands-extractor-eac41eefb5c19d9a2d2bcdd60d6989660288333d",
    # When you first run this tool, it'll recommend a sha256 hash to put here with a message like: "DEBUG: Rule 'hedron_compile_commands' indicated that a canonical reproducible form can be obtained by modifying arguments sha256 = ..."
)
load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")
hedron_compile_commands_setup()

