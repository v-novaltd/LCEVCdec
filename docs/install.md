# Install

The output artefacts of a build can be collected into an install directory tree via the CMake
`install()` rules, for example:

	cd _build
	cmake --build .
	cmake --install . --prefix=_install

The install directory is given as `_install` in this example, but the name and location can be
chosen as appropriate.

# Packaging

To create a packaged install, CPack can be used - for example, to create a ZIP file of an
installation of the current build:

	cd _build
	cmake --build .
	cpack -G ZIP .
