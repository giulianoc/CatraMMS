#!/bin/bash

#This script receives as input the workspaceDirectoryName and check if media files and media directories are present as well into the database
#This script generates a script doing this check (media files and directories) and, if removeFilesDirectories is 1,
#the files/directories not present into the database will be removed. In any case, a log file is generated with all the
#file/directories not present into the database.
if [ $# -ne 4 ];
then
	echo "Usage $0 <removeFilesDirectories (1 means 'remove', 0 just log)> <dbPassword> <partitionNumber (i.e.: 0)> <workspaceDirectoryName (i.e.: 3)>"

	exit 1
fi

#1 means yes
removeFilesDirectories=$1
dbPassword=$2
#i.e.: 0
partitionNumber=$3
#i.e.: 3
workspaceDirectoryName=$4


#i.e.: MMS_0000
repositoryName=$(printf MMS_%04d $partitionNumber)

echo "removeFilesDirectories: $removeFilesDirectories, dbPassword: $dbPassword, workspaceDirectoryName: $workspaceDirectoryName, repositoryName: $repositoryName (partitionNumber: $partitionNumber)"

read -n 1 -s -r -p "Press a key to start"
echo ""

currentDir=$(pwd)

workspacePathName=/var/catramms/storage/MMSRepository/$repositoryName/$workspaceDirectoryName
cd $workspacePathName

#check of the directory media (m3u8 directory/files)
#It is assumed we have ONLY m3u8 having the following format:
#<relative path (i.e.: /123/456/789/)><m3u8 directory (i.e.: 5384612_938916_75)><main m3u8 file (i.e.: 5384638_938983.m3u8)>
find ./ -mindepth 5 -maxdepth 5 -type f -name "*.m3u8" | awk -v dbPassword=$dbPassword -v partitionNumber=$partitionNumber -v workspacePathName=$workspacePathName -v removeFilesDirectories=$removeFilesDirectories -v logFileName=$currentDir/checkDirectoryAndRemove.log \
	'BEGIN { printf("#!/bin/bash\n\nmediaRemoved=0\nmediaIndex=0\n\n: > %s\n\n", logFileName); }	\
	{ relativePath=substr($1, 2, 13);	\
		endOfM3u8DirectoryIndex=index(substr($1, 15), "/");	\
		m3u8Directory=substr($1, 15, endOfM3u8DirectoryIndex);	\
		fileName=substr($1, 14+endOfM3u8DirectoryIndex+1);	\
		printf("#input: %s\n#relativePath: %s\n#endOfM3u8DirectoryIndex: %d\n#m3u8Directory: %s\n#fileName: %s\n", $1, relativePath, endOfM3u8DirectoryIndex, m3u8Directory, fileName);	\
		echoCommand="echo \"$mediaIndex: "fileName"\" >> "logFileName;	\
		printf("%s\n", echoCommand);	\
		printf("((mediaIndex++))\n");	\
		rmCommand="echo \"rm -rf "workspacePathName""relativePath""m3u8Directory"\" >> "logFileName;	\
		if (removeFilesDirectories == 1)	\
			rmCommand=rmCommand"\n\trm -rf "workspacePathName""relativePath""m3u8Directory;	\
			printf("count=$(echo \"select count(*) from MMS_PhysicalPath where partitionNumber = %s and relativePath = \x27%s%s\x27 and fileName = \x27%s\x27\" | psql --no-psqlrc -t -A \"postgresql://mms:%s@$MMS_DB_LOCALHOST:5432/mms\")\nif [ $count -eq 0 ]; then\n\t%s\n\tmediaRemoved=$((mediaRemoved+1))\nfi\n\n", partitionNumber, relativePath, m3u8Directory, fileName, dbPassword, rmCommand);	\
	}	\
	END { printf("echo \"Removed media: $mediaRemoved/%d\"", NR); }' > $currentDir/checkDirectoryAndRemove.sh

chmod u+x $currentDir/checkDirectoryAndRemove.sh

#check of the "simple" media (files)
find ./ -mindepth 4 -maxdepth 4 -type f | awk -v dbPassword=$dbPassword -v partitionNumber=$partitionNumber -v workspacePathName=$workspacePathName -v removeFilesDirectories=$removeFilesDirectories -v logFileName=$currentDir/checkFileAndRemove.log	\
	'BEGIN { printf("#!/bin/bash\n\nmediaRemoved=0\nmediaIndex=0\n\n: > %s\n\n", logFileName); }	\
	{ relativePath=substr($1, 2, 13);	\
		fileName=substr($1, 15);	\
		printf("#input: %s\n#relativePath: %s\n#fileName: %s\n", $1, relativePath, fileName);	\
		echoCommand="echo \"$mediaIndex: "fileName"\" >> "logFileName;	\
		printf("%s\n", echoCommand);	\
		printf("((mediaIndex++))\n");	\
		rmCommand="echo \"rm -rf "workspacePathName""relativePath""fileName"\" >> "logFileName;	\
		if (removeFilesDirectories == 1)	\
			rmCommand=rmCommand"\n\trm -rf "workspacePathName""relativePath""fileName;	\
		printf("count=$(echo \"select count(*) from MMS_PhysicalPath where partitionNumber = %s and relativePath = \x27%s\x27 and fileName = \x27%s\x27\" | psql --no-psqlrc -t -A \"postgresql://mms:%s@$MMS_DB_LOCALHOST:5432/mms\")\nif [ $count -eq 0 ]; then\n\t%s\n\tmediaRemoved=$((mediaRemoved+1))\nfi\n\n", partitionNumber, relativePath, fileName, dbPassword, rmCommand);	\
	}	\
	END { printf("echo \"Removed media: $mediaRemoved/%d\"", NR); }' > $currentDir/checkFileAndRemove.sh

chmod u+x $currentDir/checkFileAndRemove.sh

cd $currentDir

echo "Make sure to remove the script ($currentDir/check*AndRemove.sh) after his execution"

