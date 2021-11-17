#!/bin/sh
set -e

# Options
BUILD_DIR=$1
DOTAPP=$2

# Remove existing .app
rm -rf $DOTAPP

# Create .app structure
mkdir $DOTAPP
mkdir $DOTAPP/Contents
mkdir $DOTAPP/Contents/MacOS
mkdir $DOTAPP/Contents/Frameworks
mkdir $DOTAPP/Contents/Resources
mkdir $DOTAPP/Contents/Plugins

# ========================= Boilerplate =========================

delete_common_rpath() {
    # TODO: Also remove build/core if present
    sh macos/delete_rpath.sh /usr/local/lib $1
    sh macos/delete_rpath.sh glew $1
    sh macos/delete_rpath.sh fftw $1
    sh macos/delete_rpath.sh glfw $1
}

set_common_rpath() {
    sh macos/set_library_path.sh libGLEW @rpath/libGLEW.dylib $1
    sh macos/set_library_path.sh libfftw3f @rpath/libfftw3f.dylib $1
    sh macos/set_library_path.sh libglfw @rpath/libglfw.dylib $1
    sh macos/set_library_path.sh libvolk @rpath/libvolk.dylib $1
    install_name_tool -add_rpath @loader_path/../Frameworks $1
}

update_common_rpath() {
    delete_common_rpath $1
    set_common_rpath $1
}

install_module() {
    if [ -f $1 ]; then
        BNAME=$(basename $1)
        cp $1 $DOTAPP/Contents/Plugins/
        update_common_rpath $DOTAPP/Contents/Plugins/$BNAME
    fi
}

# ========================= CORE =========================

# Copy files
cp $BUILD_DIR/sdrpp $DOTAPP/Contents/MacOS/
cp $BUILD_DIR/core/libsdrpp_core.dylib $DOTAPP/Contents/Frameworks/

# Get dep paths
LIBGLEW_PATH=$(sh macos/get_library_path.sh libGLEW $DOTAPP/Contents/MacOS/sdrpp)
LIBFFTW3F_PATH=$(sh macos/get_library_path.sh libfftw3f $DOTAPP/Contents/MacOS/sdrpp)
LIBGCC_PATH=$(sh macos/get_library_path.sh libgcc $LIBFFTW3F_PATH)
LIBGLFW_PATH=$(sh macos/get_library_path.sh libglfw $DOTAPP/Contents/MacOS/sdrpp)
LIBVOLK_PATH=$(sh macos/get_library_path.sh libvolk $DOTAPP/Contents/MacOS/sdrpp)

# Copy libfftw3f in advance
cp $LIBFFTW3F_PATH $DOTAPP/Contents/Frameworks/libfftw3f.dylib
cp $LIBGCC_PATH $DOTAPP/Contents/Frameworks/libgcc.dylib

# Udpate old RPATH for sdrpp
update_common_rpath $DOTAPP/Contents/MacOS/sdrpp
sh macos/delete_rpath.sh build/core $DOTAPP/Contents/MacOS/sdrpp

# Udpate old RPATH for libsdrpp_core
update_common_rpath $DOTAPP/Contents/Frameworks/libsdrpp_core.dylib

# Remove libfftw3f signature
codesign --remove-signature $DOTAPP/Contents/Frameworks/libfftw3f.dylib

# Udpate old RPATH for libfftw3f and set new one (do it twice since it has two rpaths for some reason)
sh macos/delete_rpath.sh gcc $DOTAPP/Contents/Frameworks/libfftw3f.dylib
sh macos/delete_rpath.sh gcc $DOTAPP/Contents/Frameworks/libfftw3f.dylib
install_name_tool -add_rpath @loader_path/../Frameworks $DOTAPP/Contents/Frameworks/libfftw3f.dylib
sh macos/set_library_path.sh libgcc @rpath/libgcc.dylib $DOTAPP/Contents/Frameworks/libfftw3f.dylib

# Add back libfftw3f's signature
codesign -s - $DOTAPP/Contents/Frameworks/libfftw3f.dylib

# Copy deps
cp $LIBGLEW_PATH $DOTAPP/Contents/Frameworks/libGLEW.dylib
cp $LIBGLFW_PATH $DOTAPP/Contents/Frameworks/libglfw.dylib
cp $LIBVOLK_PATH $DOTAPP/Contents/Frameworks/libvolk.dylib

