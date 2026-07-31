#!/bin/sh

set -eu

bundle=${1:-}
if [ -z "$bundle" ] || [ ! -d "$bundle/Contents" ]; then
	echo "usage: $0 SimpleGraphicSmoke.app" >&2
	exit 2
fi

status=0
while IFS= read -r file_path; do
	if ! file "$file_path" | grep -q "Mach-O"; then
		continue
	fi

	echo "== $file_path"
	file "$file_path"
	archs=$(lipo -archs "$file_path")
	if [ "$archs" != "arm64" ]; then
		echo "unexpected architectures: $archs" >&2
		status=1
	fi

	minos=$(vtool -show-build "$file_path" | awk '/minos/ { print $2; exit }')
	if ! awk -v minos="$minos" 'BEGIN { exit !(minos + 0 <= 13) }'; then
		echo "unexpected deployment target: $minos" >&2
		status=1
	fi

	install_name=$(otool -D "$file_path" 2>/dev/null | awk 'NR == 2 { print $1 }')
	while IFS= read -r dependency; do
		if [ "$dependency" = "$install_name" ]; then
			continue
		fi
		case "$dependency" in
			@rpath/*)
				if [ ! -e "$bundle/Contents/Frameworks/${dependency#@rpath/}" ]; then
					echo "unresolved dependency: $dependency" >&2
					status=1
				fi
				;;
			/System/*|/usr/lib/*) ;;
			*) echo "non-relocatable dependency: $dependency" >&2; status=1 ;;
		esac
	done <<EOF
$(otool -L "$file_path" | awk 'NR > 1 { print $1 }')
EOF
done <<EOF
$(find "$bundle" -type f -print)
EOF

codesign --verify --deep --strict --verbose=2 "$bundle"
exit "$status"
