
BEGIN {
	FS="\t";
	outputPathName="./output.sh"

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	language=$1
	position=$2
	title=$3;
	year=$4
	genre=$5
	episodeNumber=$6
	episodeTitle=$7
	movieURL=$8
	season=$9
	description=$10

	if (NR == 1 && title == "Titolo")
	{
		printf("First row skipped\n");
		
		next;
	}

	if (title != "" && movieURL != "")
	{
		printf("\n") >> outputPathName;

		gsub(/\//, "\\\/", title);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", title);

		gsub(/\//, "\\\/", episodeTitle);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", episodeTitle);

		gsub(/\//, "\\\/", movieURL);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", movieURL);

		gsub(/\//, "\\\/", description);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", description);

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

		printf("sed \"s/__title__/%s/g\" ./utility/addIPChannelTemplate.json | sed \"s/__url__/%s/g\" | sed \"s/__description__/%s/g\" | sed \"s/__position__/%d/g\" | sed \"s/__year__/%s/g\" | sed \"s/__categories__/%s/g\" | sed \"s/__language__/%s/g\" | sed \"s/__episodeTitle__/%s/g\" | sed \"s/__episodeNumber__/%s/g\" | sed \"s/__season__/%s/g\" > ./outputAddIPChannel.json\n", title, movieURL, description, position, year, categories, language, episodeTitle, episodeNumber, season) >> outputPathName;

		printf("curl -k -u %s:%s -d @./outputAddIPChannel.json -H \"Content-Type: application/json\" https://%s/catramms/v1/conf/ipChannel\n", userKey, apiKey, mmsApiHostname) >> outputPathName;
	}
	else
	{
		if (title == "")
			printf("Title is missing\n");
		else if (filmURL == "")
			printf("filmURL is missing\n");
	}
}
