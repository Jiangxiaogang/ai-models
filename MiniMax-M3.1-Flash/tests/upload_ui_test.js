const fs = require('fs');
const http = require('http');
const path = require('path');

const PAGE = process.argv[2] || path.join(__dirname, '..', '..', 'up.html');
const BASE = process.argv[3] || 'http://127.0.0.1:8080';
const FILE = process.argv[4];

const html = fs.readFileSync(PAGE, 'utf8');
const match = html.match(/<script>([\s\S]*?)<\/script>/);
if (!match) {
    console.error('FAIL: page contains no <script> block');
    process.exit(1);
}
const script = match[1];
const ui = {};

function makeEl(tag) {
    const el = {
        tag, style: {}, className: '', children: [], value: '', files: [], _text: '', _sel: {},
        get textContent() { return el._text; },
        set textContent(v) { el._text = v; ui['text:' + v] = true; },
        get innerHTML() { return el._html || ''; },
        set innerHTML(v) {
            el._html = v;
            const plain = v.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim();
            ui['html:' + plain] = true;
        },
        appendChild(c) { el.children.push(c); },
        querySelector(sel) {
            if (!el._sel[sel]) {
                el._sel[sel] = makeEl(sel);
                ui['sel:' + sel] = true;
            }
            return el._sel[sel];
        }
    };
    return el;
}

const elements = {};
global.document = {
    getElementById(id) {
        if (!elements[id]) elements[id] = makeEl('div');
        return elements[id];
    },
    createElement(tag) { return makeEl(tag); }
};

let lastRequest = null;
class XHR {
    constructor() { this.upload = { onprogress: null }; this.headers = {}; }
    open(method, url) { this.method = method; this.url = url; }
    setRequestHeader(k, v) { this.headers[k] = v; }
    send(body) {
        const payload = body && body.buffer ? body.buffer : Buffer.from(String(body));
        lastRequest = { method: this.method, url: this.url, headers: this.headers, size: payload.length };
        const u = new URL(this.url, BASE);
        const req = http.request({
            hostname: u.hostname, port: u.port, path: u.pathname + u.search,
            method: this.method, headers: Object.assign({ 'Content-Length': payload.length }, this.headers)
        }, res => {
            this.status = res.statusCode;
            let data = '';
            res.on('data', c => data += c);
            res.on('end', () => {
                this.responseText = data;
                if (this.onload) this.onload();
            });
        });
        req.on('error', e => { if (this.onerror) this.onerror(e); });
        req.end(payload);
        let sent = 0;
        const step = () => {
            sent = Math.min(payload.length, sent + Math.ceil(payload.length / 6));
            if (this.upload.onprogress) this.upload.onprogress({ loaded: sent, total: payload.length, lengthComputable: true });
            if (sent < payload.length) setTimeout(step, 40);
        };
        setTimeout(step, 40);
    }
}
global.XMLHttpRequest = XHR;
global.performance = { now: () => Date.now() };
global.alert = (m) => { ui['alert:' + m] = true; };

eval(script);

const data = fs.readFileSync(FILE);
elements['file'].files = [{ name: path.basename(FILE), size: data.length, buffer: data }];
elements['dir'].value = 'uploads';
elements['btn'].onclick();

setTimeout(() => {
    const text = Object.keys(ui).filter(k => k.startsWith('text:') || k.startsWith('html:'));
    const progress = text.filter(k => /%/.test(k));
    const speeds = text.filter(k => /B\/s|KB\/s|MB\/s/.test(k));
    const done = text.some(k => /上传完成/.test(k));
    console.log('request      :', JSON.stringify(lastRequest));
    console.log('progress rows:', progress.length ? progress.join(' | ') : '(none)');
    console.log('speed rows   :', speeds.length ? speeds.join(' | ') : '(none)');
    console.log('completed    :', done);
    console.log(done && progress.length && speeds.length ? 'PASS' : 'FAIL');
    process.exit(done && progress.length && speeds.length ? 0 : 1);
}, 2500);
