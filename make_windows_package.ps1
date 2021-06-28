$build_dir=$args[0]
$root_dir=$args[1]

mkdir sdrpp_windows_x64

# Copy root
cp -Recurse $root_dir/* sdrpp_windows_x64/

# Copy core
cp $build_dir/Release/* sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/volk.dll' sdrpp_windows_x64/

# Copy source modules
cp $build_dir/airspy_source/Release/airspy_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/airspy.dll' sdrpp_windows_x64/

cp $build_dir/airspyhf_source/Release/airspyhf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/airspyhf.dll' sdrpp_windows_x64/

cp $build_dir/bladerf_source/Release/bladerf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/bladeRF.dll' sdrpp_windows_x64/

cp $build_dir/file_source/Release/file_source.dll sdrpp_windows_x64/modules/

cp $build_dir/hackrf_source/Release/hackrf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/hackrf.dll' sdrpp_windows_x64/

cp $build_dir/rtl_sdr_source/Release/rtl_sdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/rtlsdr.dll' sdrpp_windows_x64/

cp $build_dir/limesdr_source/Release/limesdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/LimeSuite.dll' sdrpp_windows_x64/

cp $build_dir/rtl_tcp_source/Release/rtl_tcp_source.dll sdrpp_windows_x64/modules/

cp $build_dir/sdrplay_source/Release/sdrplay_source.dll sdrpp_windows_x64/modules/ -ErrorAction SilentlyContinue
cp 'C:/Program Files/SDRplay/API/x64/sdrplay_api.dll' sdrpp_windows_x64/ -ErrorAction SilentlyContinue

cp $build_dir/soapy_source/Release/soapy_source.dll sdrpp_windows_x64/modules/

cp $build_dir/plutosdr_source/Release/plutosdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/libiio.dll' sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/libad9361.dll' sdrpp_windows_x64/


# Copy sink modules
cp $build_dir/audio_sink/Release/audio_sink.dll sdrpp_windows_x64/modules/
cp "C:/Program Files (x86)/RtAudio/bin/rtaudio.dll" sdrpp_windows_x64/


# Copy decoder modules
cp $build_dir/meteor_demodulator/Release/meteor_demodulator.dll sdrpp_windows_x64/modules/
cp $build_dir/radio/Release/radio.dll sdrpp_windows_x64/modules/


# Copy misc modules
cp $build_dir/discord_integration/Release/discord_integration.dll sdrpp_windows_x64/modules/
cp $build_dir/recorder/Release/recorder.dll sdrpp_windows_x64/modules/


# Copy supporting libs
cp 'C:/Program Files/PothosSDR/bin/libusb-1.0.dll' sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/pthreadVC2.dll' sdrpp_windows_x64/

Compress-Archive -Path sdrpp_windows_x64/ -DestinationPath sdrpp_windows_x64.zip

rm -Force -Recurse sdrpp_windows_x64