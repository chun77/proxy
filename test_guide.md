
# Testing HTTP Requests using curl

`curl` is a command-line tool used to transfer data to or from a server. It supports a variety of protocols, including HTTP, HTTPS, FTP, and more. This guide will show you how to use `curl` to test `CONNECT`, `GET`, and `POST` requests to a server or proxy.

## Testing CONNECT Requests

The `CONNECT` method is used by the client to establish a network connection to a web server or proxy.

```sh
curl -x http://localhost:8080 -v https://www.google.com
```

Replace `[proxy_host]`, `[proxy_port]`, and `[url]` with your proxy's host, proxy's port, and the URL you want to access, respectively. The `-v` flag is used for verbose output, showing the request and response headers. The `--proxytunnel` flag tells `curl` to use the `CONNECT` method.

## Testing GET Requests

The `GET` method requests a representation of the specified resource. Requests using `GET` should only retrieve data.

```sh
curl -x http://localhost:8080 -X GET "http://example.com/resource"
```

## Testing POST Requests

The `POST` method is used to submit an entity to the specified resource, often causing a change in state or side effects on the server.

```sh
curl -x http://localhost:8080 -X POST -d "param1=value1&param2=value2" "http://example.com/resource"
```

## Notes
- The `-v` flag is useful for debugging and understanding the HTTP request and response process.
