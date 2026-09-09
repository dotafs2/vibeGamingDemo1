// Inspect three explicitly selected public references. No media files are downloaded.
const { chromium } = require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const fs = require('fs');
const path = require('path');
const refs = [
  {name:'A / Anime starting-town market; third-party still',url:'https://i.pinimg.com/originals/92/a3/a7/92a3a78f0b36febbef3abcb70f37ff7e.png'},
  {name:'B / Hollow Realization; official game image, separate continuity',url:'https://hr.sao-game.jp/images/system/world/town-image-slide-1.jpg'},
  {name:'C / Tolbana; another first-floor town, third-party still',url:'https://vignette.wikia.nocookie.net/swordartonline/images/f/f6/Tolbana.png/revision/latest?cb=20140309033544'}
];
(async()=>{
  console.log(JSON.stringify({helper_pid:process.pid}));
  const browser=await chromium.launch({headless:true,channel:'msedge'});
  try {
    const page=await browser.newPage({viewport:{width:1300,height:1500}});
    await page.setContent('<html><body style="background:#eee;font:16px sans-serif;margin:20px">'+refs.map(r=>`<p>${r.name}</p><img src="${r.url}" style="width:1100px;height:390px;object-fit:contain">`).join('')+'</body></html>',{waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>[...document.images].every(i=>i.complete),null,{timeout:25000}).catch(()=>{});
    const result=await page.locator('img').evaluateAll(imgs=>imgs.map(i=>({url:i.src,width:i.naturalWidth,height:i.naturalHeight,loaded:i.complete&&i.naturalWidth>0})));
    fs.writeFileSync(path.join(__dirname,'official_style_reference_status.json'),JSON.stringify(result,null,2));
    await page.screenshot({path:path.join(__dirname,'official_style_reference_inspection.png'),fullPage:true,timeout:10000});
    console.log(JSON.stringify(result));
  } finally {await browser.close();}
})().catch(e=>{console.error(e.message);process.exitCode=1;});
