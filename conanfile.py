from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy


class ErlRecipe(ConanFile):
    name = "erl"
    version = "0.1"
    package_type = "library"

    # Optional metadata
    license = "MIT"
    author = "Tsche che@pydong.org"
    description = "Experimental reflective utility library."
    topics = () # TODO

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False], "fPIC": [True, False],
        "coverage": [True, False],
        "examples": [True, False]
    }
    default_options = {"shared": False, "fPIC": True, "coverage": False, "examples": True}
    generators = "CMakeToolchain", "CMakeDeps"

    exports_sources = "CMakeLists.txt", "include/*"
    
    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("rsl/0.1")
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)

    def build(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake = CMake(self)
            cmake.configure(
                variables={
                    "ENABLE_COVERAGE": self.options.coverage,
                    "ENABLE_EXAMPLES": self.options.examples
                }
            )
            cmake.build()

    def package(self):
        # copy(self, "include/erl/*", self.source_folder, self.package_folder)
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["erl"]
