/* Build actual browser transports. Owner binaries remain outside the repository. */
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const tool=require('../main/bin_import.js');
const out=process.argv[2],source=process.argv[3];fs.mkdirSync(out,{recursive:true});
const hash=(a)=>crypto.createHash('sha256').update(a).digest();
function image(){const b=Buffer.alloc(320);b.set([233,1,2,63]);b[12]=5;b[23]=1;b.writeUInt32LE(0x3c000020,24);b.writeUInt32LE(256,28);b.writeUInt32LE(0xabcd5432,32);b.write('1',48);b.write('Layout test',80);b.write('v5.5.3',144);let sum=239;for(const x of b.subarray(32,288))sum^=x;b[303]=sum;return Buffer.concat([b.subarray(0,304),hash(b.subarray(0,304))]);}
(async()=>{
 const app=image();
 const layout=new Uint8Array(332),v=new DataView(layout.buffer);
 // Three NVS pages keep exhaustive fault checks small while retaining valid bounds.
 const small=new Uint8Array(0x3000);small.fill(255);small[333]=0x49;
 v.setUint32(0,0x31444c50,true);v.setUint32(4,1,true);v.setUint32(8,small.length,true);layout.set(tool.sha256(small),16);
 v.setUint16(48,0x50aa,true);layout[50]=1;layout[51]=2;v.setUint32(52,0x21000,true);v.setUint32(56,small.length,true);layout.set(Buffer.from('nvs'),60);
 const info={image:app,layout,resources:small,version:'1',abi:0};
 fs.writeFileSync(path.join(out,'custom-synthetic.transport'),Buffer.from(await tool.pack(info,'Custom test','community.custom-test').arrayBuffer()));
 const results=[];
 if(source&&fs.existsSync(source))for(const name of fs.readdirSync(source).filter(n=>/\.bin$/i.test(n))){
  const raw=fs.readFileSync(path.join(source,name)),i=tool.inspect(raw,name),key='custom-'+results.length;
  const transport=Buffer.from(await tool.pack(i,i.name,i.id).arrayBuffer());fs.writeFileSync(path.join(out,key+'.transport'),transport);
  const entries=[];if(i.layout)for(let n=0;n<new DataView(i.layout.buffer).getUint32(4,true);n++){const o=48+n*32,e=i.layout.subarray(o,o+32);entries.push({name:Buffer.from(e.subarray(12,28)).toString().split('\0')[0],type:e[2],subtype:e[3],privateOffset:new DataView(e.buffer,e.byteOffset,32).getUint32(4,true),bytes:new DataView(e.buffer,e.byteOffset,32).getUint32(8,true)});}
  const r={file:name,fixture:key+'.transport',sha256:hash(raw).toString('hex'),application_bytes:i.image.length,private_bytes:i.privateBytes,minimum_allocated_bytes:Math.ceil(i.image.length/4096)*4096+i.privateBytes,idf:i.idf,partitions:entries,host_import:'PASS',physical_execution:'NOT RUN'};results.push(r);console.log(JSON.stringify(r));
 }
 fs.writeFileSync(path.join(out,'custom-input-results.json'),JSON.stringify(results,null,2)+'\n');
})().catch(e=>{console.error(e);process.exitCode=1;});
