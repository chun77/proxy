from http.server import HTTPServer, BaseHTTPRequestHandler
from io import BytesIO

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        etag = '"123456"'
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.send_header('ETag', etag)
        self.end_headers()
        self.wfile.write(b"<html><body>Hello World</body></html>")

    def do_HEAD(self):
        etag = '"123456"'
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.send_header('ETag', etag)
        self.end_headers()

httpd = HTTPServer(('localhost', 8000), SimpleHTTPRequestHandler)
httpd.serve_forever()
