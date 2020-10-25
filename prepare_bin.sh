#!/bin/sh

[ ! -d "./modules" ] && mkdir modules

cat <<'EOF' > sdrpp
#!/bin/sh

cd "${0%/*}"
cd build && ./sdrpp
EOF
chmod +x sdrpp

sed -i 's|/.*/Release/|/modules/|' root*/module_list.json


case $(uname) in
	Darwin)
		cp -f build/*/*.dynlib modules
		sed -i 's|\.dll|\.dynlib|' root*/module_list.json
		;;
	*)
		cp -f build/*/*.so modules
		sed -i 's|\.dll|\.so|' root*/module_list.json
esac

[ ! -h build/modules ] && ln -s ../modules build/modules

tar -cZf "sdrpp_$(uname)_$(uname -m)_$(git rev-parse HEAD 2>/dev/null).tar.gz" build/sdrpp build/modules root* modules sdrpp