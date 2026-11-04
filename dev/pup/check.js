const puppeteer = require('puppeteer-core');
(async () => {
  const browser = await puppeteer.launch({ executablePath: '/usr/bin/brave-browser', headless: 'new', args: ['--no-sandbox', '--disable-gpu'] });
  const page = await browser.newPage();
  await page.setViewport({ width: 1100, height: 1400 });
  const log = [];
  page.on('console', m => log.push('console.' + m.type() + ': ' + m.text()));
  page.on('pageerror', e => log.push('pageerror: ' + e.message));
  page.on('requestfailed', r => log.push('failed: ' + r.url() + ' ' + (r.failure() || {}).errorText));
  page.on('response', r => { if (r.url().includes('screen.raw')) log.push('screen.raw -> ' + r.status() + ' ' + (r.headers()['content-length'] || '?')); });
  const host = process.argv[2] || 'flux-orb.local';
  await page.goto(`http://${host}/#overview`, { waitUntil: 'load', timeout: 30000 });
  await new Promise(r => setTimeout(r, 9000));
  const info = await page.evaluate(() => { const c = document.getElementById('cv'); const d = c.getContext('2d').getImageData(0, 0, 240, 240).data; let nz = 0; for (let i = 0; i < d.length; i += 4) if (d[i] | d[i + 1] | d[i + 2]) nz++; return { nonBlackPixels: nz, stats: document.getElementById('stats').innerText.slice(0, 120), tabs: document.querySelectorAll('#nav button').length }; });
  await page.screenshot({ path: require('path').join(__dirname, '..', 'dash-brave.png') });
  console.log(JSON.stringify(info)); console.log(log.slice(0, 25).join('\n'));
  await browser.close();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
