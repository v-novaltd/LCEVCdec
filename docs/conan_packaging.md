# Conan Packaging

## Exporting package recipe and sources to local Conan repository

	cd lcevc_dec
 	conan export . user/channek

## Installing a default native binary build of package in local Conan repository

 	conan install lcevc_dec/1.0.0@user/channel --build=missing

## Installing a binary build local Conan repository using a given profile

 	conan install lcevc_dec/1.0.0@user/channel --build=missing --profile=android-armeabi-v7a-api-21-Release

## Uploading from local repository to a remote Conan repository

	conan upload -r vnova-conan lcevc_dec/1.0.0@user/channel
