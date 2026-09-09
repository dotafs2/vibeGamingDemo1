// Native browser comparison of the anime and actual engine frame; no repainting.
const fs=require('fs'),path=require('path');
const {chromium}=require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const base=path.resolve(__dirname,'../../validation/market_demo_v3');
const revision=process.argv[2]||'r4';
if(!/^r\d+$/.test(revision))throw Error('Invalid capture revision');
const out=path.join(base,revision);
const contract=JSON.parse(fs.readFileSync(path.join(base,'reference_landmarks.json'),'utf8'));
const capture=JSON.parse(fs.readFileSync(path.join(out,'capture.json'),'utf8'));
const log=path.join(base,'root_owned_processes.jsonl');
const record=(role,pid)=>fs.appendFileSync(log,JSON.stringify({role,pid,started:new Date().toISOString()})+'\n');
const data=file=>'data:image/png;base64,'+fs.readFileSync(path.join(out,file)).toString('base64');
const measured=capture.projected_landmarks.map(m=>{
  const t=contract.projected_markers.find(p=>p.id===m.name);
  return {...m,target:t?[t.u,t.v]:null,delta:t?[m.uv[0]-t.u,m.uv[1]-t.v]:null};
});
fs.writeFileSync(path.join(out,'projection_comparison.json'),JSON.stringify({revision,measurement_scope:'Named visible geometry points, not an overall image similarity score',markers:measured},null,2));
const html=`<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>Level0 市场街 · 模型复核 ${revision}</title><style>
*{box-sizing:border-box}body{margin:0;color:#17242c;background:#e8edef;font-family:'Microsoft YaHei',sans-serif}section{padding:22px 24px;background:#f8f9fa;margin-bottom:24px}h1{font-size:26px;margin:0 0 16px;font-weight:650}h1 span{font-size:16px;font-weight:400;color:#566671;margin-left:16px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:18px}figure{margin:0}figcaption{font-size:18px;margin-bottom:9px}img{width:100%;aspect-ratio:16/9;object-fit:cover;display:block}.note{font-size:16px;color:#52616a;margin:14px 0 0;line-height:1.6}a{color:#355f78}table{border-collapse:collapse;width:100%;font-size:16px}th,td{text-align:left;border-bottom:1px solid #c9d2d7;padding:10px} @media(max-width:900px){.pair{grid-template-columns:1fr}}
</style><section id="comparison"><h1>起始之城 · 市场街<span>模型与构图复核 ${revision} · 全局渲染尚待裁决</span></h1><div class="pair"><figure><figcaption><b>动画参考</b>｜原图仅隐去上下黑边</figcaption><img id="anime" src="${contract.reference.url}" alt="动画市场街"></figure><figure><figcaption><b>Godot 当前实机</b>｜独立重建的真实三维场景</figcaption><img src="${data('reference.png')}" alt="真实引擎市场街"></figure></div><p class="note">相同 16:9 画幅，按可见地标比较位置、轮廓和遮挡。人物继续使用已导入的桐人外观测试模型。<a href="${contract.reference.source_page}">动画参考来源</a> · 本次未更改全局光照、后处理或雾。</p></section>
<section id="details"><h1>真实引擎近景<span>门桥与圆塔的几何检查</span></h1><div class="pair"><figure><figcaption>门桥 · 拱洞与砌块</figcaption><img src="${data('gate_detail.png')}"></figure><figure><figcaption>圆塔 · 连拱与栏板</figcaption><img src="${data('tower_detail.png')}"></figure></div></section>
<section><h1>投影核对</h1><p class="note">下列是标记点误差；必须结合上方真实轮廓及独立审查，不能替代整幅画面验收。</p><table><thead><tr><th>标记</th><th>引擎 u, v</th><th>参考 u, v</th><th>差值 Δu, Δv</th></tr></thead><tbody>${measured.map(m=>`<tr><td>${m.name}</td><td>${m.uv.map(n=>n.toFixed(4)).join(', ')}</td><td>${m.target?m.target.map(n=>n.toFixed(4)).join(', '):'无对应硬点'}</td><td>${m.delta?m.delta.map(n=>n.toFixed(4)).join(', '):'不计分'}</td></tr>`).join('')}</tbody></table></section></html>`;
(async()=>{
 record('root comparison Node',process.pid);
 fs.writeFileSync(path.join(out,'comparison.html'),html);
 const server=await chromium.launchServer({headless:true,channel:'msedge'});
 record('root comparison Edge',server.process().pid);
 let browser;
 try{
  browser=await chromium.connect(server.wsEndpoint());
  const page=await browser.newPage({viewport:{width:2160,height:1500},deviceScaleFactor:1});
  await page.goto('file:///'+path.join(out,'comparison.html').replaceAll('\\','/'),{waitUntil:'domcontentloaded',timeout:20000});
  await page.waitForFunction(()=>[...document.images].every(i=>i.complete&&i.naturalWidth>0),null,{timeout:25000});
  for(const key of ['comparison','details'])await page.locator('#'+key).screenshot({path:path.join(out,key+'.png'),timeout:10000});
  console.log(JSON.stringify({revision,boards:['comparison.png','details.png'],reference:await page.locator('#anime').evaluate(i=>({width:i.naturalWidth,height:i.naturalHeight}))}));
 }finally{if(browser)await browser.close();await server.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
