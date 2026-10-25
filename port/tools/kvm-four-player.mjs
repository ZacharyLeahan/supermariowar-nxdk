// SPDX-License-Identifier: Apache-2.0
// Bounded four-route KVM input and private screenshot evidence; Node 22+.
import readline from 'node:readline';
import {mkdir,appendFile,writeFile,readdir,readFile} from 'node:fs/promises';
const out=process.argv[2];
const host=new URL(process.argv[3] || 'http://192.168.1.169');
if(!['http:','https:'].includes(host.protocol)||host.username||host.password||host.pathname!=='/')throw new Error('Use an http(s) origin without credentials');
const origin=host.origin;
if(!out)throw new Error('Evidence directory required');
await mkdir(out,{recursive:true});
const websocket=new URL('/ws',origin);websocket.protocol=host.protocol==='https:'?'wss:':'ws:';
const ws=new WebSocket(websocket);
const neutral=()=>{const r=new Uint8Array(20);r[1]=20;return r;};
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let owned=false,queue=Promise.resolve(),serial=(await readdir(out)).filter(n=>n.endsWith(".jpg")).length;
const abort=new AbortController();
fetch(`${origin}/stream`,{signal:abort.signal}).then(async r=>{const reader=r.body.getReader();while(!(await reader.read()).done){}}).catch(()=>{});
ws.onmessage=async({data})=>{const b=new Uint8Array(await data.arrayBuffer());if(b[0]===0x83)owned=b[1]===1;};
ws.onopen=()=>{ws.send(new Uint8Array([7]));console.log('CONNECTED');};
ws.onerror=e=>console.log('WS ERROR',e.message);
ws.onclose=()=>{abort.abort();console.log('CLOSED');process.exit();};
const digital={up:1,down:2,left:4,right:8,start:16,back:32,ls:64,rs:128};
const analog={a:4,b:5,x:6,y:7,black:8,white:9,lt:10,rt:11};
const send=(p,r)=>ws.send(new Uint8Array([8,p,...r]));
async function command(line){
 const c=JSON.parse(line);
 if(c.plan){
  if(!/^[a-zA-Z0-9_-]+\.jsonl$/.test(c.plan))throw new Error('Plan must be a JSONL basename in the evidence directory');
  const lines=(await readFile(`${out}/${c.plan}`,'utf8')).trim().split('\n');
  if(lines.length>200)throw new Error('Plan exceeds 200 bounded commands');
  for(const item of lines){if(JSON.parse(item).plan)throw new Error('Nested plan');await command(item);}
  console.log('PLAN COMPLETE',c.plan);return;
 }
 if(c.quit){ws.send(new Uint8Array([5]));ws.close();return;}
 if(!owned)throw new Error('No controller ownership');
 if(c.snap){
  await sleep(c.wait||200);
  const r=await fetch(`${origin}/api/v1/video/frame.jpg`,{signal:AbortSignal.timeout(5000)});
  if(!r.ok||!r.headers.get('content-type')?.startsWith('image/jpeg'))throw new Error('No JPEG');
  const name=`${String(serial++).padStart(3,'0')}-${c.snap.replace(/[^a-zA-Z0-9_-]/g,'_')}.jpg`;
  await writeFile(`${out}/${name}`,Buffer.from(await r.arrayBuffer()));console.log('FRAME',`${out}/${name}`);return;
 }
 const ms=c.ms||150;if(ms<20||ms>5000)throw new Error('Hold must be 20..5000 ms');
 const reports=[];
 for(const state of c.states || (c.players||[1]).map(player=>({player,buttons:c.buttons,axes:c.axes}))){
  const p=state.player;
  if(!Number.isInteger(p)||p<1||p>4)throw new Error('Player must be 1..4');
  const r=neutral();
  for(const b of (state.buttons||[])){
   if(b in digital)r[2]|=digital[b];else if(b in analog)r[analog[b]]=255;else throw new Error('Unknown button');
  }
  if(state.axes){if(state.axes.length!==4||state.axes.some(n=>!Number.isInteger(n)||n< -32768||n>32767))throw new Error('Invalid axes');const v=new DataView(r.buffer);state.axes.forEach((n,i)=>v.setInt16(12+2*i,n,true));}
  reports.push([p,r]);
 }
 const refresh=()=>reports.forEach(([p,r])=>send(p,r));
 refresh();const interval=setInterval(refresh,100);
 try{
  if(c.capture){
   await sleep(Math.min(ms/2,500));
   const response=await fetch(`${origin}/api/v1/video/frame.jpg`,{signal:AbortSignal.timeout(3000)});
   if(!response.ok||!response.headers.get('content-type')?.startsWith('image/jpeg'))throw new Error('No JPEG during hold');
   const file=`${String(serial++).padStart(3,'0')}-${c.capture.replace(/[^a-zA-Z0-9_-]/g,'_')}.jpg`;
   await writeFile(`${out}/${file}`,Buffer.from(await response.arrayBuffer()));console.log('FRAME',`${out}/${file}`);
   await sleep(Math.max(0,ms-Math.min(ms/2,500)));
  }else await sleep(ms);
 }finally{clearInterval(interval);reports.forEach(([p])=>send(p,neutral()));}
 await appendFile(`${out}/commands.jsonl`,JSON.stringify({at:new Date().toISOString(),...c})+'\n');
 console.log('RELEASED',JSON.stringify(c));await sleep(c.gap??180);
}
readline.createInterface({input:process.stdin}).on('line',line=>{queue=queue.then(()=>command(line)).catch(e=>console.log('ERROR',e.message));});
setTimeout(()=>{if(ws.readyState===WebSocket.OPEN)ws.send(new Uint8Array([5]));ws.close();},1200000);
