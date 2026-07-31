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
	case "$minos" in
		13|13.*) ;;
		*) echo "unexpected deployment target: $minos" >&2; status=1 ;;
	esac

	if otool -L "$file_path" | awk 'NR > 1 { print $1 }' | grep -Evq '^(@rpath/|@loader_path/|@executable_path/|/System/|/usr/lib/)'; then
		echo "absolute non-system dependency found" >&2
		otool -L "$file_path" >&2
		status=1
	fi
done <<EOF
$(find "$bundle" -type f -print)
EOF

codesign --force --deep --sign - "$bundle"
codesign --verify --deep --strict --verbose=2 "$bundle"
exit "$status"
