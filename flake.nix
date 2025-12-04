{
  inputs = {
    nix-ros-overlay.url = "github:lopsided98/nix-ros-overlay/ros1-25.05";
    nixpkgs.follows = "nix-ros-overlay/nixpkgs";
  };

  outputs = { self, nix-ros-overlay, nixpkgs }:
    nix-ros-overlay.inputs.flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
            cudaSupport = true;
            allowBroken = true;
            permittedInsecurePackages =
              [ "freeimage-3.18.0-unstable-2024-04-18" ];
          };
          overlays = [ nix-ros-overlay.overlays.default ];
        };
      in {
        devShells.default = pkgs.mkShell {
          name = "ROS";

          NIX_CFLAGS_COMPILE = pkgs.lib.concatStringsSep " " [
            "-I${pkgs.urdfdom-headers}/include/urdfdom_headers"
            "-Wno-error=implicit-function-declaration"
            "-Wno-error=incompatible-pointer-types"
            "-Wno-error=int-conversion"
          ];
          NIX_CXXFLAGS_COMPILE = pkgs.lib.concatStringsSep " " [
            "-I${pkgs.urdfdom-headers}/include/urdfdom_headers"
            "-Wno-error=implicit-function-declaration"
            "-Wno-error=incompatible-pointer-types"
            "-Wno-error=int-conversion"
          ];

          shellHook = ''
            export SHELL=/run/current-system/sw/bin/zsh
            source /home/admin/Repositories/Quadruped/devel/setup.sh
          '';

          packages = with pkgs; [
            # UI Dependencies

            # C++
            stdenv.cc
            cmake
            pkg-config
            gnumake
            clang-tools
            llvmPackages.openmp
            gdb
            valgrind
            ninja

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
            cyclonedds
            cyclonedds-cxx
            plutovg
            lunasvg

            # Project Dependencies
            cudaPackages.cudatoolkit
            opencv
            lcm
            urdfdom-headers

            # ROS Dependencies
            (with pkgs.rosPackages.noetic;
              buildEnv {
                paths = [
                  ### DESKTOP FULL ###
                  # desktop
                  angles
                  common-tutorials
                  geometry-tutorials
                  joint-state-publisher-gui
                  robot
                  ros-tutorials
                  roslint
                  urdf-tutorial
                  visualization-tutorials

                  # viz
                  ros-base
                  rviz

                  # rqt-common-plugins
                  rqt-action
                  rqt-bag
                  rqt-bag-plugins
                  rqt-console
                  rqt-dep
                  rqt-graph
                  rqt-image-view
                  rqt-launch
                  rqt-logger-level
                  rqt-msg
                  rqt-plot
                  rqt-publisher
                  rqt-py-common
                  rqt-py-console
                  rqt-reconfigure
                  rqt-service-caller
                  rqt-shell
                  rqt-srv
                  rqt-top
                  rqt-topic

                  # rqt-web
                  qt-gui
                  rospy
                  rqt-gui
                  rqt-gui-py
                  # webkit-dependency

                  rqt-robot-plugins

                  # perception
                  image-common
                  image-pipeline
                  # image-transport-plugins
                  laser-pipeline
                  perception-pcl
                  vision-opencv

                  # simulators
                  gazebo-ros-pkgs
                  stage-ros

                  urdf-sim-tutorial

                  ### MISC ###
                  controller-interface
                  gazebo-ros-control
                  joint-state-controller
                  effort-controllers
                  joint-trajectory-controller
                  move-base-msgs
                ];
              })
          ];

          VULKAN_SDK = "${pkgs.vulkan-headers}";
          VK_LAYER_PATH =
            "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";

          # TRT_LIBPATH = "${pkgs.tensorrt}/lib";
          CUDA_PATH = "${pkgs.cudaPackages.cudatoolkit}";
          CUDA_TOOLKIT_ROOT_DIR = "${pkgs.cudaPackages.cudatoolkit}";
        };
      });

  nixConfig = {
    extra-substituters = [ "https://ros.cachix.org" ];
    extra-trusted-public-keys =
      [ "ros.cachix.org-1:dSyZxI8geDCJrwgvCOHDoAfOm5sV1wCPjBkKL+38Rvo=" ];
  };
}
