#!/bin/sh

echo 'WARNING: THIS SCRIPT IS IN BETA, USE AT YOUR OWN RISK!!! COMPLETE SYSTEM DESTRUCTION IS POSSIBLE!!!'

mkdir sdrpp_macos_amd64
mkdir sdrpp_macos_amd64/fs

# Create install script
echo '#!/bin/sh'                                            >> sdrpp_macos_amd64/install.sh
echo 'if [ "$(id -u)" != "0" ]; then'                       >> sdrpp_macos_amd64/install.sh
echo '    echo "Please run this install with sudo" 1>&2'    >> sdrpp_macos_amd64/install.sh
echo '    exit 1'                                           >> sdrpp_macos_amd64/install.sh
echo 'fi'                                                   >> sdrpp_macos_amd64/install.sh
echo 'echo Installing...'                                   >> sdrpp_macos_amd64/install.sh
echo 'cp -vrf sdrpp_macos_amd64/fs/* /'                     >> sdrpp_macos_amd64/install.sh
echo 'echo Done.'                                           >> sdrpp_macos_amd64/install.sh

# Create uninstall script
echo '#!/bin/sh'                                            >> sdrpp_macos_amd64/uninstall.sh
echo 'if [ "$(id -u)" != "0" ]; then'                       >> sdrpp_macos_amd64/uninstall.sh
echo '    echo "Please run this install with sudo" 1>&2'    >> sdrpp_macos_amd64/uninstall.sh
echo '    exit 1'                                           >> sdrpp_macos_amd64/uninstall.sh
echo 'fi'                                                   >> sdrpp_macos_amd64/uninstall.sh
echo 'echo Uninstalling...'                                 >> sdrpp_macos_amd64/uninstall.sh
echo 'rm -f /usr/local/lib/libsdrpp_core.dylib'             >> sdrpp_macos_amd64/uninstall.sh
echo 'rm -rf /usr/local/lib/sdrpp'                          >> sdrpp_macos_amd64/uninstall.sh
echo 'rm -rf /usr/local/share/sdrpp'                        >> sdrpp_macos_amd64/uninstall.sh
echo 'echo Done.'                                           >> sdrpp_macos_amd64/uninstall.sh

# Copy files
ORIG_DIR=$PWD
cd $1
make install DESTDIR=$ORIG_DIR/sdrpp_macos_amd64/fs
cd $ORIG_DIR

# Create archive
tar -czvf sdrpp_macos_amd64.tar.gz sdrpp_macos_amd64/

# Cleanup
rm -rf sdrpp_macos_amd64/