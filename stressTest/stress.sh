#execute multiple (dynamic) curl commands in parallel
#-P parameter allows you to set the desired number of parallel executions
#-n parameter simply limits how many arguments are passed per execution

#cat urls.txt | xargs -n1 -P50 -I{} curl -w "$i: %{time_total} %{http_code} %{size_download} %{url_effective}\n" -o "/dev/null" -s {}
#cat urls.txt | xargs -n1 -P50 -I{} curl -w "$i: %{time_total} %{http_code} %{size_download}\n" -o "/dev/null" -s {}
cat checkChannelStatus.txt | xargs -n1 -P5 -I{} curl -w "$i: %{time_total} %{http_code} %{size_download}\n" -o "/dev/null" -s {}


