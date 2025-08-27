{
  inputs = {
    nixpkgs-unstable.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs-unstable, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs-unstable {
          inherit system;
          config = {
            allowUnfree = true;
          };
        };
      in {
        devShells.default = pkgs.mkShell {
          name = "VULKAN";

          shellHook = ''
            export SHELL=/run/current-system/sw/bin/zsh
          '';

          packages = [
            # C++ Tools
            pkgs.stdenv.cc
            pkgs.cmake
            pkgs.gnumake
            pkgs.clang-tools
            pkgs.llvmPackages.openmp
            pkgs.gdb
            pkgs.valgrind
            # Graphics drivers
            pkgs.mesa
            pkgs.libglvnd
            pkgs.linuxPackages.nvidia_x11
            # Vulkan
            pkgs.vulkan-headers
            pkgs.vulkan-loader
            pkgs.vulkan-validation-layers
            pkgs.vulkan-tools
            pkgs.vulkan-tools-lunarg
            pkgs.spirv-tools
            # Dependencies
            pkgs.shaderc
            pkgs.glfw
            pkgs.glm
            pkgs.freetype
            pkgs.openssl
          ];

          LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
            "${pkgs.glfw}/lib"
            "${pkgs.glm}/lib"
            "${pkgs.freetype}/lib"
            "${pkgs.vulkan-loader}/lib"
            "${pkgs.vulkan-validation-layers}/lib"
          ];

          PKG_CONFIG_PATH = pkgs.lib.makeSearchPath "lib/pkgconfig" [
            pkgs.shaderc.dev
          ];

          VULKAN_SDK = "${pkgs.vulkan-headers}";
          VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
          glm_DIR = "${pkgs.glm}";
          glfw3_DIR = "${pkgs.glfw}/lib/cmake/glfw3";
        };
      });
}
