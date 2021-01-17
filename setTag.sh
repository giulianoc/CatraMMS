#!/bin/bash


if [ $# -ne 2 ]
then
    echo "usage $0 <version tag, i.e.: 1.0.0> <tag message>"
	echo "Reminder to list tags: git tag -n --sort=taggerdate"

    exit
fi

tagName=$1
tagMessage=$2

echo "git tag -a $tagName -m \"Version $tagName: $tagMessage\""
git tag -a $tagName -m "Version $tagName: $tagMessage"

echo "echo \"$tagName\" > version.txt"
echo "$tagName" > version.txt

