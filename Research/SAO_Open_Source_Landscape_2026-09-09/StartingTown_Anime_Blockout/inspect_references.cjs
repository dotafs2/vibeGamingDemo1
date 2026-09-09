// Bounded, read-only inspection of the six stills on the official episode page.
const { chromium } = require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const fs = require('fs');
const path = require('path');
(async () => {
  console.log(JSON.stringify({ helper_pid: process.pid }));
  const browser = await chromium.launch({ headless: true, channel: 'msedge' });
  try {
    const episode = process.argv[2] || 'ep01';
    if (!/^(ep(01|11|12)|plaza)$/.test(episode)) throw new Error('Only the selected references are in scope.');
    const page = await browser.newPage({ viewport: { width: 1400, height: 1050 } });
    let stills;
    if (episode === 'plaza') {
      stills = ["https://vignette.wikia.nocookie.net/swordartonline/images/0/07/Town_of_Beginnings%27_central_plaza_from_a_distance.png/revision/latest?cb=20140308064858", "https://i.pinimg.com/originals/92/a3/a7/92a3a78f0b36febbef3abcb70f37ff7e.png"];
    } else {
      await page.goto('https://www.swordart-online.net/aincrad/story/?id='+episode, { waitUntil: 'domcontentloaded', timeout: 25000 });
      stills = await page.locator('img').evaluateAll(imgs => [...new Set(imgs.map(i => i.src).filter(s => s.includes('_photo_')))].slice(0,6));
    }
    const caption = episode === 'plaza' ? 'Anime stills hosted by third parties — provenance needs cross-check' : 'Official SAO '+episode+' stills';
    await page.setContent('<html><body style="margin:20px;background:#eee;font:16px sans-serif"><h2>'+caption+'</h2><div style="display:grid;grid-template-columns:1fr 1fr;gap:16px">' + stills.map((src,i) => `<section><div>${i+1}</div><img src="${src}" style="width:640px;height:360px;object-fit:contain;background:#ddd"></section>`).join('') + '</div></body></html>');
    await page.waitForFunction(() => [...document.images].every(i => i.complete), null, { timeout: 20000 });
    const status = await page.locator('img').evaluateAll(imgs => imgs.map(i => ({url:i.src,width:i.naturalWidth,height:i.naturalHeight})));
    await page.screenshot({ path: path.join(__dirname, 'official_'+episode+'_inspection.png'), fullPage: true });
    fs.writeFileSync(path.join(__dirname, 'official_'+episode+'_status.json'), JSON.stringify(status,null,2));
    console.log(JSON.stringify(status));
  } finally { await browser.close(); }
})().catch(e => { console.error(e.message); process.exitCode = 1; });
