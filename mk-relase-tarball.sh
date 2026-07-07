#!/bin/bash

# mk-release-tarball.sh
# run anywhere you can write to

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

REPO=/home/wael/getdiff

TMP=/tmp

PROG=getdiff

VERSION=$(grep "^#define VERSION" "$REPO/myinclude/getdiff.h" | cut -d\" -f2)

DIR2TAR=$PROG-$VERSION

rm -rf $TMP/$DIR2TAR

mkdir -p $TMP/$DIR2TAR

pushd $TMP

cp -r "$REPO/myinclude" "$DIR2TAR"
cp -r "$REPO/src" "$DIR2TAR"

cp "$REPO/LICENSE" \
   "$REPO/makefile" \
   "$REPO/README.md" \
   "$REPO/getdiff.conf.example" \
   "$REPO/getdiff.help" \
   "$REPO/README.changeFiles.md" \
   "$DIR2TAR"

mkdir -p "$REPO/releases/download/v$VERSION"

tar -czvf "$REPO/releases/download/v$VERSION/$DIR2TAR.tar.gz" "$DIR2TAR"

cd "$REPO/releases/download/v$VERSION"

md5sum "$DIR2TAR.tar.gz" > "$DIR2TAR.tar.gz.md5"

cd "$REPO" || exit 1

if git rev-parse "v$VERSION" >/dev/null 2>&1; then
    echo "Tag v$VERSION already exists."
else
    git tag -a "v$VERSION" -m "Release $VERSION"
    echo "Created tag v$VERSION"
fi


popd

rm -rf $TMP/$DIR2TAR

# update SlackBuild script and its info file

NEW_MD5=$(cat "$REPO/releases/download/v$VERSION/$DIR2TAR.tar.gz.md5" | cut -d ' ' -f1)

GITHUB=https://github.com/waelhammoudeh/getdiff
HOMEPAGE="$GITHUB"
DOWNLOAD="$GITHUB/releases/download/v$VERSION/getdiff-$VERSION.tar.gz"

sed -i "s/^VERSION=.*/VERSION=$VERSION/" "$REPO/getdiff.SlackBuild/getdiff.SlackBuild"

echo "Updated version # in SlackBuild"

# overwrite info file
cat << EOF > "$REPO/getdiff.SlackBuild/getdiff.info"
PRGNAM="getdiff"
VERSION="$VERSION"
HOMEPAGE="$HOMEPAGE"
DOWNLOAD="$DOWNLOAD"
MD5SUM=""
DOWNLOAD_x86_64=""
MD5SUM_x86_64="$NEW_MD5"
REQUIRES=""
MAINTAINER="Wael Hammoudeh"
EMAIL="w_hammoudeh@hotmail.com"
EOF

echo "Wrote new SlackBuild info file"

exit 0

