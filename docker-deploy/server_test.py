from http.server import HTTPServer, BaseHTTPRequestHandler


class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        # Respond with a 200 status code
        self.send_response(200)
        # Send the headers
        self.send_header('Content-type', 'text/plain; charset=utf-8')
        self.end_headers()
        # Send the response body
        self.wfile.write(b'Hello, this is a simple HTTP server!')

    def do_POST(self):
        # Respond with a 200 status code
        self.send_response(200)
        # Send the headers
        self.send_header('Content-type', 'text/plain; charset=utf-8')
        self.end_headers()
        # Assume we want to read the content length to read the body
        content_length = int(self.headers['Content-Length'])
        # Read the body of the POST request
        post_data = self.rfile.read(content_length)
        # You can process the post_data here
        # Send a simple response body
        response = f"Received POST request with data: {post_data.decode('utf-8')}"
        self.wfile.write(response.encode('utf-8'))


# Set the server address and port
server_address = ('', 8081)  # Empty string '' means bind to all available interfaces

# Creating the HTTP server
httpd = HTTPServer(server_address, SimpleHTTPRequestHandler)

# Running the server
httpd.serve_forever()
