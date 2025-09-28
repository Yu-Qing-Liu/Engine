# syntax=docker/dockerfile:1
FROM nixos/nix

# Enable flakes
RUN mkdir -p /etc/nix && \
    printf "experimental-features = nix-command flakes\n" > /etc/nix/nix.conf

WORKDIR /app
COPY . .
# Ensure your script is executable if it's in the repo
RUN chmod +x ./build.sh

# Build with your devshell (no separate Nix packages needed here)
RUN nix develop -c ./build.sh

# Headless runtime bits inside the image
# - xorg.xorgserver gives Xvfb
# - mesa.drivers contains lavapipe (CPU Vulkan)
# - vulkan-loader is the Vulkan ICD loader
# - vulkan-tools for vulkaninfo (handy for debugging)
# - xkeyboard_config provides XKB keymaps (silences xkbcomp warnings)
RUN nix profile install \
      nixpkgs#xorg.xorgserver \
      nixpkgs#mesa.drivers \
      nixpkgs#vulkan-loader \
      nixpkgs#vulkan-tools \
      nixpkgs#xkeyboard_config

# Cache lavapipe ICD path (fixed across runs)
RUN echo $(nix eval --raw nixpkgs#mesa.drivers)/share/vulkan/icd.d/lvp_icd.json > /etc/vk_icd

# Small, explicit entrypoint for headless execution
# - create XDG_RUNTIME_DIR with correct perms (some libs require it)
# - unset WAYLAND_DISPLAY so GLFW chooses X11 (Xvfb)
# - start Xvfb and run your binary
RUN printf '%s\n' \
  '#!/usr/bin/env sh' \
  'set -eu' \
  'export DISPLAY=:1' \
  'export XDG_RUNTIME_DIR=/tmp/xdg' \
  'mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"' \
  'unset WAYLAND_DISPLAY' \
  'export VK_ICD_FILENAMES=$(cat /etc/vk_icd)' \
  'Xvfb :1 -screen 0 1280x800x24 &' \
  'sleep 1' \
  'exec ./bin/Engine' > /usr/local/bin/entrypoint.sh \
  && chmod +x /usr/local/bin/entrypoint.sh

# Use exec-form CMD (better signal handling)
CMD ["/usr/local/bin/entrypoint.sh"]

