const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const page = fs.readFileSync(process.argv[2] ? path.resolve(process.argv[2]) : path.join(__dirname, 'notification_background.html'));
const server = http.createServer((request, response) => {
    const url = new URL(request.url, 'http://127.0.0.1');
    if (url.pathname !== '/notification_background.html') {
        response.writeHead(404);
        response.end();
        return;
    }
    response.writeHead(200, {
        'Content-Type': 'text/html; charset=utf-8',
        'Content-Length': page.length,
        'Cache-Control': 'no-store'
    });
    response.end(page);
});
server.listen(0, '127.0.0.1', () => {
    console.log(`http://127.0.0.1:${server.address().port}/notification_background.html`);
});
process.stdin.once('data', () => {
    server.close(() => process.stdin.destroy());
    server.closeIdleConnections();
});