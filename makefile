KEXT=AudioFixup.kext
DIST=AudioFixup
BUILDDIR=./build
INFO_PLIST=./AudioFixup/Info.plist
VERSION := $(shell /usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$(INFO_PLIST)")

LILU_URL=https://github.com/acidanthera/Lilu.git
SDK_URL=https://github.com/acidanthera/MacKernelSDK.git

.PHONY: all deps clean dist

all: deps
	xcodebuild build $(OPTIONS) -configuration Debug
	xcodebuild build $(OPTIONS) -configuration Release

.PHONY: deps
deps:
	rm -rf ./Lilu
	git clone $(LILU_URL) Lilu
	git clone $(SDK_URL) Lilu/MacKernelSDK
	cd Lilu && xcodebuild -configuration Debug -arch x86_64
	rm -rf ./Lilu.kext ./MacKernelSDK
	cp -R ./Lilu/build/Debug/Lilu.kext ./Lilu.kext
	cp -R ./Lilu/MacKernelSDK ./MacKernelSDK
	rm -rf ./Lilu

.PHONY: clean
clean:
	xcodebuild clean $(OPTIONS) -configuration Debug
	xcodebuild clean $(OPTIONS) -configuration Release
	rm -rf ./dist ./Lilu ./Lilu.kext ./MacKernelSDK

.PHONY: dist
dist: all
	rm -rf ./dist
	mkdir -p ./dist
	ditto -c -k --sequesterRsrc --keepParent --zlibCompressionLevel 9 $(BUILDDIR)/Debug/$(KEXT) ./dist/$(DIST)-$(VERSION)-DEBUG.zip
	ditto -c -k --sequesterRsrc --keepParent --zlibCompressionLevel 9 $(BUILDDIR)/Release/$(KEXT) ./dist/$(DIST)-$(VERSION)-RELEASE.zip
