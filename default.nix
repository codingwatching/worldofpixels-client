{
	# since the emscripten package needs duct tape to work, fix the nixpkgs commit
	# to make the build reliable in case something changes. 02/05/2025 release 24.11
	# emscripten version 3.1.64
	nixpkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/bf3287dac860542719fe7554e21e686108716879.tar.gz") { }
}:
with nixpkgs; let
	emZlibUrl = "https://github.com/madler/zlib/archive/refs/tags/v1.2.13.tar.gz";
	emLibpngUrl = "https://storage.googleapis.com/webassembly/emscripten-ports/libpng-1.6.39.tar.gz";
	emZlib = fetchurl {
		url = emZlibUrl;
		hash = "sha256-FSWVKgpWdYF5JhOpcjMz1/jMILh6gfkg+4vH4/IlFCg=";
	};
	emLibpng = fetchurl {
		url = emLibpngUrl;
		hash = "sha256-r0+38mD4OZGeWVjlqwGiddT+Q21FRCo27mL3Plvrdbo=";
	};
	projectSrc = fetchGit {
		url = ./.;
	};
in buildEmscriptenPackage {
	name = "owop-client";

	enableParallelBuilding = true;

	src = projectSrc;

	buildInputs = [
		glm
	];

	nativeBuildInputs = [
		gnutar gzip
		(writeShellScriptBin "emdwp" ''
export NODE_PATH='${emscripten.nodeModules}'
export EM_EXCLUSIVE_CACHE_ACCESS='1'
export PYTHON='${python3}/bin/python'
exec "${emscripten}/share/emscripten/emdwp"  "$@"
		'')
	];

	preConfigure = ''
		echo "#!/bin/sh" > ./configure
		chmod +x ./configure
		echo -e 'VERSION=${projectSrc.dirtyShortRev or projectSrc.shortRev}\nDISABLE_AUTO_REFRESH=1\nMAKEFLAGS+=-j'"$(nproc)" \
			| cat - Makefile > temp && mv temp Makefile
	'';

	postConfigure = ''
		mkdir -p "$EM_CACHE/ports/"{zlib,libpng}
		cp ${emZlib} "$EM_CACHE/ports/zlib.2.13.tar.gz"
		cp ${emLibpng} "$EM_CACHE/ports/libpng.6.39.tar.gz"
		tar -C "$EM_CACHE/ports/zlib" -xaf "$EM_CACHE/ports/zlib.2.13.tar.gz"
		tar -C "$EM_CACHE/ports/libpng" -xaf "$EM_CACHE/ports/libpng.6.39.tar.gz"
		echo "${emZlibUrl}" > "$EM_CACHE/ports/zlib/.emscripten_url"
		echo "${emLibpngUrl}" > "$EM_CACHE/ports/libpng/.emscripten_url"

		ln -sf ${glm}/include/glm lib/glm
	'';

	installPhase = ''
		mkdir -p $out/share/www/
		cp -r out $out/share/www/owop
	'';

	checkPhase = "";

	meta = {
		description = "The Our World of Pixels client.";
		homepage = "https://ourworldofpixels.com";
		platforms = lib.platforms.linux;
	};
}
