{
  description = "OpenGL programming environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            xorg.libX11
            xorg.libXrandr
            xorg.libXinerama
            xorg.libXcursor
            xorg.libXi
            xorg.libXext
            libGL
            glew
            glfw
            bear

            glxinfo
            python313Packages.glad

            gnumake
            gdb
            pkg-config

            assimp
            zlib
          ];

          shellHook = ''
            export LD_LIBRARY_PATH="/run/opengl-driver/lib:/run/opengl-driver-32/lib:$LD_LIBRARY_PATH"
            export NIX_CFLAGS_COMPILE="-isystem $PWD/include $NIX_CFLAGS_COMPILE"
          '';
        };
      }
    );
}
