mkdir sdrpp_windows_x64

# Copy root
cp -Recurse root/* sdrpp_windows_x64/

# Copy core
cp build/Release/* sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/volk.dll' sdrpp_windows_x64/

# Copy modules
cp build/radio/Release/radio.dll sdrpp_windows_x64/modules/

cp build/recorder/Release/recorder.dll sdrpp_windows_x64/modules/

cp build/airspyhf_source/Release/airspyhf_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/airspyhf.dll' sdrpp_windows_x64/

cp build/airspy_source/Release/airspy_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/airspy.dll' sdrpp_windows_x64/

cp build/rtl_sdr_source/Release/rtl_sdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/rtlsdr.dll' sdrpp_windows_x64/

cp build/plutosdr_source/Release/plutosdr_source.dll sdrpp_windows_x64/modules/
cp 'C:/Program Files/PothosSDR/bin/libiio.dll' sdrpp_windows_x64/
cp 'C:/Program Files/PothosSDR/bin/libad9361.dll' sdrpp_windows_x64/

cp build/rtl_tcp_source/Release/rtl_tcp_source.dll sdrpp_windows_x64/modules/

cp build/soapy_source/Release/soapy_source.dll sdrpp_windows_x64/modules/

cp build/audio_sink/Release/audio_sink.dll sdrpp_windows_x64/modules/
cp build/audio_sink/Release/portaudio.dll sdrpp_windows_x64/

Compress-Archive -Path sdrpp_windows_x64/ -DestinationPath sdrpp_windows_x64.zip

rm -Force -Recurse sdrpp_windows_x64