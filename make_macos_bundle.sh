#!/bin/sh
set -e

# ========================= Boilerplate =========================
BUILD_DIR=$1
BUNDLE=$2

source macos/bundle_utils.sh

# ========================= Prepare dotapp structure =========================

# Clear .app
rm -rf $BUNDLE

# Create .app structure
bundle_create_struct $BUNDLE

# Add resources
cp -R root/res/* $BUNDLE/Contents/Resources/

# Create the icon file
bundle_create_icns root/res/icons/sdrpp.macos.png $BUNDLE/Contents/Resources/sdrpp

# Create the property list
bundle_create_plist sdrpp SDR++ org.sdrpp.sdrpp 1.3.2 sdrp sdrpp sdrpp $BUNDLE/Contents/Info.plist

# ========================= Install binaries =========================

# Core
bundle_install_binary $BUNDLE $BUNDLE/Contents/MacOS $BUILD_DIR/sdrpp 
bundle_install_binary $BUNDLE $BUNDLE/Contents/Frameworks $BUILD_DIR/core/libsdrpp_core.dylib

# Source modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/airspy_source/airspy_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/airspyhf_source/airspyhf_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/bladerf_source/bladerf_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/dragonlabs_source/dragonlabs_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/file_source/file_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/fobossdr_source/fobossdr_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/hackrf_source/hackrf_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/hermes_source/hermes_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/hydrasdr_source/hydrasdr_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/limesdr_source/limesdr_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/network_source/network_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/perseus_source/perseus_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/plutosdr_source/plutosdr_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/rfnm_source/rfnm_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/rfspace_source/rfspace_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/rtl_sdr_source/rtl_sdr_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/rtl_tcp_source/rtl_tcp_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/sdrplay_source/sdrplay_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/sdrpp_server_source/sdrpp_server_source.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/spyserver_source/spyserver_source.dylib || true
# bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/source_modules/usrp_source/usrp_source.dylib || true

# Sink modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/sink_modules/portaudio_sink/audio_sink.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/sink_modules/network_sink/network_sink.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/sink_modules/new_portaudio_sink/new_portaudio_sink.dylib || true

# Decoder modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/decoder_modules/atv_decoder/atv_decoder.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/decoder_modules/m17_decoder/m17_decoder.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/decoder_modules/meteor_demodulator/meteor_demodulator.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/decoder_modules/radio/radio.dylib || true

# Misc modules
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/discord_integration/discord_integration.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/frequency_manager/frequency_manager.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/recorder/recorder.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/rigctl_client/rigctl_client.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/rigctl_server/rigctl_server.dylib || true
bundle_install_binary $BUNDLE $BUNDLE/Contents/Plugins $BUILD_DIR/misc_modules/scanner/scanner.dylib || true

# ========================= Finalize =========================

# Sign the app
bundle_sign $BUNDLE
