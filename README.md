<h3 align="center">Quadruped UI</h3>

<div align="center">

[![Status](https://img.shields.io/badge/status-active-success.svg)]()

</div>

---

<p align="center"> User interface for quadruped control.
    <br> 
</p>

## 📝 Table of Contents

- [About](#about)
- [Getting Started](#getting_started)

## 🧐 About <a name = "about"></a>

Custom Renderer built using the Vulkan API.

## 🏁 Getting Started <a name = "getting_started"></a>

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

##### GPU Hardware (one of the following):

- NVIDIA: GeForce 900 series (Maxwell 2nd gen) or newer
- AMD: Radeon RX 500 series (GCN 4th gen) or newer
- Intel: 7th Gen Core (Kaby Lake) integrated graphics or newer

##### Driver Requirements:

- Latest graphics drivers from manufacturer
- Vulkan 1.3 compatible driver version

##### Operating System:

- Windows 11
- Modern Linux distributions

### Installing

##### Build using vcpkg

1. Clone the project
2. Clone vcpkg somewhere on your system
3. Follow instructions from https://learn.microsoft.com/en-us/vcpkg/get_started/overview to setup vcpkg for your work environment.
4. The instructions from https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell step 6 onwards were used to test the build in vscode on Windows 11.


*** This will only build the UI. For a full development environment setup, see building using Nix.

##### Build using Nix

1. Install nix operating system or package manager at https://nixos.org/
2. Run nix develop . in quadruped/gui. (Where the flake.nix  file is)
3. Build the project using cmake or use the launch.sh script.


*** Note that the flake contains the dependencies for ROS and quadruped development.

## ✍️ Authors <a name = "authors"></a>

- [@Yu Qing Liu](https://github.com/Yu-Qing-Liu)

## 🎉 Acknowledgements <a name = "acknowledgement"></a>

- See vcpkg.json for dependencies used.