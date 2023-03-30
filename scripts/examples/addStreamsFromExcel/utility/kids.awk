
BEGIN {
	FS="\t";
	outputPathName="./output.sh"

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	position=$2
	title=$3
	year=$4
	movieURL=$8
	language=$1
	genre=$5
	episodeNumber=$6
	episodeTitle=$7
	description=$9
	#duration=$9

	if (NR == 1 && title == "Titolo")
	{
		printf("First row skipped\n");
		
		next;
	}

	if (title != "" && movieURL != "")
	{
		printf("\n") >> outputPathName;

		gsub(/\//, "\\/", title);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", title);

		gsub(/\//, "\\/", description);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", description);

		gsub(/\//, "\\/", movieURL);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", movieURL);

		gsub(/\//, "\\/", episodeTitle);
		gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", episodeTitle);

		if (year == "")
			year = "null";

		categories="";
		len=split(genre, arr, " - ");
		for(i=1; i<=len; i++)
		{
			category=arr[i];
			sub(/^[ \t]+/, "", category)
			sub(/[ \t]+$/, "", category)

			if (categories != "")
				categories=categories", ";
			categories=categories"\\\""category"\\\"";
		}

		printf("sed \"s/__title__/%s/g\" ./utility/kids_addStreamTemplate.json | sed \"s/__url__/%s/g\" | sed \"s/__description__/%s/g\" | sed \"s/__year__/%s/g\" | sed \"s/__categories__/%s/g\" | sed \"s/__position__/%s/g\" | sed \"s/__language__/%s/g\" | sed \"s/__episodeTitle__/%s/g\" | sed \"s/__episodeNumber__/%s/g\"> ./outputAddStream.json\n", title, movieURL, description, year, categories, position, language, episodeTitle, episodeNumber) >> outputPathName;

		printf("curl -k -X POST -u %s:%s -d @./outputAddStream.json -H \"Content-Type: application/json\" https://%s/catramms/1.0.1/conf/stream\n", userKey, apiKey, mmsApiHostname) >> outputPathName;
	}
	else
	{
		if (title == "")
			printf("title is missing\n");
		else if (movieURL == "")
			printf("movieURL is missing\n");
	}
}

