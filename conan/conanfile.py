from conans import ConanFile, CMake
from conan.tools.google import BazelDeps, bazel_layout

class ConanDeps(ConanFile):
    name = "mono"
    settings = "os", "compiler", "build_type", "arch"
    generators = "BazelDeps", "BazelToolchain"
    requires = ["abseil/20220623.1", "glog/0.6.0", "simdjson/3.6.4"]

    def layout(self):
        # DEPRECATED: Default generators folder will be "conan" in Conan 2.x
        self.folders.generators = "conan"
        bazel_layout(self)

    def generate(self):
        bz = BazelDeps(self)
        bz.generate()
