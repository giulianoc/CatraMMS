
BEGIN {
	FS="\t";
	retention="20y";
	outputPathName="./output.sh"

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	title=$2;
	imageURL=$10;

	if (NR == 1 && title == "Titolo")
	{
		printf("First row skipped\n");
		
		next;
	}

	fileFormat=""
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
		# printf("imageURL: %s\n", imageURL);
		# printf("fileFormat: %s\n", fileFormat);
		printf("sed \"s/__title__/%s/g\" ./utility/ingestionImageTemplate.json | sed \"s/__uniqueName__/%s/g\" | sed \"s/__url__/%s/g\" | sed \"s/__fileformat__/%s/g\" | sed \"s/__retention__/%s/g\" > ./outputIngestionImage.json\n", title, title, imageURL, fileFormat, retention) >> outputPathName;

		printf("curl -k -v -X POST -u %s:%s -d @./outputIngestionImage.json -H \"Content-Type: application/json\" https://%s/catramms/v1/ingestion\n", userKey, apiKey, mmsApiHostname) >> outputPathName;
	}
	else
	{
		if (title == "")
			printf("Title is missing\n");
		else if (imageURL == "")
			printf("ImageURL is missing\n");
		else if (fileFormat == "")
			printf("FileFormat is missing, wrong image URL: %s\n", imageURL);
	}
}

