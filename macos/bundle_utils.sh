#==============================================#
#           MacOS Bundle Utilities             #
#         Copyright(C) Ryzerth, 2021           #
#==============================================#

# ========================= Customization =========================

# bundle_is_not_to_be_installed [dylib_name]
bundle_is_not_to_be_installed() {
    # NOTE: Customize this list to exclude libraries you don't want copied into the bundle
    if [ "$1" = "libsdrpp_core.dylib" ]; then echo 1; fi
    if [ "$1" = "OpenGL" ]; then echo 1; fi
    if [ "$1" = "libc++.1.dylib" ]; then echo 1; fi
    if [ "$1" = "libSystem.B.dylib" ]; then echo 1; fi
    if [ "$1" = "Cocoa" ]; then echo 1; fi
    if [ "$1" = "IOKit" ]; then echo 1; fi
    if [ "$1" = "CoreFoundation" ]; then echo 1; fi
    if [ "$1" = "AppKit" ]; then echo 1; fi
    if [ "$1" = "CoreGraphics" ]; then echo 1; fi
    if [ "$1" = "CoreServices" ]; then echo 1; fi
    if [ "$1" = "Foundation" ]; then echo 1; fi
    if [ "$1" = "CoreAudio" ]; then echo 1; fi
    if [ "$1" = "AudioToolbox" ]; then echo 1; fi
    if [ "$1" = "AudioUnit" ]; then echo 1; fi
    if [ "$1" = "libobjc.A.dylib" ]; then echo 1; fi
}

# ========================= FOR INTERNAL USE ONLY =========================

# bundle_get_first_element [elem0] [elem1] [elem2] ...
bundle_get_first_element() {
    echo $1
}

# bundle_get_second_element [elem0] [elem1] [elem2] ...
bundle_get_second_element() {
    echo $2
}

# bundle_get_exec_deps [exec_path]
bundle_get_exec_deps() {
    DEPS_RAW=$(otool -L $1 | tail -n +2)

    # Iterate over all lines
    echo "$DEPS_RAW" | while read -r DEP; do
        echo $(bundle_get_first_element $DEP)
    done
}

# bundle_get_exec_rpaths [exec_path]
bundle_get_exec_rpaths() {
    RPATHS_RAW=$(otool -l $1 | grep "path\ ")

    # Iterate over all lines
    echo "$RPATHS_RAW" | while read -r RPATH; do
        echo $(bundle_get_second_element $RPATH)
    done
}

# bundle_find_full_path [dep_path] [exec_rpaths]
bundle_find_full_path() {
    # If path is relative to rpath, find the full path
    local IS_RPATH_RELATIVE=$(echo $1 | grep @rpath/)
    if [ "$IS_RPATH_RELATIVE" = "" ]; then
        echo $1
        return
    fi

    local RPATH_NEXT=$(echo $1 | cut -c 7-)

    # Search in the exec's RPATH
    echo "$2" | while read -r RPATH; do
        # If not found, skip
        if [ ! -f $RPATH/$RPATH_NEXT ]; then
            continue
        fi

        # Correct dep path
        echo $RPATH/$RPATH_NEXT
        return
    done

    # Search other common paths
    if [ -f /usr/local/lib/$RPATH_NEXT ]; then
        echo /usr/local/lib/$RPATH_NEXT 
        return
    fi
    if [ -f /Library/Frameworks/$RPATH_NEXT ]; then
        echo /Library/Frameworks/$RPATH_NEXT 
        return
    fi

    # Not found, give up
    echo $1
}

# ========================= Public Functions =========================

