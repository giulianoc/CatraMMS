cat urls_2.txt | xargs -n1 -P10 -I{} curl --header 'Authorization: dbca607c839fd8d023f7049035706a53' {}

