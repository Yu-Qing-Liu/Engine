{
  inputs.nixpkgs-unstable.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs-unstable }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forSystems = f:
        (import nixpkgs-unstable { system = "x86_64-linux"; }).lib.genAttrs
        systems f;
    in {
      devShells = forSystems (system:
        let
          pkgs = import nixpkgs-unstable {
            inherit system;
            config.allowUnfree = true;
          };
        in {
          default = pkgs.mkShell {
            name = "VULKAN";
            shellHook = "export SHELL=/run/current-system/sw/bin/zsh";
            packages = with pkgs; [
              # Dev tools
              stdenv.cc
              cmake
              pkg-config
              gnumake
              clang-tools
              llvmPackages.openmp
              gdb
              valgrind
              # Drivers
              mesa
              # Vulkan
              vulkan-headers
              vulkan-loader
              vulkan-validation-layers
              vulkan-tools
              vulkan-tools-lunarg
              spirv-tools
              # Dependencies
              imgui
              shaderc
              glfw
              glm
              freetype
              openssl
              assimp
              libpqxx
            ];
            LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
              pkgs.glfw
              pkgs.glm
              pkgs.freetype.dev
              pkgs.vulkan-loader
              pkgs.vulkan-validation-layers
              pkgs.assimp.dev
              pkgs.openssl.dev
              pkgs.libpqxx.dev
            ];
            PKG_CONFIG_PATH = pkgs.lib.makeSearchPath "lib/pkgconfig" [
              pkgs.shaderc.dev
              pkgs.assimp.dev
              pkgs.openssl.dev
              pkgs.freetype.dev
              pkgs.libpqxx.dev
            ];
            VULKAN_SDK = "${pkgs.vulkan-headers}";
            VK_LAYER_PATH =
              "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
            glm_DIR = "${pkgs.glm}";
            glfw3_DIR = "${pkgs.glfw}/lib/cmake/glfw3";
            assimp_DIR = "${pkgs.assimp.dev}/lib/cmake/assimp-5.4}";
            OpenSSL_DIR = "${pkgs.openssl.dev}";
            freetype_DIR = "${pkgs.freetype.dev}";
            libpqxx_DIR = "${pkgs.libpqxx.dev}";
          };
        });
    };
}
