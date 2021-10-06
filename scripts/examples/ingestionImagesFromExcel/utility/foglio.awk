
BEGIN {
	FS="\t";
	retention="20y";
	outputPathName="./output.sh"

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	title=$1;
	imageURL=$5;
	imageURLLower=tolower(imageURL);
	if (index(imageURLLower, ".png") != 0)
		fileFormat="png";
	else if (index(imageURLLower, ".jpg") != 0)
		fileFormat="jpg";
	if (title != "" && imageURL != "" and fileFormat != "")
	{
		printf("\n") >> outputPathName;

		gsub(/\//, "\\\/", title);
		gsub(/\//, "\\\/", imageURL);
		printf("sed \"s/__title__/%s/g\" ./utility/ingestionImageTemplate.json | sed \"s/__url__/%s/g\" | sed \"s/__fileformat__/%s/g\" | sed \"s/__retention__/%s/g\" > ./ingestionImage.json\n", title, imageURL, fileFormat, retention) >> outputPathName;

		printf("curl -k -v -X POST -u %s:%s -d @./ingestionImage.json -H \"Content-Type: application/json\" https://mms-api.cloud-mms.com/catramms/v1/ingestion\n", userKey, apiKey) >> outputPathName;
	}
	else
	{
		if (title == "")
			printf("Title is missing\n");
		else if (imageURL == "")
			printf("ImageURL is missing\n");
		else if (fileFormat == "")
			printf("FileFormat is missing\n");
	}
}

