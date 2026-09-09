const { chromium } = require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const fs = require('fs');
const path = require('path');
const out = path.join(__dirname, 'reference_landmarks_overlay.png');
const url = 'https://i.pinimg.com/originals/92/a3/a7/92a3a78f0b36febbef3abcb70f37ff7e.png';
const markers = [
  ['gate',.451,.35,.686,.705], ['opening',.495,.455,.598,.655],
  ['tower',.545,.075,.687,.435], ['domes',.302,.15,.505,.43],
  ['right wall',.70,.06,1,.75], ['foreground crowd',.03,.48,.72,1], ['near character',.735,.53,.94,1]
];
const clothPoints = [['white canopy lower',.015,.525],['green canopy lower',.335,.555]];
(async()=>{
  console.log(JSON.stringify({helper_pid:process.pid}));
  const browser=await chromium.launch({headless:true,channel:'msedge'});
  try {
    const page=await browser.newPage({viewport:{width:1280,height:800}});
    // At 1280x800 the 16:9 image has 40px bars, not 80px bars.
    const boxes=markers.map(([n,x0,y0,x1,y1])=>`<rect x="${x0*1280}" y="${40+y0*720}" width="${(x1-x0)*1280}" height="${(y1-y0)*720}"/><text x="${x0*1280+5}" y="${40+y0*720+18}">${n}</text>`).join('');
    const dots=clothPoints.map(([n,u,v])=>`<circle cx="${u*1280}" cy="${40+v*720}" r="8"/><text x="${u*1280+10}" y="${40+v*720-10}">${n}</text>`).join('');
    await page.setContent(`<style>html,body{margin:0;background:#222}#stage{position:relative;width:1280px;height:800px}img{width:1280px;height:800px;display:block}svg{position:absolute;inset:0;stroke:#ff2d55;stroke-width:3;fill:none;font:18px sans-serif}circle{fill:#ff2d55}text{fill:#ff2d55;stroke:#fff;stroke-width:1;paint-order:stroke}</style><div id="stage"><img src="${url}"><svg width="1280" height="800" viewBox="0 0 1280 800">${boxes}${dots}</svg></div>`,{waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>document.images[0].complete&&document.images[0].naturalWidth>0,{timeout:25000});
    await page.screenshot({path:out,timeout:10000});
  } finally { await browser.close(); }
})().catch(e=>{console.error(e);process.exitCode=1;});
