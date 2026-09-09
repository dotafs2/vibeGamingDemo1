const fs = require('fs');
const path = require('path');
const { chromium } = require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const out = path.join(__dirname, 'ExistingSAO');
fs.mkdirSync(out, { recursive: true });
(async () => {
  const server = await chromium.launchServer({ channel: 'msedge', headless: true });
  const processRecord = { pid: server.process().pid, started: new Date().toISOString(), exited: false };
  fs.writeFileSync(path.join(out, 'browser_process.json'), JSON.stringify(processRecord, null, 2));
  let browser;
  const results = [];
  try {
    browser = await chromium.connect(server.wsEndpoint());
    const context = await browser.newContext({ acceptDownloads: true });
    const urls = process.argv.slice(2);
    for (const url of urls) {
      const page = await context.newPage();
      try {
        await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 20000 });
        await page.waitForTimeout(2500);
        const result = { url, final: page.url(), text: (await page.locator('body').innerText()).slice(0, 17000), controls: await page.locator('button, a, input[type=submit]').evaluateAll(nodes => nodes.filter(n => /download|ダウンロード|free|sign in|log in|login/i.test(n.innerText || n.value || '')).map(n => ({ text: (n.innerText || n.value).slice(0,180), href: n.getAttribute('href'), id:n.id })).slice(0,35)), images: await page.locator('meta[property="og:image"], .thumbnail img').evaluateAll(nodes => nodes.map(n=>n.content||n.src)) };
        if (url.includes('bowlroll.net/file/96677')) {
          const button = page.locator('#file-show-download-control button').filter({hasText:/download/i}).first();
          if (await button.count()) {
            const event = page.waitForEvent('download', { timeout: 20000 });
            await button.click({timeout:5000});
            try { const download = await event; const file=path.join(out,path.basename(download.suggestedFilename())); await download.saveAs(file);result.download=file; }
            catch (e) { result.download_error=String(e);result.after=(await page.locator('body').innerText()).slice(0,7000); }
          }
        }
        results.push(result);
        console.log(JSON.stringify(result));
      } catch(e) { results.push({url,error:String(e)}); console.log(JSON.stringify(results.at(-1))); }
      finally { await page.close(); }
    }
  } finally {
    if (browser) await browser.close();
    await server.close();
    processRecord.exited = server.process().exitCode !== null || server.process().signalCode !== null;
    processRecord.finished = new Date().toISOString();
    fs.writeFileSync(path.join(out, 'browser_process.json'), JSON.stringify(processRecord, null, 2));
    fs.writeFileSync(path.join(out, 'page_checks.json'), JSON.stringify(results, null, 2));
  }
})().catch(e => { console.error(e);process.exitCode=1; });
