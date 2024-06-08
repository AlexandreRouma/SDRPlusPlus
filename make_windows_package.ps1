$build_dir=$args[0]
$root_dir=$args[1]

mkdir sdrpp_windows_x64

# Copy root
cp -Recurse $root_dir/* sdrpp_windows_x64/

# Copy core
cp $build_dir/Debug/* sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/volk.dll' sdrpp_windows_x64/

# Copy source modules
cp $build_dir/source_modules/airspy_source/Debug/airspy_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/airspy.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/airspyhf_source/Debug/airspyhf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/airspyhf.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/audio_source/Debug/audio_source.dll sdrpp_windows_x64/modules/

cp $build_dir/source_modules/bladerf_source/Debug/bladerf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/bladeRF.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/file_source/Debug/file_source.dll sdrpp_windows_x64/modules/

cp $build_dir/source_modules/hackrf_source/Debug/hackrf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/hackrf.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/hermes_source/Debug/hermes_source.dll sdrpp_windows_x64/modules/

cp $build_dir/source_modules/limesdr_source/Debug/limesdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/LimeSuite.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/perseus_source/Debug/perseus_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/perseus-sdr.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/plutosdr_source/Debug/plutosdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/libiio.dll' sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/libad9361.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/rfspace_source/Debug/rfspace_source.dll sdrpp_windows_x64/modules/

cp $build_dir/source_modules/rtl_sdr_source/Debug/rtl_sdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/rtlsdr.dll' sdrpp_windows_x64/

cp $build_dir/source_modules/rtl_tcp_source/Debug/rtl_tcp_source.dll sdrpp_windows_x64/modules/

cp $build_dir/source_modules/sdrplay_source/Debug/sdrplay_source.dll sdrpp_windows_x64/modules/ -ErrorAction SilentlyContinue
cp 'C:/Program Files/SDRplay/API/x64/sdrplay_api.dll' sdrpp_windows_x64/ -ErrorAction SilentlyContinue

cp $build_dir/source_modules/sdrpp_server_source/Debug/sdrpp_server_source.dll sdrpp_windows_x64/modules/

cp $build_dir/source_modules/spyserver_source/Debug/spyserver_source.dll sdrpp_windows_x64/modules/

# cp $build_dir/source_modules/usrp_source/Debug/usrp_source.dll sdrpp_windows_x64/modules/


# Copy sink modules
cp $build_dir/sink_modules/audio_sink/Debug/audio_sink.dll sdrpp_windows_x64/modules/
cp "C:/Program Files (x86)/RtAudio/bin/rtaudio.dll" sdrpp_windows_x64/

cp $build_dir/sink_modules/network_sink/Debug/network_sink.dll sdrpp_windows_x64/modules/


# Copy decoder modules
cp $build_dir/decoder_modules/m17_decoder/Debug/m17_decoder.dll sdrpp_windows_x64/modules/
cp "C:/Program Files/codec2/lib/libcodec2.dll" sdrpp_windows_x64/

cp $build_dir/decoder_modules/meteor_demodulator/Debug/meteor_demodulator.dll sdrpp_windows_x64/modules/

cp $build_dir/decoder_modules/radio/Debug/radio.dll sdrpp_windows_x64/modules/


# Copy misc modules
cp $build_dir/misc_modules/discord_integration/Debug/discord_integration.dll sdrpp_windows_x64/modules/

cp $build_dir/misc_modules/frequency_manager/Debug/frequency_manager.dll sdrpp_windows_x64/modules/

cp $build_dir/misc_modules/iq_exporter/Debug/iq_exporter.dll sdrpp_windows_x64/modules/

cp $build_dir/misc_modules/recorder/Debug/recorder.dll sdrpp_windows_x64/modules/

cp $build_dir/misc_modules/rigctl_client/Debug/rigctl_client.dll sdrpp_windows_x64/modules/

cp $build_dir/misc_modules/rigctl_server/Debug/rigctl_server.dll sdrpp_windows_x64/modules/

cp $build_dir/misc_modules/scanner/Debug/scanner.dll sdrpp_windows_x64/modules/


# Copy supporting libs
cp 'C:/Program Files/PothosSDR/bin/libusb-1.0.dll' sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/pthreadVC2.dll' sdrpp_windows_x64/

Compress-Archive -Path sdrpp_windows_x64/ -DestinationPath sdrpp_windows_x64.zip

rm -Force -Recurse sdrpp_windows_x64
