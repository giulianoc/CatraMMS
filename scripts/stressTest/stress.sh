cat urls.txt | xargs -n1 -P10 -I{} curl --header 'Authorization: 4686dee867885c0a900043ff0ffe9267' {}

