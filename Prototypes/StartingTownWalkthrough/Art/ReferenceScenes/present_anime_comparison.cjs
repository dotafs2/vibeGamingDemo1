// Browser review sheet: real reference images beside unchanged engine captures.
// Remote images remain external URLs. No image-generation or paint-over step.
const fs = require('fs');
const path = require('path');
const {chromium} = require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const output = path.resolve(__dirname,'../../validation/anime_comparison');
const records = path.join(output,'owned_processes.jsonl');
const refs = [
  {key:'market', title:'起始之城 · 市场街',
   url:'https://i.pinimg.com/originals/92/a3/a7/92a3a78f0b36febbef3abcb70f37ff7e.png',
   source:'https://www.pinterest.com/pin/637470522238655015/',
   note:'动画文件含上下黑边；展示区仅用 CSS 隐去黑边，画面内容保留。'},
  {key:'tolbana', title:'托尔巴纳 · 喷泉广场',
   url:'https://vignette.wikia.nocookie.net/swordartonline/images/f/f6/Tolbana.png/revision/latest?cb=20140309033544',
   source:'https://swordartonline.fandom.com/wiki/Tolbana',
   note:'这是托尔巴纳，位于独立的第一层地理区域。'}
];
function record(role,pid){fs.appendFileSync(records,JSON.stringify({role,pid,started:new Date().toISOString()})+'\n');}
function html(){return `<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>Level0 · 动画与当前引擎对照</title>
<style>*{box-sizing:border-box}body{margin:0;background:#e7eaed;color:#1c242c;font-family:'Microsoft YaHei',sans-serif}main{max-width:2200px;margin:auto}section{background:#f7f8f9;margin:0 0 28px;padding:22px 24px 18px}h1{font-size:27px;font-weight:650;margin:0 0 16px}h1 span{font-size:17px;color:#52616c;font-weight:400;margin-left:20px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:18px}figure{margin:0;min-width:0}figcaption{font-size:19px;margin:0 0 9px}figcaption b{display:inline-block;margin-right:10px}img{display:block;width:100%;aspect-ratio:16/9;object-fit:cover;background:#d3d8dc}.note{font-size:16px;color:#495966;margin:12px 0 0;line-height:1.65}a{color:#36586a}footer{font-size:16px;max-width:1400px;margin:24px;line-height:1.8} @media(max-width:900px){.pair{grid-template-columns:1fr}h1 span{display:block;margin:6px 0}}
</style><main>${refs.map(r=>`<section id="${r.key}"><h1>${r.title}<span>构图对照 · 2026-09-09</span></h1><div class="pair"><figure><figcaption><b>动画参考</b>第三方托管截图</figcaption><img class="anime" src="${r.url}" alt="${r.title}动画参考"></figure><figure><figcaption><b>Godot 当前实机</b>仅校准比较机位，渲染未改</figcaption><img src="data:image/png;base64,${fs.readFileSync(path.join(output,r.key+'_matched.png')).toString('base64')}" alt="${r.title}当前引擎截图"></figure></div><p class="note">${r.note} <a href="${r.source}">参考来源</a> · 对齐主要地标的方向与画面位置；模型比例及遮挡差异仍保留，未做像素级复原。</p></section>`).join('')}
<footer>引擎图片由本机 Godot 4.7.2 Forward+ / Vulkan 渲染，1600 × 900；比较机位只在独立采集脚本中使用，没有更改正式相机预设、材质、光照或雾。人物为同一桐人素材的临时外观实例。上述画风仍待用户裁决。</footer></main></html>`;}
(async()=>{
  record('comparison Node helper',process.pid);
  const server = await chromium.launchServer({headless:true,channel:'msedge'});
  record('owned Edge comparison browser',server.process().pid);
  let browser;
  try{
    browser=await chromium.connect(server.wsEndpoint());
    const page=await browser.newPage({viewport:{width:2160,height:1500},deviceScaleFactor:1});
    fs.writeFileSync(path.join(output,'comparison.html'),html());
    await page.goto('file:///'+path.join(output,'comparison.html').replaceAll('\\','/'),{waitUntil:'domcontentloaded',timeout:20000});
    await page.waitForFunction(()=>[...document.images].every(i=>i.complete),null,{timeout:25000}).catch(()=>{});
    const images=await page.locator('img.anime').evaluateAll(imgs=>imgs.map(i=>({url:i.src,width:i.naturalWidth,height:i.naturalHeight,loaded:i.complete&&i.naturalWidth>0})));
    fs.writeFileSync(path.join(output,'reference_loading.json'),JSON.stringify(images,null,2));
    if(images.some(i=>!i.loaded))throw Error('Reference failed to load; comparison not accepted.');
    for(const r of refs)await page.locator('#'+r.key).screenshot({path:path.join(output,r.key+'_comparison.png'),timeout:10000});
    console.log(JSON.stringify({images,boards:refs.map(r=>r.key+'_comparison.png')}));
  }finally{
    if(browser)await browser.close();
    await server.close();
  }
})().catch(e=>{console.error(e);process.exitCode=1;});
