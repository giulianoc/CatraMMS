
BEGIN {
	FS="\t";
	outputPathName="./output.sh"

	position=5555

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	title=$1;
	year=$2
	filmURL=$3
	language=$6
	genere=$7
	description=$8

	if (title != "" && filmURL != "")
	{
		printf("\n") >> outputPathName;

		gsub(/\//, "\\\/", title);
		gsub(/\//, "\\\/", filmURL);
		gsub(/\//, "\\\/", description);

		categories="";
		split(genere, a, "/");
		for(int index=0; index<a.length; index++)
		{
			category=a[index];
			sub(/^[ \t]+/, "", category)
			sub(/[ \t]+$/, "", category)

			if (categories != "")
				categories=categories", ";
			categories=categories"\""category"\"";
		}

		printf("sed \"s/__title__/%s/g\" ./utility/addIPChannelTemplate.json | sed \"s/__url__/%s/g\" | sed \"s/__description__/%s/g\" | sed \"s/__position__/%d/g\" | sed \"s/__year__/%s/g\" | sed \"s/__categories__/%s/g\" | sed \"s/__language__/%s/g\" > ./addIPChannel.json\n", title, filmURL, description, position, year, categories, language) >> outputPathName;

		printf("curl -k -v -X POST -u %s:%s -d @./addIPChannel.json -H \"Content-Type: application/json\" https://mms-api.cloud-mms.com/catramms/v1/conf/ipChannel\n", userKey, apiKey) >> outputPathName;
	}
	else
	{
		if (title == "")
			printf("Title is missing\n");
		else if (filmURL == "")
			printf("filmURL is missing\n");
	}
}

