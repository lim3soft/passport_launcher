const fs=require('node:fs'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const tool=require('../main/bin_import.js');
const hash=(name,x)=>new Uint8Array(crypto.createHash(name).update(x).digest());
const eq=(a,b)=>assert.deepEqual(Buffer.from(a),Buffer.from(b));
function image({sdk=false,large=0,chip=5,idf='v5.5.3',version='1.0'}={}) {
  const h=Buffer.alloc(24);h.set([0xe9,1,2,0x3f]);h.writeUInt16LE(chip,12);h[23]=1;
  const payload=Buffer.alloc(256+large+(sdk?64:0));payload.writeUInt32LE(0xabcd5432);
  payload.write(version,16);payload.write('CommunityTest',48);payload.write(idf,112);
  if(sdk)payload.write('PASSPORTOS_ABI2:org.test.app\0',256);
  const segment=Buffer.alloc(8);segment.writeUInt32LE(0x3c000020);segment.writeUInt32LE(payload.length,4);
  const body=Buffer.concat([h,segment,payload]);
  const padded=Buffer.alloc(Math.floor((body.length+16)/16)*16);body.copy(padded);
  let sum=0xef;for(const x of payload)sum^=x;padded[padded.length-1]=sum;
  return Buffer.concat([padded,hash('sha256',padded)]);
}
function merged(app) {
  const raw=Buffer.alloc(0x10000+app.length,255);raw[0]=0xe9;raw.writeUInt16LE(5,12);
  const rows=[['nvs',1,2,0x9000,0x6000],['phy_init',1,1,0xf000,0x1000],['factory',0,0,0x10000,0x7f0000]];
  rows.forEach(([name,type,sub,off,size],i)=>{
    const o=0x8000+i*32;raw.fill(0,o,o+32);raw.writeUInt16LE(0x50aa,o);raw[o+2]=type;raw[o+3]=sub;
    raw.writeUInt32LE(off,o+4);raw.writeUInt32LE(size,o+8);raw.write(name,o+12);
  });
  raw[0x8060]=raw[0x8061]=0xeb;raw.set(hash('md5',raw.subarray(0x8000,0x8060)),0x8070);raw.set(app,0x10000);return raw;
}
(async()=>{
  for(const n of [0,1,3,55,56,63,64,65,1000,100000]) {
    const bytes=crypto.randomBytes(n);eq(tool.sha256(bytes),hash('sha256',bytes));eq(tool.md5(bytes),hash('md5',bytes));
  }
  const app=image(),full=merged(app);
  const info=tool.inspect(full,'Radio Tools.bin');
  assert.equal(info.merged,true);assert.equal(info.abi,0);assert.equal(info.id,'community.radio-tools');eq(info.image,app);
  const blob=tool.pack(info,'Radio Tools',info.id),transport=new Uint8Array(await blob.arrayBuffer());
  assert.equal(transport.length,512+app.length);assert.equal(new DataView(transport.buffer).getUint32(12,true),0);
  eq(transport.subarray(148,180),hash('sha256',app));eq(transport.subarray(512),app);
  const native=tool.inspect(image({sdk:true}),'Native.bin');assert.equal(native.abi,2);assert.equal(native.id,'org.test.app');
  assert.equal(tool.inspect(image({idf:'v5.5'}),'Radio.bin').abi,0);
  assert.equal(tool.inspect(image({idf:'v5.5.2'}),'Radio.bin').abi,0);
  assert.equal(tool.inspect(image(),'QSO-Vision_latest2.3.3.bin').id,'community.qso-vision');
  assert.throws(()=>tool.inspect(image({sdk:true,idf:'v5.5'}),'Native.bin'),/SDK apps require/);
  assert.throws(()=>tool.pack(native,'Native','community.wrong'),/native SDK/);
  assert.throws(()=>tool.pack(info,'Radio','../app'),/App ID/);
  assert.throws(()=>tool.pack(info,'名字',info.id),/ASCII/);
  assert.ok(tool.inspect(image({large:0x280000}),'Large.bin').image.length>0x200000);
  const padded=Buffer.alloc(0x800000,255);full.copy(padded);eq(tool.inspect(padded,'Full.bin').image,app);
  for(const offset of [0x8000,0x8002,0x8004,0x8008,0x800c,0x801c,0x8070,0x8080,0x9000,0xf000,0x10000,0x10001,0x10002,0x10003,0x1000c,0x10020,0x10120,full.length-1]) {
    const changed=Buffer.from(full);changed[offset]^=1;
    assert.throws(()=>tool.inspect(changed,'Bad.bin'),undefined,'offset '+offset.toString(16));
  }
  for(const raw of [full.subarray(0,full.length-1),Buffer.concat([app,Buffer.from([0])]),image({chip:9}),
    image({idf:'v5.4.0'}),image({large:0x640000}),Buffer.alloc(0x800001),Buffer.alloc(20)])
    assert.throws(()=>tool.inspect(raw,'Bad.bin'));
  // Exercise the importer on audited real community firmware, not only fixtures.
  const real='/root/qso-ai-passport-publish/firmware/2.3.3/QSO-Vision.bin';
  if(fs.existsSync(real)) {
    const qso=tool.inspect(fs.readFileSync(real),'QSO-Vision.bin');
    assert.equal(qso.merged,true);assert.equal(qso.abi,0);assert.equal(qso.version,'2.3.3');
    console.log('Real QSO Vision 2.3.3 community merged BIN: extracted '+qso.image.length+' application bytes; integrity PASS (not executed).');
  }
  console.log('Browser BIN import: SHA/MD5 reference vectors, raw/merged/padded/native, transport, corruption, partition dependencies and size limits: PASS');
})().catch(e=>{console.error(e);process.exitCode=1;});
