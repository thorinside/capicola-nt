#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 SOURCE_DIR DESTINATION PATCH_DIR" >&2
    exit 2
fi

source_dir=$1
destination=$2
patch_dir=$3

case "$destination" in
    build/capicola-overlay|*/build/capicola-overlay) ;;
    *)
        echo "refusing to replace unexpected overlay path: $destination" >&2
        exit 2
        ;;
esac

rm -rf -- "$destination"
mkdir -p "$destination"
cp -R "$source_dir/." "$destination/"

for patch_file in "$patch_dir"/*.patch; do
    [ -f "$patch_file" ] || continue
    patch -s -d "$destination" -p1 < "$patch_file"
done

touch "$destination/.prepared"
