{
  description = "My flake";

  inputs = {
    nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.1";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        nativeBuildInputs = with pkgs; [
          cmake
          meson
          ninja
          pkg-config
          glslang
          shaderc
        ];
        buildInputs = with pkgs; [
          vulkan-loader
          vulkan-memory-allocator
          vulkan-validation-layers
          vulkan-utility-libraries
          vk-bootstrap
          openxr-loader
          wayland
          zlib
          sdl3
        ];
      in
      {
        devShells.default =
          pkgs.mkShell.override { stdenv = pkgs.stdenvAdapters.useMoldLinker pkgs.llvmPackages_21.stdenv; }
            {
              packages =
                with pkgs;
                [
                  llvmPackages_21.clang-tools
                  lldb
                  codespell
                  doxygen
                  gtest
                  cppcheck
                ]
                ++ buildInputs
                ++ nativeBuildInputs
                ++ pkgs.lib.optionals pkgs.stdenv.isLinux (
                  with pkgs;
                  [
                    valgrind-light
                  ]
                );
              shellHook = ''
                export VK_BOOTSTRAP_LIB=${pkgs.vk-bootstrap}
                export VK_BOOTSTRAP_DEV=${pkgs.vk-bootstrap.dev}

                cat > .clangd << 'EOF'
                CompileFlags:
                  Add: [
                    "-isystem", "${pkgs.llvmPackages_21.libcxx.dev}/include/c++/v1",
                    "-isystem", "${pkgs.glibc.dev}/include",
                    "-I${pkgs.vulkan-utility-libraries}/include"
                  ]
                EOF
              '';
            };
      }
    );
}
