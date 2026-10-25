KEXT=AudioFixup.kext
DIST=AudioFixup
BUILDDIR=./build
INFO_PLIST=./AudioFixup/Info.plist
VERSION := $(shell /usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$(INFO_PLIST)")

LILU_URL=https://github.com/acidanthera/Lilu.git
SDK_URL=https://github.com/acidanthera/MacKernelSDK.git

.PHONY: all deps clean distribute

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
	rm -rf ./Distribute ./Lilu ./Lilu.kext ./MacKernelSDK

.PHONY: distribute
distribute: all
	rm -rf ./Distribute
	mkdir -p ./Distribute
	ditto -c -k --sequesterRsrc --keepParent --zlibCompressionLevel 9 $(BUILDDIR)/Debug/$(KEXT) ./Distribute/$(DIST)-$(VERSION)-DEBUG.zip
	ditto -c -k --sequesterRsrc --keepParent --zlibCompressionLevel 9 $(BUILDDIR)/Release/$(KEXT) ./Distribute/$(DIST)-$(VERSION)-RELEASE.zip