# ========================= Resources =========================
cp -R root/res/* $DOTAPP/Contents/Resources/

# ========================= Icon =========================
mkdir $DOTAPP/Contents/Resources/sdrpp.iconset
sips -z 16 16     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_16x16.png
sips -z 32 32     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_16x16@2x.png
sips -z 32 32     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_32x32.png
sips -z 64 64     root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_32x32@2x.png
sips -z 128 128   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_128x128.png
sips -z 256 256   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_128x128@2x.png
sips -z 256 256   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_256x256.png
sips -z 512 512   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_256x256@2x.png
sips -z 512 512   root/res/icons/sdrpp.png --out $DOTAPP/Contents/Resources/sdrpp.iconset/icon_512x512.png
iconutil -c icns $DOTAPP/Contents/Resources/sdrpp.iconset
rm -R $DOTAPP/Contents/Resources/sdrpp.iconset

# ========================= Source Modules =========================

install_module $BUILD_DIR/source_modules/airspy_source/airspy_source.dylib
LIBAIRSPY_PATH=$(sh macos/get_library_path.sh libairspy $BUILD_DIR/source_modules/airspy_source/airspy_source.dylib)
LIBUSB_PATH=$(sh macos/get_library_path.sh libusb $LIBAIRSPY_PATH)
cp $LIBAIRSPY_PATH $DOTAPP/Contents/Frameworks/libairspy.dylib
cp $LIBUSB_PATH $DOTAPP/Contents/Frameworks/libusb.dylib
sh macos/delete_rpath.sh /airspy/ $DOTAPP/Contents/Plugins/airspy_source.dylib
# sh macos/delete_rpath.sh libusb $DOTAPP/Contents/Frameworks/libairspy.dylib # NOT NEEDED????
sh macos/set_library_path.sh libairspy @rpath/libairspy.dylib $DOTAPP/Contents/Plugins/airspy_source.dylib
sh macos/set_library_path.sh libusb @rpath/libusb.dylib $DOTAPP/Contents/Frameworks/libairspy.dylib
install_name_tool -add_rpath @loader_path/../Frameworks $DOTAPP/Contents/Frameworks/libairspy.dylib
codesign --remove-signature $DOTAPP/Contents/Frameworks/libairspy.dylib
codesign -s - $DOTAPP/Contents/Frameworks/libairspy.dylib

install_module $BUILD_DIR/source_modules/airspyhf_source/airspyhf_source.dylib
LIBAIRSPYHF_PATH=$(sh macos/get_library_path.sh libairspyhf $BUILD_DIR/source_modules/airspyhf_source/airspyhf_source.dylib)
cp $LIBAIRSPYHF_PATH $DOTAPP/Contents/Frameworks/libairspyhf.dylib
sh macos/delete_rpath.sh /airspyhf/ $DOTAPP/Contents/Plugins/airspyhf_source.dylib
# sh macos/delete_rpath.sh libusb $DOTAPP/Contents/Frameworks/libairspyhf.dylib # NOT NEEDED????
sh macos/set_library_path.sh libairspy @rpath/libairspyhf.dylib $DOTAPP/Contents/Plugins/airspyhf_source.dylib
sh macos/set_library_path.sh libusb @rpath/libusb.dylib $DOTAPP/Contents/Frameworks/libairspyhf.dylib
install_name_tool -add_rpath @loader_path/../Frameworks $DOTAPP/Contents/Frameworks/libairspyhf.dylib
codesign --remove-signature $DOTAPP/Contents/Frameworks/libairspyhf.dylib
codesign -s - $DOTAPP/Contents/Frameworks/libairspyhf.dylib

install_module $BUILD_DIR/source_modules/bladerf_source/bladerf_source.dylib
LIBBLADERF_PATH=$(sh macos/get_library_path.sh libbladeRF $BUILD_DIR/source_modules/bladerf_source/bladerf_source.dylib)
cp $LIBBLADERF_PATH $DOTAPP/Contents/Frameworks/libbladeRF.dylib
sh macos/delete_rpath.sh libbladerf $DOTAPP/Contents/Plugins/bladerf_source.dylib
# sh macos/delete_rpath.sh libusb $DOTAPP/Contents/Frameworks/libbladeRF.dylib # NOT NEEDED????
sh macos/set_library_path.sh libbladeRF @rpath/libbladeRF.dylib $DOTAPP/Contents/Plugins/bladerf_source.dylib
sh macos/set_library_path.sh libusb @rpath/libusb.dylib $DOTAPP/Contents/Frameworks/libbladeRF.dylib
install_name_tool -add_rpath @loader_path/../Frameworks $DOTAPP/Contents/Frameworks/libbladeRF.dylib
codesign --remove-signature $DOTAPP/Contents/Frameworks/libbladeRF.dylib
codesign -s - $DOTAPP/Contents/Frameworks/libbladeRF.dylib

install_module $BUILD_DIR/source_modules/file_source/file_source.dylib

install_module $BUILD_DIR/source_modules/hackrf_source/hackrf_source.dylib
LIBHACKRF_PATH=$(sh macos/get_library_path.sh libhackrf $BUILD_DIR/source_modules/hackrf_source/hackrf_source.dylib)
cp $LIBHACKRF_PATH $DOTAPP/Contents/Frameworks/libhackrf.dylib
sh macos/delete_rpath.sh /hackrf/ $DOTAPP/Contents/Plugins/hackrf_source.dylib
# sh macos/delete_rpath.sh libusb $DOTAPP/Contents/Frameworks/libhackrf.dylib # NOT NEEDED????
sh macos/set_library_path.sh libhackrf @rpath/libhackrf.dylib $DOTAPP/Contents/Plugins/hackrf_source.dylib
sh macos/set_library_path.sh libusb @rpath/libusb.dylib $DOTAPP/Contents/Frameworks/libhackrf.dylib
install_name_tool -add_rpath @loader_path/../Frameworks $DOTAPP/Contents/Frameworks/libhackrf.dylib
codesign --remove-signature $DOTAPP/Contents/Frameworks/libhackrf.dylib
codesign -s - $DOTAPP/Contents/Frameworks/libhackrf.dylib

install_module $BUILD_DIR/source_modules/rtl_sdr_source/rtl_sdr_source.dylib
LIBRTLSDR_PATH=$(sh macos/get_library_path.sh librtlsdr $BUILD_DIR/source_modules/rtl_sdr_source/rtl_sdr_source.dylib)
cp $LIBRTLSDR_PATH $DOTAPP/Contents/Frameworks/librtlsdr.dylib
sh macos/delete_rpath.sh librtlsdr $DOTAPP/Contents/Plugins/rtl_sdr_source.dylib
# sh macos/delete_rpath.sh libusb $DOTAPP/Contents/Frameworks/librtlsdr.dylib # NOT NEEDED????
sh macos/set_library_path.sh librtlsdr @rpath/librtlsdr.dylib $DOTAPP/Contents/Plugins/rtl_sdr_source.dylib
sh macos/set_library_path.sh libusb @rpath/libusb.dylib $DOTAPP/Contents/Plugins/rtl_sdr_source.dylib # On intel needed too apparently
sh macos/set_library_path.sh libusb @rpath/libusb.dylib $DOTAPP/Contents/Frameworks/librtlsdr.dylib
install_name_tool -add_rpath @loader_path/../Frameworks $DOTAPP/Contents/Frameworks/librtlsdr.dylib
codesign --remove-signature $DOTAPP/Contents/Frameworks/librtlsdr.dylib
codesign -s - $DOTAPP/Contents/Frameworks/librtlsdr.dylib

install_module $BUILD_DIR/source_modules/rtl_tcp_source/rtl_tcp_source.dylib

install_module $BUILD_DIR/source_modules/sdrplay_source/sdrplay_source.dylib

install_module $BUILD_DIR/source_modules/spyserver_source/spyserver_source.dylib

# ========================= Sink Modules =========================

install_module $BUILD_DIR/sink_modules/network_sink/network_sink.dylib

install_module $BUILD_DIR/sink_modules/portaudio_sink/audio_sink.dylib
LIBPORTAUDIO_PATH=$(sh macos/get_library_path.sh libportaudio $BUILD_DIR/sink_modules/portaudio_sink/audio_sink.dylib)
cp $LIBPORTAUDIO_PATH $DOTAPP/Contents/Frameworks/libportaudio.dylib
sh macos/delete_rpath.sh /portaudio/ $DOTAPP/Contents/Plugins/audio_sink.dylib
sh macos/set_library_path.sh libportaudio @rpath/libportaudio.dylib $DOTAPP/Contents/Plugins/audio_sink.dylib

install_module $BUILD_DIR/sink_modules/new_portaudio_sink/new_portaudio_sink.dylib
sh macos/delete_rpath.sh /portaudio/ $DOTAPP/Contents/Plugins/new_portaudio_sink.dylib
sh macos/set_library_path.sh libportaudio @rpath/libportaudio.dylib $DOTAPP/Contents/Plugins/new_portaudio_sink.dylib

# ========================= Decoder Modules =========================

install_module $BUILD_DIR/decoder_modules/m17_decoder/m17_decoder.dylib
# TODO: Add codec2

install_module $BUILD_DIR/decoder_modules/meteor_demodulator/meteor_demodulator.dylib

install_module $BUILD_DIR/decoder_modules/radio/radio.dylib

# ========================= Misc Modules =========================

install_module $BUILD_DIR/misc_modules/discord_integration/discord_integration.dylib

install_module $BUILD_DIR/misc_modules/frequency_manager/frequency_manager.dylib

install_module $BUILD_DIR/misc_modules/recorder/recorder.dylib

install_module $BUILD_DIR/misc_modules/rigctl_server/rigctl_server.dylib

# ========================= Create PList =========================

echo '<?xml version="1.0" encoding="UTF-8"?>' >> $DOTAPP/Contents/Info.plist
echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $DOTAPP/Contents/Info.plist
echo '' >> $DOTAPP/Contents/Info.plist
echo '<plist version="1.0">' >> $DOTAPP/Contents/Info.plist
echo '    <dict>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleName</key><string>sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleDisplayName</key><string>SDR++</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleIdentifier</key><string>org.sdrpp.sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleVersion</key><string>1.0.5</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundlePackageType</key><string>APPL</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleSignature</key><string>sdrp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleExecutable</key><string>sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '        <key>CFBundleIconFile</key><string>sdrpp</string>' >> $DOTAPP/Contents/Info.plist
echo '    </dict>' >> $DOTAPP/Contents/Info.plist
echo '</plist>' >> $DOTAPP/Contents/Info.plist