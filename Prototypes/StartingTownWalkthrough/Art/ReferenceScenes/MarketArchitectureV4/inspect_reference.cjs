// Inspect the existing anime reference in a browser at its file resolution.
// No reference artwork is used as a 3D material or an engine background.
const fs=require('fs'),path=require('path');
const {chromium}=require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const out=path.resolve(__dirname,'../../../validation/market_architecture_v4');
const url='https://i.pinimg.com/originals/92/a3/a7/92a3a78f0b36febbef3abcb70f37ff7e.png';
const log=path.join(out,'owned_processes.jsonl');
const record=(role,pid)=>fs.appendFileSync(log,JSON.stringify({pid,role,started:new Date().toISOString()})+'\n');
const regions=[['left_architecture',0,0,1240,920],['central_landmarks',770,55,1110,870],['right_architecture',1550,0,1010,1040],['canopy_structure',0,300,1850,600]];
(async()=>{
 record('root reference inspection Node',process.pid);
 const server=await chromium.launchServer({headless:true,channel:'msedge'});record('root reference inspection Edge',server.process().pid);
 let browser;
 try{
  browser=await chromium.connect(server.wsEndpoint());
  const page=await browser.newPage({viewport:{width:1900,height:1100},deviceScaleFactor:1});
  for(const [name,x,y,w,h] of regions){
   await page.setContent(`<style>body{margin:0}.crop{width:${w}px;height:${h}px;position:relative;overflow:hidden}img{position:absolute;width:2560px;height:1600px;left:${-x}px;top:${-y-80}px}</style><div class="crop"><img src="${url}"></div>`);
   await page.waitForFunction(()=>document.images[0].complete&&document.images[0].naturalWidth===2560,null,{timeout:25000});
   await page.locator('.crop').screenshot({path:path.join(out,name+'.png')});
   const grid=[];
   for(let xx=50;xx<w;xx+=100)grid.push(`<path d="M${xx} 0V${h}"/><text x="${xx+3}" y="18">x=${x+xx}</text>`);
   for(let yy=50;yy<h;yy+=100)grid.push(`<path d="M0 ${yy}H${w}"/><text x="3" y="${yy-4}">y=${y+yy}</text>`);
   await page.locator('.crop').evaluate((el,{grid,w,h})=>el.insertAdjacentHTML('beforeend',`<svg style="position:absolute;inset:0" width="${w}" height="${h}"><g stroke="#bd274766" stroke-width="1" fill="none">${grid.replaceAll('<text','<text style="font:13px monospace;fill:#91162f;stroke:white;stroke-width:2px;paint-order:stroke"')}</g></svg>`),{grid:grid.join(''),w,h});
   await page.locator('.crop').screenshot({path:path.join(out,name+'_grid.png')});
  }
  fs.writeFileSync(path.join(out,'reference_regions.json'),JSON.stringify({source_url:url,file_px:[2560,1600],active_rect_px:[0,80,2560,1440],regions:regions.map(([id,x,y,w,h])=>({id,active_rect_px:[x,y,w,h]}))},null,2));
  console.log(JSON.stringify({regions:regions.map(x=>x[0])}));
 }finally{if(browser)await browser.close();await server.close();}
})().catch(e=>{console.error(e);process.exitCode=1});
