
BEGIN {
	FS="\t";
	outputPathName="./output.sh"

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	position=$1
	title=$2;
	year=$3
	movieURL=$4
	language=$7
	genre=$8
	description=$9
	duration=$10

	if (NR == 1 && title == "TITOLO")
	{
		printf("First row skipped\n");
		
		next;
	}

	if (title != "" && movieURL != "")
	{
		printf("\n") >> outputPathName;

		gsub(/\//, "\\\/", title);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", title);

		gsub(/\//, "\\\/", description);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", description);

		gsub(/\//, "\\\/", movieURL);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", movieURL);

		if (year == "")
			year = "null";

		categories="";
		len=split(genre, arr, "/");
		for(i=1; i<=len; i++)
		{
			category=arr[i];
			sub(/^[ \t]+/, "", category)
			sub(/[ \t]+$/, "", category)

			if (categories != "")
				categories=categories", ";
			categories=categories"\\\""category"\\\"";
		}

		printf("sed \"s/__title__/%s/g\" ./utility/movie_addIPChannelTemplate.json | sed \"s/__url__/%s/g\" | sed \"s/__description__/%s/g\" | sed \"s/__position__/%d/g\" | sed \"s/__year__/%s/g\" | sed \"s/__categories__/%s/g\" | sed \"s/__language__/%s/g\" | sed \"s/__duration__/%s/g\" > ./outputAddIPChannel.json\n", title, movieURL, description, position, year, categories, language, duration) >> outputPathName;

		printf("curl -k -u %s:%s -d @./outputAddIPChannel.json -H \"Content-Type: application/json\" https://%s/catramms/1.0.1/conf/ipChannel\n", userKey, apiKey, mmsApiHostname) >> outputPathName;
	}
	else
	{
		if (serieTitle == "")
			printf("serieTitle is missing\n");
		else if (filmURL == "")
			printf("filmURL is missing\n");
	}
}

