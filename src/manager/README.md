A simple example using the REST API:
```bash
# Run this in a separate window
$ ./rest
# Send JSON request using curl
$ curl -X POST --header "Content-Type:text/json" -d '{"uid": 1, "input": [1, 2, 3, 4]}' 127.0.0.1:1337/predict
```
