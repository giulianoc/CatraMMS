#!/bin/bash

TEMP_DIR=~/tmp
PUBLISH_HTML_DIR=$(dirname $0)

echo $0

osName=$(uname -s)

CURRENT_DIRECTORY=$PWD

rm -rf $TEMP_DIR/CatraMMS.wiki
rm -rf $TEMP_DIR/html
rm -rf $PUBLISH_HTML_DIR/html

mkdir $TEMP_DIR/html
cd $TEMP_DIR
git clone https://github.com/giulianoc/CatraMMS.wiki.git

cd CatraMMS.wiki

fileNumber=0
for filename in *.md; do
	fileBaseName=$(basename "$filename" .md)

	cat $filename | pandoc -f gfm | sed "s/https:\/\/github.com\/giulianoc\/CatraMMS\/wiki/./g" > $TEMP_DIR/html/$fileBaseName.html

	echo "$fileNumber: Generated $TEMP_DIR/html/$fileBaseName.html"
	fileNumber=$((fileNumber + 1))
done

for link in *.md; do
	linkToAddHtmlExtension=$(basename "$link" .md)

	fileNumber=$((fileNumber - 1))
	echo "$fileNumber: $linkToAddHtmlExtension ..."

	for filename in *.md; do
		fileBaseName=$(basename "$filename" .md)

		if [ "$osName" == "Darwin" ]; then
			gsed "s/$linkToAddHtmlExtension/$linkToAddHtmlExtension.html/gI" $TEMP_DIR/html/$fileBaseName.html > $TEMP_DIR/html/$fileBaseName.html.tmp
		else
			sed "s/$linkToAddHtmlExtension/$linkToAddHtmlExtension.html/gI" $TEMP_DIR/html/$fileBaseName.html > $TEMP_DIR/html/$fileBaseName.html.tmp
		fi
		mv $TEMP_DIR/html/$fileBaseName.html.tmp $TEMP_DIR/html/$fileBaseName.html
	done
done

cd $CURRENT_DIRECTORY

cp layout.html $TEMP_DIR/html

mv $TEMP_DIR/html $PUBLISH_HTML_DIR

