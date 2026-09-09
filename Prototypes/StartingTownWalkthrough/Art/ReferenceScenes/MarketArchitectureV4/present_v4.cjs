// Browser layout only. All right-hand pixels come from actual Godot captures.
const fs=require('fs'),path=require('path');
const {chromium}=require('C:/Users/quchenxi/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const base=path.resolve(__dirname,'../../../validation/market_architecture_v4');
const rev=process.argv[2]||'r1';if(!/^r\d+$/.test(rev))throw Error('Invalid revision');
const out=path.join(base,rev),log=path.join(base,'owned_processes.jsonl');
const record=(role,pid)=>fs.appendFileSync(log,JSON.stringify({role,pid,started:new Date().toISOString()})+'\n');
const report=JSON.parse(fs.readFileSync(path.join(out,'capture.json'),'utf8'));
const source='https://i.pinimg.com/originals/92/a3/a7/92a3a78f0b36febbef3abcb70f37ff7e.png';
const data=name=>'data:image/png;base64,'+fs.readFileSync(path.join(out,name)).toString('base64');
const features=report.projected_features;
const svg=(side)=>`<svg viewBox="0 0 1600 900">${features.filter(f=>f.kind==='window_outline'||f.kind==='facade_outline'||f.kind==='open_arch').map(f=>`<polyline points="${(side==='reference'?f.reference_px.map(p=>p.map(v=>v/1.6)):f.engine_px).map(p=>p.join(',')).join(' ')}" fill="none" stroke="${f.kind==='window_outline'?'#f29145':'#bb3152'}" stroke-width="1.2"/>`).join('')}</svg>`;
const fig=(title,src,overlay='')=>`<figure><figcaption>${title}</figcaption><div class="frame"><img src="${src}">${overlay}</div></figure>`;
const pair=(id,title,left,right,note='')=>`<section id="${id}"><h1>${title}</h1><div class="pair">${left}${right}</div><p>${note}</p></section>`;
const html=`<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>市场街建筑修复 V4 ${rev}</title><style>*{box-sizing:border-box}body{margin:0;font-family:'Microsoft YaHei',sans-serif;background:#e5eaed;color:#20313e}section{padding:18px 22px;background:#f8fafb;margin-bottom:20px}h1{font-size:23px;margin:0 0 14px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:16px}figure{margin:0}figcaption{font-size:17px;margin-bottom:8px}.frame{position:relative;aspect-ratio:16/9;overflow:hidden}.frame img{width:100%;height:100%;object-fit:cover;display:block}svg{position:absolute;inset:0;width:100%;height:100%}p{font-size:14px;color:#52646e;margin:12px 0 0;line-height:1.6}</style>
${pair('comparison','起始之城 · 市场街｜建筑修复 V4 '+rev,fig('动画原图 · 上下黑边仅作版面裁切',source),fig('Godot 实机 · 固定参考机位',data('reference.png')),'本轮修复几何；全局光照、后处理与雾未改。动画为第三方托管参考，未证实原生分辨率；模型与画风均等待用户验收。')}
${pair('architecture','排除人物遮挡 · 检查建筑',fig('动画参考',source),fig('同一帧机位 · 仅隐藏测试人物',data('architecture.png')),'窗洞、外挑阳台、屋顶和棚布均为真实三维几何。未使用动画截图作模型贴图或背景。')}
${pair('traces','轮廓核对 · 左侧源图描线，右侧引擎投影',fig('手工描线（仍需目视检查准确性）',source,svg('reference')),fig('相应三维轮廓在实际 Camera3D 的投影',data('architecture.png'),svg('engine')),'描线与引擎投影接近仅能验证导入和机位；不能单独证明整个建筑复原正确。')}
${pair('details','近景与移动镜头',fig('实际建筑近景',data('close.png')),fig('实际相机横移＋前移',data('motion_090.png')))}
${pair('landmarks','圆塔与门桥',fig('圆塔',data('tower_detail.png')),fig('门桥',data('gate_detail.png')))}</html>`;
(async()=>{record('root V4 comparison Node',process.pid);fs.writeFileSync(path.join(out,'comparison.html'),html);const server=await chromium.launchServer({headless:true,channel:'msedge'});record('root V4 comparison Edge',server.process().pid);let browser;try{browser=await chromium.connect(server.wsEndpoint());const page=await browser.newPage({viewport:{width:2160,height:1500},deviceScaleFactor:1});await page.goto('file:///'+path.join(out,'comparison.html').replaceAll('\\','/'));await page.waitForFunction(()=>[...document.images].every(i=>i.complete&&i.naturalWidth>0),null,{timeout:25000});for(const name of ['comparison','architecture','traces','details','landmarks'])await page.locator('#'+name).screenshot({path:path.join(out,name+'_comparison.png')});console.log(JSON.stringify({revision:rev,features:features.length}));}finally{if(browser)await browser.close();await server.close();}})().catch(e=>{console.error(e);process.exitCode=1});
