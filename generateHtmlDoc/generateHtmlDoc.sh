#!/bin/bash

TEMP_DIR=~/tmp

#This script has to be called from the directory where this script is placed
#PUBLISH_HTML_DIR will be initialized automatically with the directory containing this script
PUBLISH_HTML_DIR=$(dirname $0)

echo $0

osName=$(uname -s)

CURRENT_DIRECTORY=$PWD

rm -rf $TEMP_DIR/CatraMMS.wiki
rm -rf $TEMP_DIR/www
rm -rf $PUBLISH_HTML_DIR/www

mkdir -p $TEMP_DIR/www
cd $TEMP_DIR
git clone https://github.com/giulianoc/CatraMMS.wiki.git

cd CatraMMS.wiki

#generate html and remove https://github.com/giulianoc/CatraMMS/wiki/
fileNumber=0
if [ "$osName" == "Darwin" ]; then
	pandocInputFormat=gfm
else
	pandocInputFormat=markdown
fi
for filename in *.md; do
	fileBaseName=$(basename "$filename" .md)

	if [ "$fileBaseName" == "_Sidebar" ]; then
		cat $filename | pandoc -f $pandocInputFormat | sed -E "s/href=\"https:\/\/github.com\/giulianoc\/CatraMMS\/wiki\/([^\"]*)/target="\"main\"" href=\"\1.html/g" > $TEMP_DIR/www/$fileBaseName.html
	else
		cat $filename | pandoc -f $pandocInputFormat | sed -E "s/href=\"https:\/\/github.com\/giulianoc\/CatraMMS\/wiki\/([^\"]*)/href=\"\1.html/g" > $TEMP_DIR/www/$fileBaseName.html
	fi

	echo "$fileNumber: Generated $TEMP_DIR/www/$fileBaseName.html"
	fileNumber=$((fileNumber + 1))
done

cd $CURRENT_DIRECTORY

#manage image (MMS_Physical_Architecture.png)
imageHtmlFileName=Home.html
cp $PUBLISH_HTML_DIR/../docs/MMS_Physical_Architecture.png $TEMP_DIR/www
if [ "$osName" == "Darwin" ]; then
	gsed "s/https:\/\/github.com\/giulianoc\/CatraMMS\/blob\/master\/docs\///gI" $TEMP_DIR/www/$imageHtmlFileName > $TEMP_DIR/www/$imageHtmlFileName.tmp
else
	sed "s/https:\/\/github.com\/giulianoc\/CatraMMS\/blob\/master\/docs\///gI" $TEMP_DIR/www/$imageHtmlFileName > $TEMP_DIR/www/$imageHtmlFileName.tmp
fi
mv $TEMP_DIR/www/$imageHtmlFileName.tmp $TEMP_DIR/www/$imageHtmlFileName

#manage image (UserRegistration.png)
imageHtmlFileName=User-registration.html
cp $PUBLISH_HTML_DIR/../docs/UserRegistration.png $TEMP_DIR/www
if [ "$osName" == "Darwin" ]; then
	gsed "s/https:\/\/github.com\/giulianoc\/CatraMMS\/blob\/master\/docs\///gI" $TEMP_DIR/www/$imageHtmlFileName > $TEMP_DIR/www/$imageHtmlFileName.tmp
else
	sed "s/https:\/\/github.com\/giulianoc\/CatraMMS\/blob\/master\/docs\///gI" $TEMP_DIR/www/$imageHtmlFileName > $TEMP_DIR/www/$imageHtmlFileName.tmp
fi
mv $TEMP_DIR/www/$imageHtmlFileName.tmp $TEMP_DIR/www/$imageHtmlFileName


cp $PUBLISH_HTML_DIR/index.html $TEMP_DIR/www

#cd $TEMP_DIR/www
#for htmlFile in *.html; do
#	sed "s/a href=/a target="\"main\"" href=/g" $TEMP_DIR/www/$htmlFile > $TEMP_DIR/www/$htmlFile.tmp
#	mv $TEMP_DIR/www/$htmlFile.tmp $TEMP_DIR/www/$htmlFile
#done

#cd $CURRENT_DIRECTORY

mv $TEMP_DIR/www $PUBLISH_HTML_DIR/www