bundle_create_struct() {
    if [ $# -ne 1 ]; then
        echo "bundle_create_struct [bundle_path]";
        return
    fi
    
    mkdir -p $1
    mkdir -p $1/Contents
    mkdir -p $1/Contents/MacOS
    mkdir -p $1/Contents/Frameworks
    mkdir -p $1/Contents/Resources
    mkdir -p $1/Contents/Plugins
}

bundle_install_binary() {
    if [ $# -ne 3 ]; then
        echo "bundle_install_binary [bundle_path] [install_path] [exec_path]";
        return
    fi

    local EXEC_NAME=$(basename $3)
    local EXEC_DEST=$2/$EXEC_NAME

    # Check if file exists
    if [ ! -f $3 ]; then
        echo "==NOT== Installing" $3
        return
    fi

    # Get RPATHs
    local RPATHS=$(bundle_get_exec_rpaths $3)

    echo "Installing" $3

    # Copy it to its install location
    cp $3 $EXEC_DEST
    
    # Install dependencies and change path
    local DEPS=$(bundle_get_exec_deps $EXEC_DEST)
    echo "$DEPS" | while read -r DEP; do
        local DEP_NAME=$(basename $DEP)

        # Skip if this dep is blacklisted
        local NOT_TO_BE_INSTALLED=$(bundle_is_not_to_be_installed $DEP_NAME)
        if [ "$NOT_TO_BE_INSTALLED" = "1" ]; then
            continue
        fi

        # Skip if this dep is itself
        if [ "$DEP_NAME" = "$EXEC_NAME" ]; then
            continue
        fi

        DEP=$(bundle_find_full_path $DEP $RPATHS)

        # If the dependency is not installed, install it
        if [ ! -f $1/Contents/Frameworks/$DEP_NAME ]; then
            bundle_install_binary $1 $1/Contents/Frameworks $DEP
        fi

        # Fix path
        install_name_tool -change $DEP @rpath/$DEP_NAME $EXEC_DEST
    done

    # Remove all its rpaths
    if [ "$RPATHS" != "" ]; then
        echo "$RPATHS" | while read -r RPATH; do
            install_name_tool -delete_rpath $RPATH $EXEC_DEST
        done
    fi
    
    # Add new single rpath
    install_name_tool -add_rpath @loader_path/../Frameworks $EXEC_DEST
}

bundle_create_icns() {
    if [ $# -ne 2 ]; then
        echo "bundle_create_icns [image_path] [icns_file]";
        return
    fi

    mkdir $2.iconset
    sips -z 16 16     $1 --out $2.iconset/icon_16x16.png
    sips -z 32 32     $1 --out $2.iconset/icon_16x16@2x.png
    sips -z 32 32     $1 --out $2.iconset/icon_32x32.png
    sips -z 64 64     $1 --out $2.iconset/icon_32x32@2x.png
    sips -z 128 128   $1 --out $2.iconset/icon_128x128.png
    sips -z 256 256   $1 --out $2.iconset/icon_128x128@2x.png
    sips -z 256 256   $1 --out $2.iconset/icon_256x256.png
    sips -z 512 512   $1 --out $2.iconset/icon_256x256@2x.png
    sips -z 512 512   $1 --out $2.iconset/icon_512x512.png
    iconutil -c icns $2.iconset
    rm -R $2.iconset
}

bundle_create_plist() {
    if [ $# -ne 8 ]; then
        echo "bundle_create_plist [bundle_name] [display_name] [identifier] [version] [signature] [exec] [icon] [plist]";
        return
    fi
    
    echo '<?xml version="1.0" encoding="UTF-8"?>' >> $8
    echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $8
    echo '' >> $8
    echo '<plist version="1.0">' >> $8
    echo '    <dict>' >> $8
    echo '        <key>CFBundleName</key><string>'$1'</string>' >> $8
    echo '        <key>CFBundleDisplayName</key><string>'$2'</string>' >> $8
    echo '        <key>CFBundleIdentifier</key><string>o'$3'</string>' >> $8
    echo '        <key>CFBundleVersion</key><string>'$4'</string>' >> $8
    echo '        <key>CFBundlePackageType</key><string>APPL</string>' >> $8
    echo '        <key>CFBundleSignature</key><string>'$5'</string>' >> $8
    echo '        <key>CFBundleExecutable</key><string>'$6'</string>' >> $8
    echo '        <key>CFBundleIconFile</key><string>'$7'</string>' >> $8
    echo '    </dict>' >> $8
    echo '</plist>' >> $8
}

bundle_sign() {
    if [ $# -ne 1 ]; then
        echo "bundle_sign [bundle_dir]";
        return
    fi

    codesign --force --deep -s - $1
}